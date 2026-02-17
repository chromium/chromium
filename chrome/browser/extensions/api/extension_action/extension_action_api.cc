// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/browser_window_util.h"
#include "chrome/browser/extensions/extension_action_dispatcher.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_popup_types.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/common/color_parser.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/prefs_helper.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/icon_util.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/image_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/base_window.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#endif  // !BUILDFLAG(IS_ANDROID)

using content::WebContents;
using extensions::browser_window_util::GetLastActiveBrowserWithProfile;

namespace extensions {

namespace {

// Errors.
const char kNoExtensionActionError[] =
    "This extension has no action specified.";
const char kNoTabError[] = "No tab with id: *.";
constexpr char kOpenPopupError[] =
    "Failed to show popup either because there is an existing popup or another "
    "error occurred.";
constexpr char kFailedToOpenPopupGenericError[] = "Failed to open popup.";
constexpr char kNoActiveWindowFound[] =
    "Could not find an active browser window.";
constexpr char kNoActivePopup[] =
    "Extension does not have a popup on the active tab.";
constexpr char kOpenPopupInactiveWindow[] =
    "Cannot show popup for an inactive window. To show the popup for this "
    "window, first call `chrome.windows.update` with `focused` set to "
    "true.";

bool g_report_error_for_invisible_icon = false;

// Returns true if the color values provided could be parsed into a color
// object out param.
bool ParseColor(const base::Value& color_value, SkColor& color) {
  if (color_value.is_string()) {
    return content::ParseCssColorString(color_value.GetString(), &color);
  }

  if (!color_value.is_list()) {
    return false;
  }

  const base::ListValue& color_list = color_value.GetList();
  if (color_list.size() != 4 ||
      std::ranges::any_of(color_list,
                          [](const auto& color) { return !color.is_int(); })) {
    return false;
  }

  color = SkColorSetARGB(color_list[3].GetInt(), color_list[0].GetInt(),
                         color_list[1].GetInt(), color_list[2].GetInt());
  return true;
}

// Returns the browser that is active in the given `profile`, optionally
// also checking the incognito profile.
BrowserWindowInterface* FindActiveBrowserWindow(Profile& profile,
                                                bool check_incognito_profile) {
  BrowserWindowInterface* browser = GetLastActiveBrowserWithProfile(
      profile,
      /*include_incognito_or_parent=*/check_incognito_profile);
  return browser && browser->GetWindow()->IsActive() ? browser : nullptr;
}

// Returns true if the given `extension` has an active popup on the active tab
// of `browser`.
bool HasPopupOnActiveTab(BrowserWindowInterface& browser,
                         content::BrowserContext* browser_context,
                         const Extension& extension) {
  content::WebContents* web_contents =
      TabListInterface::From(&browser)->GetActiveTab()->GetContents();
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
bool OpenPopupInBrowser(BrowserWindowInterface& browser,
                        const Extension& extension,
                        std::string* error,
                        ShowPopupCallback callback) {
#if !BUILDFLAG(IS_ANDROID)
  // On Android, the extension toolbar exists if and only if ExtensionsContainer
  // exists, so the check below is sufficient.
  // On other platforms, ExtensionsContainer is always constructed except for
  // guest sessions, so we need more detailed checks.
  Browser& browser_legacy = *browser.GetBrowserForMigrationOnly();
  if (!browser_legacy.SupportsWindowFeature(
          Browser::WindowFeature::kFeatureToolbar) ||
      !browser_legacy.window()->IsToolbarVisible()) {
    *error = "Browser window has no toolbar.";
    return false;
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  ExtensionsContainer* extensions_container =
      ExtensionsContainer::From(browser);
  // The ExtensionsContainer could be null if, e.g., this is a guest session.
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
// ExtensionActionFunction
//

ExtensionActionFunction::ExtensionActionFunction()
    : details_(nullptr),
      tab_id_(ExtensionAction::kDefaultTabId),
      contents_(nullptr),
      extension_action_(nullptr) {}

ExtensionActionFunction::~ExtensionActionFunction() = default;

ExtensionFunction::ResponseAction ExtensionActionFunction::Run() {
  ExtensionActionManager* manager =
      ExtensionActionManager::Get(browser_context());
  extension_action_ = manager->GetExtensionAction(*extension());
  if (!extension_action_) {
    // TODO(kalman): Ideally the browserAction/pageAction APIs wouldn't even
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
  if (args().empty()) {
    return true;
  }

  const base::Value& first_arg = args()[0];

  switch (first_arg.type()) {
    case base::Value::Type::INTEGER:
      tab_id_ = first_arg.GetInt();
      break;

    case base::Value::Type::DICT: {
      // Found the details argument.
      details_ = &first_arg.GetDict();
      // Still need to check for the tabId within details.
      if (const base::Value* tab_id_value = details_->Find("tabId")) {
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
  ExtensionActionDispatcher::Get(browser_context())
      ->NotifyChange(extension_action_, contents_, browser_context());
}

void ExtensionActionFunction::SetVisible(bool visible) {
  if (extension_action_->GetIsVisible(tab_id_) == visible) {
    return;
  }
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
  const base::DictValue* canvas_set = details_->FindDict("imageData");
  if (canvas_set) {
    gfx::ImageSkia icon;

    extensions::IconParseResult parse_result =
        extensions::ParseIconFromCanvasDictionary(*canvas_set, &icon);
    EXTENSION_FUNCTION_VALIDATE(parse_result ==
                                extensions::IconParseResult::kSuccess);

    if (icon.isNull()) {
      return RespondNow(Error("Icon invalid."));
    }

    gfx::Image icon_image(icon);
    const SkBitmap bitmap = icon_image.AsBitmap();
    const bool is_visible = image_util::IsIconSufficientlyVisible(bitmap);
    if (!is_visible && g_report_error_for_invisible_icon) {
      return RespondNow(Error("Icon not sufficiently visible."));
    }

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
  const std::string* popup_string = details_->FindString("popup");
  EXTENSION_FUNCTION_VALIDATE(popup_string);
  GURL popup_url;

  // If an empty string is passed, remove the explicitly set popup. Setting it
  // back to an empty string (URL) will cause no popup to be shown (even if
  // one is specified in the manifest).
  if (!popup_string->empty()) {
    popup_url = extension()->ResolveExtensionURL(*popup_string);
    if (!popup_url.is_valid()) {
      return RespondNow(Error(manifest_errors::kInvalidExtensionPopupPath));
    }
  }

  extension_action_->SetPopupUrl(tab_id_, popup_url);
  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionSetBadgeTextFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);

  const std::string* badge_text = details_->FindString("text");
  if (badge_text) {
    extension_action_->SetBadgeText(tab_id_, *badge_text);
  } else {
    extension_action_->ClearBadgeText(tab_id_);
  }

  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionSetBadgeBackgroundColorFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);
  const base::Value* color_value = details_->Find("color");
  EXTENSION_FUNCTION_VALIDATE(color_value);
  SkColor color = 0;
  if (!ParseColor(*color_value, color)) {
    return RespondNow(Error(extension_misc::kInvalidColorError));
  }
  extension_action_->SetBadgeBackgroundColor(tab_id_, color);
  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ActionSetBadgeTextColorFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);
  const base::Value* color_value = details_->Find("color");
  EXTENSION_FUNCTION_VALIDATE(color_value);
  SkColor color = 0;
  if (!ParseColor(*color_value, color)) {
    return RespondNow(Error(extension_misc::kInvalidColorError));
  }

  if (SkColorGetA(color) == SK_AlphaTRANSPARENT) {
    return RespondNow(Error(extension_misc::kInvalidColorError));
  }
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
  base::ListValue list;
  SkColor color = extension_action_->GetBadgeBackgroundColor(tab_id_);
  list.Append(static_cast<int>(SkColorGetR(color)));
  list.Append(static_cast<int>(SkColorGetG(color)));
  list.Append(static_cast<int>(SkColorGetB(color)));
  list.Append(static_cast<int>(SkColorGetA(color)));
  return RespondNow(WithArguments(std::move(list)));
}

ExtensionFunction::ResponseAction
ActionGetBadgeTextColorFunction::RunExtensionAction() {
  base::ListValue list;
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
  base::DictValue ui_settings;
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

  BrowserWindowInterface* browser = nullptr;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  std::string error;
  if (window_id == extension_misc::kCurrentWindowId) {
    browser =
        FindActiveBrowserWindow(*profile, include_incognito_information());
    if (!browser) {
      error = kNoActiveWindowFound;
    }
  } else {
    if (WindowController* controller =
            ExtensionTabUtil::GetControllerInProfileWithId(
                profile, window_id, include_incognito_information(), &error)) {
      browser = controller->GetBrowserWindowInterface();
    }
  }

  if (!browser) {
    DCHECK(!error.empty());
    return RespondNow(Error(std::move(error)));
  }

  if (!browser->GetWindow()->IsActive()) {
    return RespondNow(Error(kOpenPopupInactiveWindow));
  }

  if (!HasPopupOnActiveTab(*browser, browser_context(), *extension())) {
    return RespondNow(Error(kNoActivePopup));
  }

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
  BrowserWindowInterface* browser =
      FindActiveBrowserWindow(*profile, include_incognito_information());

  if (!browser) {
    return RespondNow(Error(kNoActiveWindowFound));
  }

  if (!HasPopupOnActiveTab(*browser, browser_context(), *extension())) {
    return RespondNow(Error(kNoActivePopup));
  }

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
  if (did_respond()) {
    return;
  }

  DVLOG(1) << "chrome.browserAction.openPopup did not show a popup.";
  Respond(Error(kOpenPopupError));
}

void BrowserActionOpenPopupFunction::OnExtensionHostCompletedFirstLoad(
    content::BrowserContext* browser_context,
    ExtensionHost* host) {
  if (did_respond()) {
    return;
  }

  if (host->extension_host_type() != mojom::ViewType::kExtensionPopup ||
      host->extension()->id() != extension_->id()) {
    return;
  }

  Respond(NoArguments());
  host_registry_observation_.Reset();
}

}  // namespace extensions
