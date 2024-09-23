// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/common/color_parser.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/prefs_helper.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/image_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "url/origin.h"

using content::WebContents;

namespace extensions {

namespace {

// Errors.
const char kNoExtensionActionError[] =
    "This extension has no action specified.";
const char kNoTabError[] = "No tab with id: *.";
const char kOpenPopupError[] =
    "Failed to show popup either because there is an existing popup or another "
    "error occurred.";
const char kFailedToOpenPopupGenericError[] = "Failed to open popup.";
constexpr char kNoActiveWindowFound[] =
    "Could not find an active browser window.";
constexpr char kNoActivePopup[] =
    "Extension does not have a popup on the active tab.";
constexpr char kOpenPopupInactiveWindow[] =
    "Cannot show popup for an inactive window. To show the popup for this "
    "window, first call `chrome.windows.update` with `focused` set to "
    "true.";

bool g_report_error_for_invisible_icon = false;

// Returns the browser that was last active in the given `profile`, optionally
// also checking the incognito profile.
Browser* FindLastActiveBrowserWindow(Profile* profile,
                                     bool check_incognito_profile) {
  Browser* browser = chrome::FindLastActiveWithProfile(profile);

  if (browser && browser->window()->IsActive())
    return browser;  // Found an active browser.

  // It's possible that the last active browser actually corresponds to the
  // associated incognito profile, and this won't be returned by
  // FindLastActiveWithProfile(). If the extension can operate incognito, then
  // check the last active incognito, too.
  if (check_incognito_profile && profile->HasPrimaryOTRProfile()) {
    Profile* incognito_profile =
        profile->GetPrimaryOTRProfile(/*create_if_needed=*/false);
    DCHECK(incognito_profile);
    Browser* incognito_browser =
        chrome::FindLastActiveWithProfile(incognito_profile);
    if (incognito_browser->window()->IsActive())
      return incognito_browser;
  }

  return nullptr;
}

// Returns true if the color values provided could be parsed into a color
// object out param.
bool ParseColor(const base::Value& color_value, SkColor& color) {
  if (color_value.is_string())
    return content::ParseCssColorString(color_value.GetString(), &color);

  if (!color_value.is_list())
    return false;

  const base::Value::List& color_list = color_value.GetList();
  if (color_list.size() != 4 ||
      base::ranges::any_of(color_list,
                           [](const auto& color) { return !color.is_int(); })) {
    return false;
  }

  color = SkColorSetARGB(color_list[3].GetInt(), color_list[0].GetInt(),
                         color_list[1].GetInt(), color_list[2].GetInt());
  return true;
}

// Returns true if the given `extension` has an active popup on the active tab
// of `browser`.
bool HasPopupOnActiveTab(Browser* browser,
                         content::BrowserContext* browser_context,
                         const Extension& extension) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ExtensionAction* extension_action =
      ExtensionActionManager::Get(browser_context)
          ->GetExtensionAction(extension);
  DCHECK(extension_action);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  return extension_action->HasPopup(tab_id) &&
         extension_action->GetIsVisibleIgnoringDeclarative(tab_id);
}

// Attempts to open `extension`'s popup in the given `browser`. Returns true on
// success; otherwise, populates `error` and returns false.
bool OpenPopupInBrowser(Browser& browser,
                        const Extension& extension,
                        std::string* error,
                        ShowPopupCallback callback) {
  if (!browser.SupportsWindowFeature(Browser::FEATURE_TOOLBAR) ||
      !browser.window()->IsToolbarVisible()) {
    *error = "Browser window has no toolbar.";
    return false;
  }

  ExtensionsContainer* extensions_container =
      browser.window()->GetExtensionsContainer();
  // The ExtensionsContainer could be null if, e.g., this is a popup window with
  // no toolbar.
  // TODO(devlin): Is that still possible, given the checks above?
  if (!extensions_container ||
      !extensions_container->ShowToolbarActionPopupForAPICall(
          extension.id(), std::move(callback))) {
    *error = kFailedToOpenPopupGenericError;
    return false;
  }

  return true;
}

}  // namespace

//
// ExtensionActionAPI::Observer
//

void ExtensionActionAPI::Observer::OnExtensionActionUpdated(
    ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
}

void ExtensionActionAPI::Observer::OnExtensionActionAPIShuttingDown() {
}

ExtensionActionAPI::Observer::~Observer() {
}

//
// ExtensionActionAPI
//

static base::LazyInstance<BrowserContextKeyedAPIFactory<ExtensionActionAPI>>::
    DestructorAtExit g_extension_action_api_factory = LAZY_INSTANCE_INITIALIZER;

ExtensionActionAPI::ExtensionActionAPI(content::BrowserContext* context)
    : browser_context_(context), extension_prefs_(nullptr) {}

ExtensionActionAPI::~ExtensionActionAPI() {
}

// static
BrowserContextKeyedAPIFactory<ExtensionActionAPI>*
ExtensionActionAPI::GetFactoryInstance() {
  return g_extension_action_api_factory.Pointer();
}

// static
ExtensionActionAPI* ExtensionActionAPI::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<ExtensionActionAPI>::Get(context);
}

void ExtensionActionAPI::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionActionAPI::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ExtensionActionAPI::NotifyChange(ExtensionAction* extension_action,
                                      content::WebContents* web_contents,
                                      content::BrowserContext* context) {
  for (auto& observer : observers_)
    observer.OnExtensionActionUpdated(extension_action, web_contents, context);
}

void ExtensionActionAPI::DispatchExtensionActionClicked(
    const ExtensionAction& extension_action,
    WebContents* web_contents,
    const Extension* extension) {
  events::HistogramValue histogram_value = events::UNKNOWN;
  const char* event_name = nullptr;
  switch (extension_action.action_type()) {
    case ActionInfo::Type::kAction:
      histogram_value = events::ACTION_ON_CLICKED;
      event_name = "action.onClicked";
      break;
    case ActionInfo::Type::kBrowser:
      histogram_value = events::BROWSER_ACTION_ON_CLICKED;
      event_name = "browserAction.onClicked";
      break;
    case ActionInfo::Type::kPage:
      histogram_value = events::PAGE_ACTION_ON_CLICKED;
      event_name = "pageAction.onClicked";
      break;
  }

  if (event_name) {
    base::Value::List args;
    // The action APIs (browserAction, pageAction, action) are only available
    // to privileged extension contexts. As such, we deterministically know that
    // the right context type here is privileged.
    constexpr mojom::ContextType context_type =
        mojom::ContextType::kPrivilegedExtension;
    ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
        ExtensionTabUtil::GetScrubTabBehavior(extension, context_type,
                                              web_contents);
    args.Append(ExtensionTabUtil::CreateTabObject(web_contents,
                                                  scrub_tab_behavior, extension)
                    .ToValue());

    DispatchEventToExtension(web_contents->GetBrowserContext(),
                             extension_action.extension_id(), histogram_value,
                             event_name, std::move(args));
  }
}

void ExtensionActionAPI::ClearAllValuesForTab(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  const ExtensionSet& enabled_extensions =
      ExtensionRegistry::Get(browser_context_)->enabled_extensions();
  ExtensionActionManager* action_manager =
      ExtensionActionManager::Get(browser_context_);

  for (ExtensionSet::const_iterator iter = enabled_extensions.begin();
       iter != enabled_extensions.end(); ++iter) {
    ExtensionAction* extension_action =
        action_manager->GetExtensionAction(**iter);
    if (extension_action) {
      extension_action->ClearAllValuesForTab(tab_id.id());
      NotifyChange(extension_action, web_contents, browser_context);
    }
  }
}

ExtensionPrefs* ExtensionActionAPI::GetExtensionPrefs() {
  // This lazy initialization is more than just an optimization, because it
  // allows tests to associate a new ExtensionPrefs with the browser context
  // before we access it.
  if (!extension_prefs_)
    extension_prefs_ = ExtensionPrefs::Get(browser_context_);
  return extension_prefs_;
}

void ExtensionActionAPI::DispatchEventToExtension(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    base::Value::List event_args) {
  if (!EventRouter::Get(context))
    return;

  auto event = std::make_unique<Event>(histogram_value, event_name,
                                       std::move(event_args), context);
  event->user_gesture = EventRouter::USER_GESTURE_ENABLED;
  EventRouter::Get(context)
      ->DispatchEventToExtension(extension_id, std::move(event));
}

void ExtensionActionAPI::Shutdown() {
  for (auto& observer : observers_)
    observer.OnExtensionActionAPIShuttingDown();
}

void ExtensionActionAPI::OnActionPinnedStateChanged(
    const ExtensionId& extension_id,
    bool is_pinned) {
  // TODO(crbug.com/360916928): Today, no action APIs are compiled.
  // Unfortunately, this means we miss out on the compiled types, which would be
  // rather helpful here.
  base::Value::List args;
  base::Value::Dict change;
  change.Set("isOnToolbar", is_pinned);
  args.Append(std::move(change));
  DispatchEventToExtension(browser_context_, extension_id,
                           events::ACTION_ON_USER_SETTINGS_CHANGED,
                           "action.onUserSettingsChanged", std::move(args));
}

//
// ExtensionActionFunction
//

ExtensionActionFunction::ExtensionActionFunction()
    : details_(nullptr),
      tab_id_(ExtensionAction::kDefaultTabId),
      contents_(nullptr),
      extension_action_(nullptr) {}

ExtensionActionFunction::~ExtensionActionFunction() {
}

ExtensionFunction::ResponseAction ExtensionActionFunction::Run() {
  ExtensionActionManager* manager =
      ExtensionActionManager::Get(browser_context());
  extension_action_ = manager->GetExtensionAction(*extension());
  if (!extension_action_) {
    // TODO(kalman): ideally the browserAction/pageAction APIs wouldn't event
    // exist for extensions that don't have one declared. This should come as
    // part of the Feature system.
    return RespondNow(Error(kNoExtensionActionError));
  }

  // Populates the tab_id_ and details_ members.
  EXTENSION_FUNCTION_VALIDATE(ExtractDataFromArguments());

  // Find the WebContents that contains this tab id if one is required.
  if (tab_id_ != ExtensionAction::kDefaultTabId) {
    content::WebContents* contents_out_param = nullptr;
    ExtensionTabUtil::GetTabById(tab_id_, browser_context(),
                                 include_incognito_information(),
                                 &contents_out_param);
    if (!contents_out_param) {
      return RespondNow(Error(kNoTabError, base::NumberToString(tab_id_)));
    }
    contents_ = contents_out_param;
  } else {
    // Page actions do not have a default tabId.
    EXTENSION_FUNCTION_VALIDATE(extension_action_->action_type() !=
                                ActionInfo::Type::kPage);
  }
  return RunExtensionAction();
}

bool ExtensionActionFunction::ExtractDataFromArguments() {
  // There may or may not be details (depends on the function).
  // The tabId might appear in details (if it exists), as the first
  // argument besides the action type (depends on the function), or be omitted
  // entirely.
  if (args().empty())
    return true;

  base::Value& first_arg = mutable_args()[0];

  switch (first_arg.type()) {
    case base::Value::Type::INTEGER:
      tab_id_ = first_arg.GetInt();
      break;

    case base::Value::Type::DICT: {
      // Found the details argument.
      details_ = &first_arg.GetDict();
      // Still need to check for the tabId within details.
      if (base::Value* tab_id_value = details_->Find("tabId")) {
        switch (tab_id_value->type()) {
          case base::Value::Type::NONE:
            // OK; tabId is optional, leave it default.
            return true;
          case base::Value::Type::INTEGER:
            tab_id_ = tab_id_value->GetInt();
            return true;
          default:
            // Boom.
            return false;
        }
      }
      // Not found; tabId is optional, leave it default.
      break;
    }

    case base::Value::Type::NONE:
      // The tabId might be an optional argument.
      break;

    default:
      return false;
  }

  return true;
}

void ExtensionActionFunction::NotifyChange() {
  ExtensionActionAPI::Get(browser_context())
      ->NotifyChange(extension_action_, contents_, browser_context());
}

void ExtensionActionFunction::SetVisible(bool visible) {
  if (extension_action_->GetIsVisible(tab_id_) == visible)
    return;
  extension_action_->SetIsVisible(tab_id_, visible);
  NotifyChange();
}

ExtensionFunction::ResponseAction
ExtensionActionShowFunction::RunExtensionAction() {
  SetVisible(true);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionHideFunction::RunExtensionAction() {
  SetVisible(false);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ActionIsEnabledFunction::RunExtensionAction() {
  return RespondNow(WithArguments(
      extension_action_->GetIsVisibleIgnoringDeclarative(tab_id_)));
}

// static
void ExtensionActionSetIconFunction::SetReportErrorForInvisibleIconForTesting(
    bool value) {
  g_report_error_for_invisible_icon = value;
}

ExtensionFunction::ResponseAction
ExtensionActionSetIconFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);

  // setIcon can take a variant argument: either a dictionary of canvas
  // ImageData, or an icon index.
  base::Value::Dict* canvas_set = details_->FindDict("imageData");
  if (canvas_set) {
    gfx::ImageSkia icon;

    ExtensionAction::IconParseResult parse_result =
        ExtensionAction::ParseIconFromCanvasDictionary(*canvas_set, &icon);
    EXTENSION_FUNCTION_VALIDATE(parse_result ==
                                ExtensionAction::IconParseResult::kSuccess);

    if (icon.isNull())
      return RespondNow(Error("Icon invalid."));

    gfx::Image icon_image(icon);
    const SkBitmap bitmap = icon_image.AsBitmap();
    const bool is_visible = image_util::IsIconSufficientlyVisible(bitmap);
    if (!is_visible && g_report_error_for_invisible_icon)
      return RespondNow(Error("Icon not sufficiently visible."));

    extension_action_->SetIcon(tab_id_, icon_image);
  } else if (details_->FindInt("iconIndex")) {
    // Obsolete argument: ignore it.
    return RespondNow(NoArguments());
  } else {
    EXTENSION_FUNCTION_VALIDATE(false);
  }

  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionSetTitleFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);
  const std::string* title = details_->FindString("title");
  EXTENSION_FUNCTION_VALIDATE(title);
  extension_action_->SetTitle(tab_id_, *title);
  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionSetPopupFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);
  std::string* popup_string = details_->FindString("popup");
  EXTENSION_FUNCTION_VALIDATE(popup_string);
  GURL popup_url;

  // If an empty string is passed, remove the explicitly set popup. Setting it
  // back to an empty string (URL) will cause it to fall back to the default set
  // in the manifest.
  if (!popup_string->empty()) {
    popup_url = extension()->GetResourceURL(*popup_string);
    // Validate popup is same-origin (only for this extension). We do not
    // validate the file exists (like we do in manifest validation) because an
    // extension could potentially intercept the request with a service worker
    // and dynamically provide content.
    if (!extension()->origin().IsSameOriginWith(popup_url)) {
      return RespondNow(Error(manifest_errors::kInvalidExtensionOriginPopup));
    }
  }

  extension_action_->SetPopupUrl(tab_id_, popup_url);
  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionSetBadgeTextFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);

  std::string* badge_text = details_->FindString("text");
  if (badge_text)
    extension_action_->SetBadgeText(tab_id_, *badge_text);
  else
    extension_action_->ClearBadgeText(tab_id_);

  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionSetBadgeBackgroundColorFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);
  base::Value* color_value = details_->Find("color");
  EXTENSION_FUNCTION_VALIDATE(color_value);
  SkColor color = 0;
  if (!ParseColor(*color_value, color))
    return RespondNow(Error(extension_misc::kInvalidColorError));
  extension_action_->SetBadgeBackgroundColor(tab_id_, color);
  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ActionSetBadgeTextColorFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);
  base::Value* color_value = details_->Find("color");
  EXTENSION_FUNCTION_VALIDATE(color_value);
  SkColor color = 0;
  if (!ParseColor(*color_value, color))
    return RespondNow(Error(extension_misc::kInvalidColorError));

  if (SkColorGetA(color) == SK_AlphaTRANSPARENT)
    return RespondNow(Error(extension_misc::kInvalidColorError));
  extension_action_->SetBadgeTextColor(tab_id_, color);
  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionGetTitleFunction::RunExtensionAction() {
  return RespondNow(WithArguments(extension_action_->GetTitle(tab_id_)));
}

ExtensionFunction::ResponseAction
ExtensionActionGetPopupFunction::RunExtensionAction() {
  return RespondNow(
      WithArguments(extension_action_->GetPopupUrl(tab_id_).spec()));
}

ExtensionFunction::ResponseAction
ExtensionActionGetBadgeTextFunction::RunExtensionAction() {
  declarative_net_request::PrefsHelper helper(
      *ExtensionPrefs::Get(browser_context()));
  bool is_dnr_action_count_active =
      helper.GetUseActionCountAsBadgeText(extension_id()) &&
      !extension_action_->HasBadgeText(tab_id_);

  // Ensure that the placeholder string is returned if this extension is
  // displaying action counts for the badge labels and the extension doesn't
  // have permission to view the action count for this tab. Note that
  // tab-specific badge text takes priority over the action count.
  if (is_dnr_action_count_active &&
      !declarative_net_request::HasDNRFeedbackPermission(extension(),
                                                         tab_id_)) {
    return RespondNow(WithArguments(
        std::move(declarative_net_request::kActionCountPlaceholderBadgeText)));
  }

  return RespondNow(
      WithArguments(extension_action_->GetDisplayBadgeText(tab_id_)));
}

ExtensionFunction::ResponseAction
ExtensionActionGetBadgeBackgroundColorFunction::RunExtensionAction() {
  base::Value::List list;
  SkColor color = extension_action_->GetBadgeBackgroundColor(tab_id_);
  list.Append(static_cast<int>(SkColorGetR(color)));
  list.Append(static_cast<int>(SkColorGetG(color)));
  list.Append(static_cast<int>(SkColorGetB(color)));
  list.Append(static_cast<int>(SkColorGetA(color)));
  return RespondNow(WithArguments(std::move(list)));
}

ExtensionFunction::ResponseAction
ActionGetBadgeTextColorFunction::RunExtensionAction() {
  base::Value::List list;
  SkColor color = extension_action_->GetBadgeTextColor(tab_id_);
  list.Append(static_cast<int>(SkColorGetR(color)));
  list.Append(static_cast<int>(SkColorGetG(color)));
  list.Append(static_cast<int>(SkColorGetB(color)));
  list.Append(static_cast<int>(SkColorGetA(color)));
  return RespondNow(WithArguments(std::move(list)));
}

ActionGetUserSettingsFunction::ActionGetUserSettingsFunction() = default;
ActionGetUserSettingsFunction::~ActionGetUserSettingsFunction() = default;

ExtensionFunction::ResponseAction ActionGetUserSettingsFunction::Run() {
  DCHECK(extension());
  ExtensionActionManager* const action_manager =
      ExtensionActionManager::Get(browser_context());
  ExtensionAction* const action =
      action_manager->GetExtensionAction(*extension());

  // This API is only available to extensions with the "action" key in the
  // manifest, so they should always have an action.
  DCHECK(action);
  DCHECK_EQ(ActionInfo::Type::kAction, action->action_type());

  const bool is_pinned =
      ToolbarActionsModel::Get(Profile::FromBrowserContext(browser_context()))
          ->IsActionPinned(extension_id());

  // TODO(crbug.com/360916928): Today, no action APIs are compiled.
  // Unfortunately, this means we miss out on the compiled types, which would be
  // rather helpful here.
  base::Value::Dict ui_settings;
  ui_settings.Set("isOnToolbar", is_pinned);

  return RespondNow(WithArguments(std::move(ui_settings)));
}

ActionOpenPopupFunction::ActionOpenPopupFunction() = default;
ActionOpenPopupFunction::~ActionOpenPopupFunction() = default;

ExtensionFunction::ResponseAction ActionOpenPopupFunction::Run() {
  // TODO(crbug.com/360916928): Unfortunately, the action API types aren't
  // compiled. However, the bindings should still valid the form of the
  // arguments.
  EXTENSION_FUNCTION_VALIDATE(args().size() == 1u);
  EXTENSION_FUNCTION_VALIDATE(extension());
  const base::Value& options = args()[0];

  // TODO(crbug.com/40057101): Support specifying the tab ID? This is
  // kind of racy (because really what the extension probably cares about is
  // the document ID; tab ID persists across pages, whereas document ID would
  // detect things like navigations).
  int window_id = extension_misc::kCurrentWindowId;
  if (options.is_dict()) {
    const base::Value* window_value = options.GetDict().Find("windowId");
    if (window_value) {
      EXTENSION_FUNCTION_VALIDATE(window_value->is_int());
      window_id = window_value->GetInt();
    }
  }

  Browser* browser = nullptr;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  std::string error;
  if (window_id == extension_misc::kCurrentWindowId) {
    browser =
        FindLastActiveBrowserWindow(profile, include_incognito_information());
    if (!browser)
      error = kNoActiveWindowFound;
  } else {
    if (WindowController* controller =
            ExtensionTabUtil::GetControllerInProfileWithId(
                profile, window_id, include_incognito_information(), &error)) {
      browser = controller->GetBrowser();
    }
  }

  if (!browser) {
    DCHECK(!error.empty());
    return RespondNow(Error(std::move(error)));
  }

  if (!browser->window()->IsActive()) {
    return RespondNow(Error(kOpenPopupInactiveWindow));
  }

  if (!HasPopupOnActiveTab(browser, browser_context(), *extension()))
    return RespondNow(Error(kNoActivePopup));

  if (!OpenPopupInBrowser(
          *browser, *extension(), &error,
          base::BindOnce(&ActionOpenPopupFunction::OnShowPopupComplete,
                         this))) {
    DCHECK(!error.empty());
    return RespondNow(Error(std::move(error)));
  }

  // The function responds in OnShowPopupComplete(). Note that the function is
  // kept alive by the ref-count owned by the ShowPopupCallback.
  return RespondLater();
}

void ActionOpenPopupFunction::OnShowPopupComplete(ExtensionHost* popup_host) {
  DCHECK(!did_respond());

  if (popup_host) {
    // TODO(crbug.com/40057101): Return the tab for which the extension
    // popup was shown?
    DCHECK(popup_host->document_element_available());
    Respond(NoArguments());
  } else {
    // NOTE(devlin): We could have the callback pass more information here about
    // why the popup didn't open (e.g., another active popup vs popup closing
    // before display, as may happen if the window closes), but it's not clear
    // whether that would be significantly helpful to developers and it may
    // leak other information about the user's browser.
    Respond(Error(kFailedToOpenPopupGenericError));
  }
}

BrowserActionOpenPopupFunction::BrowserActionOpenPopupFunction() = default;
BrowserActionOpenPopupFunction::~BrowserActionOpenPopupFunction() = default;

ExtensionFunction::ResponseAction BrowserActionOpenPopupFunction::Run() {
  // We only allow the popup in the active window.
  Profile* profile = Profile::FromBrowserContext(browser_context());
  Browser* browser =
      FindLastActiveBrowserWindow(profile, include_incognito_information());

  if (!browser)
    return RespondNow(Error(kNoActiveWindowFound));

  if (!HasPopupOnActiveTab(browser, browser_context(), *extension()))
    return RespondNow(Error(kNoActivePopup));

  std::string error;
  if (!OpenPopupInBrowser(*browser, *extension(), &error,
                          ShowPopupCallback())) {
    DCHECK(!error.empty());
    return RespondNow(Error(std::move(error)));
  }

  // Even if this is for an incognito window, we want to use the normal profile.
  // If the extension is spanning, then extension hosts are created with the
  // original profile, and if it's split, then we know the api call came from
  // the right profile.
  host_registry_observation_.Observe(ExtensionHostRegistry::Get(profile));

  // Set a timeout for waiting for the notification that the popup is loaded.
  // Waiting is required so that the popup view can be retrieved by the custom
  // bindings for the response callback. It's also needed to keep this function
  // instance around until a notification is observed.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BrowserActionOpenPopupFunction::OpenPopupTimedOut, this),
      base::Seconds(10));
  return RespondLater();
}

void BrowserActionOpenPopupFunction::OnBrowserContextShutdown() {
  // No point in responding at this point (the context is gone). However, we
  // need to explicitly remove the ExtensionHostRegistry observation, since the
  // ExtensionHostRegistry's lifetime is tied to the BrowserContext. Otherwise,
  // this would cause a UAF when the observation is destructed as part of this
  // instance's destruction.
  host_registry_observation_.Reset();
}

void BrowserActionOpenPopupFunction::OpenPopupTimedOut() {
  if (did_respond())
    return;

  DVLOG(1) << "chrome.browserAction.openPopup did not show a popup.";
  Respond(Error(kOpenPopupError));
}

void BrowserActionOpenPopupFunction::OnExtensionHostCompletedFirstLoad(
    content::BrowserContext* browser_context,
    ExtensionHost* host) {
  if (did_respond())
    return;

  if (host->extension_host_type() != mojom::ViewType::kExtensionPopup ||
      host->extension()->id() != extension_->id())
    return;

  Respond(NoArguments());
  host_registry_observation_.Reset();
}

}  // namespace extensions
