// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
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
#include "content/public/browser/notification_service.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/image_util.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

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
const char kInvalidColorError[] =
    "The color specification could not be parsed.";

bool g_report_error_for_invisible_icon = false;

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

bool ExtensionActionAPI::ShowExtensionActionPopupForAPICall(
    const Extension* extension,
    Browser* browser) {
  ExtensionAction* extension_action =
      ExtensionActionManager::Get(browser_context_)->GetExtensionAction(
          *extension);
  if (!extension_action)
    return false;

  // Don't support showing action popups in a popup window.
  if (!browser->SupportsWindowFeature(Browser::FEATURE_TOOLBAR))
    return false;

  ExtensionsContainer* extensions_container =
      browser->window()->GetExtensionsContainer();
  // The ExtensionsContainer could be null if, e.g., this is a popup window with
  // no toolbar.
  return extensions_container &&
         extensions_container->ShowToolbarActionPopupForAPICall(
             extension->id());
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
  const char* event_name = NULL;
  switch (extension_action.action_type()) {
    case ActionInfo::TYPE_ACTION:
      histogram_value = events::ACTION_ON_CLICKED;
      event_name = "action.onClicked";
      break;
    case ActionInfo::TYPE_BROWSER:
      histogram_value = events::BROWSER_ACTION_ON_CLICKED;
      event_name = "browserAction.onClicked";
      break;
    case ActionInfo::TYPE_PAGE:
      histogram_value = events::PAGE_ACTION_ON_CLICKED;
      event_name = "pageAction.onClicked";
      break;
  }

  if (event_name) {
    std::unique_ptr<base::ListValue> args(new base::ListValue());
    // The action APIs (browserAction, pageAction, action) are only available
    // to blessed extension contexts. As such, we deterministically know that
    // the right context type here is blessed.
    constexpr Feature::Context context_type =
        Feature::BLESSED_EXTENSION_CONTEXT;
    ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
        ExtensionTabUtil::GetScrubTabBehavior(extension, context_type,
                                              web_contents);
    args->Append(ExtensionTabUtil::CreateTabObject(
                     web_contents, scrub_tab_behavior, extension)
                     ->ToValue());

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
    const std::string& extension_id,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args) {
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
    ExtensionTabUtil::GetTabById(tab_id_, browser_context(),
                                 include_incognito_information(), &contents_);
    if (!contents_)
      return RespondNow(Error(kNoTabError, base::NumberToString(tab_id_)));
  } else {
    // Page actions do not have a default tabId.
    EXTENSION_FUNCTION_VALIDATE(extension_action_->action_type() !=
                                ActionInfo::TYPE_PAGE);
  }
  return RunExtensionAction();
}

bool ExtensionActionFunction::ExtractDataFromArguments() {
  // There may or may not be details (depends on the function).
  // The tabId might appear in details (if it exists), as the first
  // argument besides the action type (depends on the function), or be omitted
  // entirely.
  base::Value* first_arg = NULL;
  if (!args_->Get(0, &first_arg))
    return true;

  switch (first_arg->type()) {
    case base::Value::Type::INTEGER:
      CHECK(first_arg->GetAsInteger(&tab_id_));
      break;

    case base::Value::Type::DICTIONARY: {
      // Found the details argument.
      details_ = static_cast<base::DictionaryValue*>(first_arg);
      // Still need to check for the tabId within details.
      base::Value* tab_id_value = NULL;
      if (details_->Get("tabId", &tab_id_value)) {
        switch (tab_id_value->type()) {
          case base::Value::Type::NONE:
            // OK; tabId is optional, leave it default.
            return true;
          case base::Value::Type::INTEGER:
            CHECK(tab_id_value->GetAsInteger(&tab_id_));
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

// static
void ExtensionActionSetIconFunction::SetReportErrorForInvisibleIconForTesting(
    bool value) {
  g_report_error_for_invisible_icon = value;
}

ExtensionFunction::ResponseAction
ExtensionActionSetIconFunction::RunExtensionAction() {
  // TODO(devlin): Temporary logging to track down https://crbug.com/1087948.
  // Remove this (and the redundant `if (!x) { VALIDATE(x); }`) checks after
  // the bug is fixed.
  // Don't reorder or remove values.
  enum class FailureType {
    kFailedToParseDetails = 0,
    kFailedToDecodeCanvas = 1,
    kFailedToUnpickleCanvas = 2,
    kNoImageDataOrIconIndex = 3,
    kMaxValue = kNoImageDataOrIconIndex,
  };

  auto log_set_icon_failure = [](FailureType type) {
    base::UmaHistogramEnumeration("Extensions.ActionSetIconFailureType", type);
  };

  if (!details_) {
    log_set_icon_failure(FailureType::kFailedToParseDetails);
    EXTENSION_FUNCTION_VALIDATE(details_);
  }

  // setIcon can take a variant argument: either a dictionary of canvas
  // ImageData, or an icon index.
  base::DictionaryValue* canvas_set = NULL;
  int icon_index;
  if (details_->GetDictionary("imageData", &canvas_set)) {
    gfx::ImageSkia icon;

    ExtensionAction::IconParseResult parse_result =
        ExtensionAction::ParseIconFromCanvasDictionary(*canvas_set, &icon);

    if (parse_result != ExtensionAction::IconParseResult::kSuccess) {
      switch (parse_result) {
        case ExtensionAction::IconParseResult::kDecodeFailure:
          log_set_icon_failure(FailureType::kFailedToDecodeCanvas);
          break;
        case ExtensionAction::IconParseResult::kUnpickleFailure:
          log_set_icon_failure(FailureType::kFailedToUnpickleCanvas);
          break;
        case ExtensionAction::IconParseResult::kSuccess:
          NOTREACHED();
          break;
      }
      EXTENSION_FUNCTION_VALIDATE(false);
    }

    if (icon.isNull())
      return RespondNow(Error("Icon invalid."));

    gfx::Image icon_image(icon);
    const SkBitmap bitmap = icon_image.AsBitmap();
    const bool is_visible = image_util::IsIconSufficientlyVisible(bitmap);
    UMA_HISTOGRAM_BOOLEAN("Extensions.DynamicExtensionActionIconWasVisible",
                          is_visible);
    const bool is_visible_rendered =
        extensions::ui_util::IsRenderedIconSufficientlyVisibleForBrowserContext(
            bitmap, browser_context());
    UMA_HISTOGRAM_BOOLEAN(
        "Extensions.DynamicExtensionActionIconWasVisibleRendered",
        is_visible_rendered);

    if (!is_visible && g_report_error_for_invisible_icon)
      return RespondNow(Error("Icon not sufficiently visible."));

    extension_action_->SetIcon(tab_id_, icon_image);
  } else if (details_->GetInteger("iconIndex", &icon_index)) {
    // Obsolete argument: ignore it.
    return RespondNow(NoArguments());
  } else {
    log_set_icon_failure(FailureType::kNoImageDataOrIconIndex);
    EXTENSION_FUNCTION_VALIDATE(false);
  }
  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionSetTitleFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);
  std::string title;
  EXTENSION_FUNCTION_VALIDATE(details_->GetString("title", &title));
  extension_action_->SetTitle(tab_id_, title);
  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionSetPopupFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);
  std::string popup_string;
  EXTENSION_FUNCTION_VALIDATE(details_->GetString("popup", &popup_string));

  GURL popup_url;
  if (!popup_string.empty())
    popup_url = extension()->GetResourceURL(popup_string);

  extension_action_->SetPopupUrl(tab_id_, popup_url);
  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionSetBadgeTextFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);

  std::string badge_text;
  if (details_->GetString("text", &badge_text))
    extension_action_->SetBadgeText(tab_id_, badge_text);
  else
    extension_action_->ClearBadgeText(tab_id_);

  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionSetBadgeBackgroundColorFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);
  base::Value* color_value = NULL;
  EXTENSION_FUNCTION_VALIDATE(details_->Get("color", &color_value));
  SkColor color = 0;
  if (color_value->is_list()) {
    base::ListValue* list = NULL;
    EXTENSION_FUNCTION_VALIDATE(details_->GetList("color", &list));
    EXTENSION_FUNCTION_VALIDATE(list->GetSize() == 4);

    int color_array[4] = {0};
    for (size_t i = 0; i < base::size(color_array); ++i) {
      EXTENSION_FUNCTION_VALIDATE(list->GetInteger(i, &color_array[i]));
    }

    color = SkColorSetARGB(color_array[3], color_array[0],
                           color_array[1], color_array[2]);
  } else if (color_value->is_string()) {
    std::string color_string;
    EXTENSION_FUNCTION_VALIDATE(details_->GetString("color", &color_string));
    if (!image_util::ParseCssColorString(color_string, &color))
      return RespondNow(Error(kInvalidColorError));
  }

  extension_action_->SetBadgeBackgroundColor(tab_id_, color);
  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionGetTitleFunction::RunExtensionAction() {
  return RespondNow(
      OneArgument(base::Value(extension_action_->GetTitle(tab_id_))));
}

ExtensionFunction::ResponseAction
ExtensionActionGetPopupFunction::RunExtensionAction() {
  return RespondNow(
      OneArgument(base::Value(extension_action_->GetPopupUrl(tab_id_).spec())));
}

ExtensionFunction::ResponseAction
ExtensionActionGetBadgeTextFunction::RunExtensionAction() {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());
  bool is_dnr_action_count_active =
      prefs->GetDNRUseActionCountAsBadgeText(extension_id()) &&
      !extension_action_->HasBadgeText(tab_id_);

  // Ensure that the placeholder string is returned if this extension is
  // displaying action counts for the badge labels and the extension doesn't
  // have permission to view the action count for this tab. Note that
  // tab-specific badge text takes priority over the action count.
  if (is_dnr_action_count_active &&
      !declarative_net_request::HasDNRFeedbackPermission(extension(),
                                                         tab_id_)) {
    return RespondNow(OneArgument(base::Value(
        std::move(declarative_net_request::kActionCountPlaceholderBadgeText))));
  }

  return RespondNow(OneArgument(
      base::Value(extension_action_->GetDisplayBadgeText(tab_id_))));
}

ExtensionFunction::ResponseAction
ExtensionActionGetBadgeBackgroundColorFunction::RunExtensionAction() {
  std::unique_ptr<base::ListValue> list(new base::ListValue());
  SkColor color = extension_action_->GetBadgeBackgroundColor(tab_id_);
  list->AppendInteger(static_cast<int>(SkColorGetR(color)));
  list->AppendInteger(static_cast<int>(SkColorGetG(color)));
  list->AppendInteger(static_cast<int>(SkColorGetB(color)));
  list->AppendInteger(static_cast<int>(SkColorGetA(color)));
  return RespondNow(
      OneArgument(base::Value::FromUniquePtrValue(std::move(list))));
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
  DCHECK_EQ(ActionInfo::TYPE_ACTION, action->action_type());

  const bool is_pinned =
      ToolbarActionsModel::Get(Profile::FromBrowserContext(browser_context()))
          ->IsActionPinned(extension_id());

  // TODO(devlin): Today, no action APIs are compiled. Unfortunately, this
  // means we miss out on the compiled types, which would be rather helpful
  // here.
  base::Value ui_settings(base::Value::Type::DICTIONARY);
  ui_settings.SetBoolKey("isOnToolbar", is_pinned);

  return RespondNow(OneArgument(std::move(ui_settings)));
}

BrowserActionOpenPopupFunction::BrowserActionOpenPopupFunction() = default;

ExtensionFunction::ResponseAction BrowserActionOpenPopupFunction::Run() {
  // We only allow the popup in the active window.
  Profile* profile = Profile::FromBrowserContext(browser_context());
  Browser* browser = chrome::FindLastActiveWithProfile(profile);
  // It's possible that the last active browser actually corresponds to the
  // associated incognito profile, and this won't be returned by
  // FindLastActiveWithProfile. If the browser we found isn't active and the
  // extension can operate incognito, then check the last active incognito, too.
  if ((!browser || !browser->window()->IsActive()) &&
      util::IsIncognitoEnabled(extension()->id(), profile) &&
      profile->HasPrimaryOTRProfile()) {
    browser =
        chrome::FindLastActiveWithProfile(profile->GetPrimaryOTRProfile());
  }

  // If there's no active browser, or the Toolbar isn't visible, abort.
  // Otherwise, try to open a popup in the active browser.
  // TODO(justinlin): Remove toolbar check when http://crbug.com/308645 is
  // fixed.
  if (!browser || !browser->window()->IsActive() ||
      !browser->window()->IsToolbarVisible() ||
      !ExtensionActionAPI::Get(profile)->ShowExtensionActionPopupForAPICall(
          extension_.get(), browser)) {
    return RespondNow(Error(kOpenPopupError));
  }

  // Even if this is for an incognito window, we want to use the normal profile.
  // If the extension is spanning, then extension hosts are created with the
  // original profile, and if it's split, then we know the api call came from
  // the right profile.
  registrar_.Add(this, NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD,
                 content::Source<Profile>(profile));

  // Set a timeout for waiting for the notification that the popup is loaded.
  // Waiting is required so that the popup view can be retrieved by the custom
  // bindings for the response callback. It's also needed to keep this function
  // instance around until a notification is observed.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BrowserActionOpenPopupFunction::OpenPopupTimedOut, this),
      base::TimeDelta::FromSeconds(10));
  return RespondLater();
}

void BrowserActionOpenPopupFunction::OpenPopupTimedOut() {
  if (did_respond())
    return;

  DVLOG(1) << "chrome.browserAction.openPopup did not show a popup.";
  Respond(Error(kOpenPopupError));
}

void BrowserActionOpenPopupFunction::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD, type);
  if (did_respond())
    return;

  ExtensionHost* host = content::Details<ExtensionHost>(details).ptr();
  if (host->extension_host_type() != mojom::ViewType::kExtensionPopup ||
      host->extension()->id() != extension_->id())
    return;

  Respond(NoArguments());
  registrar_.RemoveAll();
}

}  // namespace extensions
