// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_api.h"

#include <stddef.h>
#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/tabs_util.h"
#include "chrome/browser/extensions/api/tabs/windows_util.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "extensions/browser/file_reader.h"
#include "extensions/browser/script_executor.h"
#include "extensions/common/api/extension_types.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/host_id.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#include "extensions/common/message_bundle.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"
#include "net/base/escape.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/ui_base_types.h"

using content::BrowserThread;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;
using zoom::ZoomController;

namespace extensions {

namespace windows = api::windows;
namespace tabs = api::tabs;

using api::extension_types::InjectDetails;

namespace {

template <typename T>
class ApiParameterExtractor {
 public:
  explicit ApiParameterExtractor(T* params) : params_(params) {}
  ~ApiParameterExtractor() {}

  bool populate_tabs() {
    if (params_->get_info.get() && params_->get_info->populate.get())
      return *params_->get_info->populate;
    return false;
  }

  WindowController::TypeFilter type_filters() {
    if (params_->get_info.get() && params_->get_info->window_types.get())
      return WindowController::GetFilterFromWindowTypes(
          *params_->get_info->window_types);
    return WindowController::kNoWindowFilter;
  }

 private:
  T* params_;
};

bool GetBrowserFromWindowID(const ChromeExtensionFunctionDetails& details,
                            int window_id,
                            Browser** browser,
                            std::string* error) {
  Browser* result = nullptr;
  result = ExtensionTabUtil::GetBrowserFromWindowID(details, window_id, error);
  if (!result)
    return false;

  *browser = result;
  return true;
}

bool GetBrowserFromWindowID(ExtensionFunction* function,
                            int window_id,
                            Browser** browser,
                            std::string* error) {
  return GetBrowserFromWindowID(ChromeExtensionFunctionDetails(function),
                                window_id, browser, error);
}

// |error_message| can optionally be passed in and will be set with an
// appropriate message if the tab cannot be found by id.
bool GetTabById(int tab_id,
                content::BrowserContext* context,
                bool include_incognito,
                Browser** browser,
                TabStripModel** tab_strip,
                content::WebContents** contents,
                int* tab_index,
                std::string* error_message) {
  if (ExtensionTabUtil::GetTabById(tab_id, context, include_incognito, browser,
                                   tab_strip, contents, tab_index)) {
    return true;
  }

  if (error_message) {
    *error_message = ErrorUtils::FormatErrorMessage(
        tabs_constants::kTabNotFoundError, base::NumberToString(tab_id));
  }

  return false;
}

// Gets the WebContents for |tab_id| if it is specified. Otherwise get the
// WebContents for the active tab in the |function|'s current window.
// Returns nullptr and fills |error| if failed.
content::WebContents* GetTabsAPIDefaultWebContents(ExtensionFunction* function,
                                                   int tab_id,
                                                   std::string* error) {
  content::WebContents* web_contents = nullptr;
  if (tab_id != -1) {
    // We assume this call leaves web_contents unchanged if it is unsuccessful.
    GetTabById(tab_id, function->browser_context(),
               function->include_incognito_information(),
               nullptr /* ignore Browser* output */,
               nullptr /* ignore TabStripModel* output */, &web_contents,
               nullptr /* ignore int tab_index output */, error);
  } else {
    Browser* browser =
        ChromeExtensionFunctionDetails(function).GetCurrentBrowser();
    if (!browser)
      *error = tabs_constants::kNoCurrentWindowError;
    else if (!ExtensionTabUtil::GetDefaultTab(browser, &web_contents, nullptr))
      *error = tabs_constants::kNoSelectedTabError;
  }
  return web_contents;
}

// Returns true if either |boolean| is a null pointer, or if |*boolean| and
// |value| are equal. This function is used to check if a tab's parameters match
// those of the browser.
bool MatchesBool(bool* boolean, bool value) {
  return !boolean || *boolean == value;
}

template <typename T>
void AssignOptionalValue(const std::unique_ptr<T>& source,
                         std::unique_ptr<T>* destination) {
  if (source)
    *destination = std::make_unique<T>(*source);
}

ui::WindowShowState ConvertToWindowShowState(windows::WindowState state) {
  switch (state) {
    case windows::WINDOW_STATE_NORMAL:
      return ui::SHOW_STATE_NORMAL;
    case windows::WINDOW_STATE_MINIMIZED:
      return ui::SHOW_STATE_MINIMIZED;
    case windows::WINDOW_STATE_MAXIMIZED:
      return ui::SHOW_STATE_MAXIMIZED;
    case windows::WINDOW_STATE_FULLSCREEN:
    case windows::WINDOW_STATE_LOCKED_FULLSCREEN:
      return ui::SHOW_STATE_FULLSCREEN;
    case windows::WINDOW_STATE_NONE:
      return ui::SHOW_STATE_DEFAULT;
  }
  NOTREACHED();
  return ui::SHOW_STATE_DEFAULT;
}

bool IsValidStateForWindowsCreateFunction(
    const windows::Create::Params::CreateData* create_data) {
  if (!create_data)
    return true;

  bool has_bound = create_data->left || create_data->top ||
                   create_data->width || create_data->height;

  switch (create_data->state) {
    case windows::WINDOW_STATE_MINIMIZED:
      // If minimised, default focused state should be unfocused.
      return !(create_data->focused && *create_data->focused) && !has_bound;
    case windows::WINDOW_STATE_MAXIMIZED:
    case windows::WINDOW_STATE_FULLSCREEN:
    case windows::WINDOW_STATE_LOCKED_FULLSCREEN:
      // If maximised/fullscreen, default focused state should be focused.
      return !(create_data->focused && !*create_data->focused) && !has_bound;
    case windows::WINDOW_STATE_NORMAL:
    case windows::WINDOW_STATE_NONE:
      return true;
  }
  NOTREACHED();
  return true;
}

bool ExtensionHasLockedFullscreenPermission(const Extension* extension) {
  return extension->permissions_data()->HasAPIPermission(
      APIPermission::kLockWindowFullscreenPrivate);
}

std::unique_ptr<api::tabs::Tab> CreateTabObjectHelper(
    WebContents* contents,
    const Extension* extension,
    Feature::Context context,
    TabStripModel* tab_strip,
    int tab_index) {
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(extension, context, contents);
  return ExtensionTabUtil::CreateTabObject(contents, scrub_tab_behavior,
                                           extension, tab_strip, tab_index);
}

}  // namespace

void ZoomModeToZoomSettings(ZoomController::ZoomMode zoom_mode,
                            api::tabs::ZoomSettings* zoom_settings) {
  DCHECK(zoom_settings);
  switch (zoom_mode) {
    case ZoomController::ZOOM_MODE_DEFAULT:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_AUTOMATIC;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_ORIGIN;
      break;
    case ZoomController::ZOOM_MODE_ISOLATED:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_AUTOMATIC;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_TAB;
      break;
    case ZoomController::ZOOM_MODE_MANUAL:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_MANUAL;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_TAB;
      break;
    case ZoomController::ZOOM_MODE_DISABLED:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_DISABLED;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_TAB;
      break;
  }
}

// Windows ---------------------------------------------------------------------

ExtensionFunction::ResponseAction WindowsGetFunction::Run() {
  std::unique_ptr<windows::Get::Params> params(
      windows::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ApiParameterExtractor<windows::Get::Params> extractor(params.get());
  Browser* browser = nullptr;
  std::string error;
  if (!windows_util::GetBrowserFromWindowID(this, params->window_id,
                                            extractor.type_filters(), &browser,
                                            &error)) {
    return RespondNow(Error(error));
  }

  ExtensionTabUtil::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? ExtensionTabUtil::kPopulateTabs
                                : ExtensionTabUtil::kDontPopulateTabs;
  std::unique_ptr<base::DictionaryValue> windows =
      ExtensionTabUtil::CreateWindowValueForExtension(
          *browser, extension(), populate_tab_behavior, source_context_type());
  return RespondNow(OneArgument(std::move(windows)));
}

ExtensionFunction::ResponseAction WindowsGetCurrentFunction::Run() {
  std::unique_ptr<windows::GetCurrent::Params> params(
      windows::GetCurrent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ApiParameterExtractor<windows::GetCurrent::Params> extractor(params.get());
  Browser* browser = nullptr;
  std::string error;
  if (!windows_util::GetBrowserFromWindowID(
          this, extension_misc::kCurrentWindowId, extractor.type_filters(),
          &browser, &error)) {
    return RespondNow(Error(error));
  }

  ExtensionTabUtil::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? ExtensionTabUtil::kPopulateTabs
                                : ExtensionTabUtil::kDontPopulateTabs;
  std::unique_ptr<base::DictionaryValue> windows =
      ExtensionTabUtil::CreateWindowValueForExtension(
          *browser, extension(), populate_tab_behavior, source_context_type());
  return RespondNow(OneArgument(std::move(windows)));
}

ExtensionFunction::ResponseAction WindowsGetLastFocusedFunction::Run() {
  std::unique_ptr<windows::GetLastFocused::Params> params(
      windows::GetLastFocused::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ApiParameterExtractor<windows::GetLastFocused::Params> extractor(
      params.get());
  // The WindowControllerList should contain a list of application,
  // browser and devtools windows.
  Browser* browser = nullptr;
  for (auto* controller : WindowControllerList::GetInstance()->windows()) {
    if (controller->GetBrowser() &&
        windows_util::CanOperateOnWindow(this, controller,
                                         extractor.type_filters())) {
      // TODO(devlin): Doesn't this mean that we'll use the last window in the
      // list if there is no active window? That seems wrong.
      // See https://crbug.com/809822.
      browser = controller->GetBrowser();
      if (controller->window()->IsActive())
        break;  // Use focused window.
    }
  }
  if (!browser)
    return RespondNow(Error(tabs_constants::kNoLastFocusedWindowError));

  ExtensionTabUtil::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? ExtensionTabUtil::kPopulateTabs
                                : ExtensionTabUtil::kDontPopulateTabs;
  std::unique_ptr<base::DictionaryValue> windows =
      ExtensionTabUtil::CreateWindowValueForExtension(
          *browser, extension(), populate_tab_behavior, source_context_type());
  return RespondNow(OneArgument(std::move(windows)));
}

ExtensionFunction::ResponseAction WindowsGetAllFunction::Run() {
  std::unique_ptr<windows::GetAll::Params> params(
      windows::GetAll::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ApiParameterExtractor<windows::GetAll::Params> extractor(params.get());
  std::unique_ptr<base::ListValue> window_list(new base::ListValue());
  ExtensionTabUtil::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? ExtensionTabUtil::kPopulateTabs
                                : ExtensionTabUtil::kDontPopulateTabs;
  for (auto* controller : WindowControllerList::GetInstance()->windows()) {
    if (!controller->GetBrowser() ||
        !windows_util::CanOperateOnWindow(this, controller,
                                          extractor.type_filters())) {
      continue;
    }
    window_list->Append(ExtensionTabUtil::CreateWindowValueForExtension(
        *controller->GetBrowser(), extension(), populate_tab_behavior,
        source_context_type()));
  }

  return RespondNow(OneArgument(std::move(window_list)));
}

bool WindowsCreateFunction::ShouldOpenIncognitoWindow(
    const windows::Create::Params::CreateData* create_data,
    std::vector<GURL>* urls,
    std::string* error) {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  const IncognitoModePrefs::Availability incognito_availability =
      IncognitoModePrefs::GetAvailability(profile->GetPrefs());
  bool incognito = false;
  if (create_data && create_data->incognito) {
    incognito = *create_data->incognito;
    if (incognito && incognito_availability == IncognitoModePrefs::DISABLED) {
      *error = tabs_constants::kIncognitoModeIsDisabled;
      return false;
    }
    if (!incognito && incognito_availability == IncognitoModePrefs::FORCED) {
      *error = tabs_constants::kIncognitoModeIsForced;
      return false;
    }
  } else if (incognito_availability == IncognitoModePrefs::FORCED) {
    // If incognito argument is not specified explicitly, we default to
    // incognito when forced so by policy.
    incognito = true;
  }

  // Remove all URLs that are not allowed in an incognito session. Note that a
  // ChromeOS guest session is not considered incognito in this case.
  if (incognito && !profile->IsGuestSession()) {
    std::string first_url_erased;
    for (size_t i = 0; i < urls->size();) {
      if (IsURLAllowedInIncognito((*urls)[i], profile)) {
        i++;
      } else {
        if (first_url_erased.empty())
          first_url_erased = (*urls)[i].spec();
        urls->erase(urls->begin() + i);
      }
    }
    if (urls->empty() && !first_url_erased.empty()) {
      *error = ErrorUtils::FormatErrorMessage(
          tabs_constants::kURLsNotAllowedInIncognitoError, first_url_erased);
      return false;
    }
  }
  return incognito;
}

ExtensionFunction::ResponseAction WindowsCreateFunction::Run() {
  std::unique_ptr<windows::Create::Params> params(
      windows::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  std::vector<GURL> urls;
  TabStripModel* source_tab_strip = NULL;
  int tab_index = -1;

  windows::Create::Params::CreateData* create_data = params->create_data.get();

  // Look for optional url.
  if (create_data && create_data->url) {
    std::vector<std::string> url_strings;
    // First, get all the URLs the client wants to open.
    if (create_data->url->as_string)
      url_strings.push_back(*create_data->url->as_string);
    else if (create_data->url->as_strings)
      url_strings.swap(*create_data->url->as_strings);

    // Second, resolve, validate and convert them to GURLs.
    for (auto i = url_strings.begin(); i != url_strings.end(); ++i) {
      GURL url = ExtensionTabUtil::ResolvePossiblyRelativeURL(*i, extension());
      if (!url.is_valid())
        return RespondNow(Error(tabs_constants::kInvalidUrlError, *i));
      // Don't let the extension crash the browser or renderers.
      if (ExtensionTabUtil::IsKillURL(url))
        return RespondNow(Error(tabs_constants::kNoCrashBrowserError));
      urls.push_back(url);
    }
  }

  // Decide whether we are opening a normal window or an incognito window.
  std::string error;
  bool open_incognito_window =
      ShouldOpenIncognitoWindow(create_data, &urls, &error);
  if (!error.empty())
    return RespondNow(Error(error));

  Profile* calling_profile = Profile::FromBrowserContext(browser_context());
  Profile* window_profile = open_incognito_window
                                ? calling_profile->GetOffTheRecordProfile()
                                : calling_profile;

  // Look for optional tab id.
  if (create_data && create_data->tab_id) {
    // Find the tab. |source_tab_strip| and |tab_index| will later be used to
    // move the tab into the created window.
    Browser* source_browser = nullptr;
    if (!GetTabById(*create_data->tab_id, calling_profile,
                    include_incognito_information(), &source_browser,
                    &source_tab_strip, nullptr, &tab_index, &error)) {
      return RespondNow(Error(error));
    }

    if (!source_browser->window()->IsTabStripEditable())
      return RespondNow(Error(tabs_constants::kTabStripNotEditableError));

    if (source_browser->profile() != window_profile)
      return RespondNow(
          Error(tabs_constants::kCanOnlyMoveTabsWithinSameProfileError));
  }

  if (!IsValidStateForWindowsCreateFunction(create_data))
    return RespondNow(Error(tabs_constants::kInvalidWindowStateError));

  Browser::Type window_type = Browser::TYPE_NORMAL;

  gfx::Rect window_bounds;
  bool focused = true;
  std::string extension_id;

  if (create_data) {
    // Figure out window type before figuring out bounds so that default
    // bounds can be set according to the window type.
    switch (create_data->type) {
      // TODO(stevenjb): Remove 'panel' from windows.json.
      case windows::CREATE_TYPE_PANEL:
      case windows::CREATE_TYPE_POPUP:
        window_type = Browser::TYPE_POPUP;
        extension_id = extension()->id();
        break;
      case windows::CREATE_TYPE_NONE:
      case windows::CREATE_TYPE_NORMAL:
        break;
      default:
        return RespondNow(Error(tabs_constants::kInvalidWindowTypeError));
    }

    // Initialize default window bounds according to window type.
    ui::WindowShowState ignored_show_state = ui::SHOW_STATE_DEFAULT;
    WindowSizer::GetBrowserWindowBoundsAndShowState(std::string(), gfx::Rect(),
                                                    nullptr, &window_bounds,
                                                    &ignored_show_state);

    // Any part of the bounds can optionally be set by the caller.
    if (create_data->left)
      window_bounds.set_x(*create_data->left);

    if (create_data->top)
      window_bounds.set_y(*create_data->top);

    if (create_data->width)
      window_bounds.set_width(*create_data->width);

    if (create_data->height)
      window_bounds.set_height(*create_data->height);

    if (create_data->focused)
      focused = *create_data->focused;
  }

  // Create a new BrowserWindow.
  Browser::CreateParams create_params(window_type, window_profile,
                                      user_gesture());
  if (extension_id.empty()) {
    create_params.initial_bounds = window_bounds;
  } else {
    create_params = Browser::CreateParams::CreateForApp(
        web_app::GenerateApplicationNameFromAppId(extension_id),
        false /* trusted_source */, window_bounds, window_profile,
        user_gesture());
  }
  create_params.initial_show_state = ui::SHOW_STATE_NORMAL;
  if (create_data && create_data->state) {
    if (create_data->state == windows::WINDOW_STATE_LOCKED_FULLSCREEN &&
        !ExtensionHasLockedFullscreenPermission(extension())) {
      return RespondNow(
          Error(tabs_constants::kMissingLockWindowFullscreenPrivatePermission));
    }
    create_params.initial_show_state =
        ConvertToWindowShowState(create_data->state);
  }

  Browser* new_window = Browser::Create(create_params);
  if (!new_window)
    return RespondNow(Error(tabs_constants::kBrowserWindowNotAllowed));

  for (const GURL& url : urls) {
    NavigateParams navigate_params(new_window, url, ui::PAGE_TRANSITION_LINK);
    navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

    // Depending on the |setSelfAsOpener| option, we need to put the new
    // contents in the same BrowsingInstance as their opener.  See also
    // https://crbug.com/713888.
    bool set_self_as_opener = create_data->set_self_as_opener &&  // present?
                              *create_data->set_self_as_opener;  // set to true?
    if (set_self_as_opener) {
      if (is_from_service_worker()) {
        // TODO(crbug.com/984350): Add test for this.
        return RespondNow(
            Error("Cannot specify setSelfAsOpener Service Worker extension."));
      }
      // TODO(crbug.com/984350): Add tests for checking opener SiteInstance
      // behavior from a SW based extension's extension frame (e.g. from popup).
      // See ExtensionApiTest.WindowsCreate* tests for details.
      navigate_params.opener = render_frame_host();
      navigate_params.source_site_instance =
          render_frame_host()->GetSiteInstance();
    }

    Navigate(&navigate_params);
  }

  WebContents* contents = NULL;
  // Move the tab into the created window only if it's an empty popup or it's
  // a tabbed window.
  if (window_type == Browser::TYPE_NORMAL || urls.empty()) {
    if (source_tab_strip) {
      std::unique_ptr<content::WebContents> detached_tab =
          source_tab_strip->DetachWebContentsAt(tab_index);
      contents = detached_tab.get();
      TabStripModel* target_tab_strip = new_window->tab_strip_model();
      target_tab_strip->InsertWebContentsAt(
          urls.size(), std::move(detached_tab), TabStripModel::ADD_NONE);
    }
  }
  // Create a new tab if the created window is still empty. Don't create a new
  // tab when it is intended to create an empty popup.
  if (!contents && urls.empty() && window_type == Browser::TYPE_NORMAL) {
    chrome::NewTab(new_window);
  }
  chrome::SelectNumberedTab(new_window, 0, {TabStripModel::GestureType::kNone});

  if (focused)
    new_window->window()->Show();
  else
    new_window->window()->ShowInactive();

  // Lock the window fullscreen only after the new tab has been created
  // (otherwise the tabstrip is empty), and window()->show() has been called
  // (otherwise that resets the locked mode for devices in tablet mode).
  if (create_data &&
      create_data->state == windows::WINDOW_STATE_LOCKED_FULLSCREEN) {
    tabs_util::SetLockedFullscreenState(new_window, true);
  }

  std::unique_ptr<base::Value> result;
  if (new_window->profile()->IsOffTheRecord() &&
      !browser_context()->IsOffTheRecord() &&
      !include_incognito_information()) {
    // Don't expose incognito windows if extension itself works in non-incognito
    // profile and CanCrossIncognito isn't allowed.
    result = std::make_unique<base::Value>();
  } else {
    result = ExtensionTabUtil::CreateWindowValueForExtension(
        *new_window, extension(), ExtensionTabUtil::kPopulateTabs,
        source_context_type());
  }

  return RespondNow(OneArgument(std::move(result)));
}

ExtensionFunction::ResponseAction WindowsUpdateFunction::Run() {
  std::unique_ptr<windows::Update::Params> params(
      windows::Update::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Browser* browser = nullptr;
  std::string error;
  if (!windows_util::GetBrowserFromWindowID(
          this, params->window_id, WindowController::GetAllWindowFilter(),
          &browser, &error)) {
    return RespondNow(Error(error));
  }

  // Don't allow locked fullscreen operations on a window without the proper
  // permission (also don't allow any operations on a locked window if the
  // extension doesn't have the permission).
  const bool is_locked_fullscreen =
      platform_util::IsBrowserLockedFullscreen(browser);
  if ((params->update_info.state == windows::WINDOW_STATE_LOCKED_FULLSCREEN ||
       is_locked_fullscreen) &&
      !ExtensionHasLockedFullscreenPermission(extension())) {
    return RespondNow(
        Error(tabs_constants::kMissingLockWindowFullscreenPrivatePermission));
  }
  // state will be WINDOW_STATE_NONE if the state parameter wasn't passed from
  // the JS side, and in that case we don't want to change the locked state.
  if (is_locked_fullscreen &&
      params->update_info.state != windows::WINDOW_STATE_LOCKED_FULLSCREEN &&
      params->update_info.state != windows::WINDOW_STATE_NONE) {
    tabs_util::SetLockedFullscreenState(browser, false);
  } else if (!is_locked_fullscreen &&
             params->update_info.state ==
                 windows::WINDOW_STATE_LOCKED_FULLSCREEN) {
    tabs_util::SetLockedFullscreenState(browser, true);
  }

  ui::WindowShowState show_state =
      ConvertToWindowShowState(params->update_info.state);

  if (show_state != ui::SHOW_STATE_FULLSCREEN &&
      show_state != ui::SHOW_STATE_DEFAULT) {
    browser->extension_window_controller()->SetFullscreenMode(
        false, extension()->url());
  }

  switch (show_state) {
    case ui::SHOW_STATE_MINIMIZED:
      browser->window()->Minimize();
      break;
    case ui::SHOW_STATE_MAXIMIZED:
      browser->window()->Maximize();
      break;
    case ui::SHOW_STATE_FULLSCREEN:
      if (browser->window()->IsMinimized() ||
          browser->window()->IsMaximized()) {
        browser->window()->Restore();
      }
      browser->extension_window_controller()->SetFullscreenMode(
          true, extension()->url());
      break;
    case ui::SHOW_STATE_NORMAL:
      browser->window()->Restore();
      break;
    default:
      break;
  }

  gfx::Rect bounds;
  if (browser->window()->IsMinimized())
    bounds = browser->window()->GetRestoredBounds();
  else
    bounds = browser->window()->GetBounds();
  bool set_bounds = false;

  // Any part of the bounds can optionally be set by the caller.
  if (params->update_info.left) {
    bounds.set_x(*params->update_info.left);
    set_bounds = true;
  }

  if (params->update_info.top) {
    bounds.set_y(*params->update_info.top);
    set_bounds = true;
  }

  if (params->update_info.width) {
    bounds.set_width(*params->update_info.width);
    set_bounds = true;
  }

  if (params->update_info.height) {
    bounds.set_height(*params->update_info.height);
    set_bounds = true;
  }

  if (set_bounds) {
    if (show_state == ui::SHOW_STATE_MINIMIZED ||
        show_state == ui::SHOW_STATE_MAXIMIZED ||
        show_state == ui::SHOW_STATE_FULLSCREEN) {
      return RespondNow(Error(tabs_constants::kInvalidWindowStateError));
    }
    // TODO(varkha): Updating bounds during a drag can cause problems and a more
    // general solution is needed. See http://crbug.com/251813 .
    browser->window()->SetBounds(bounds);
  }

  if (params->update_info.focused) {
    if (*params->update_info.focused) {
      if (show_state == ui::SHOW_STATE_MINIMIZED)
        return RespondNow(Error(tabs_constants::kInvalidWindowStateError));
      browser->window()->Activate();
    } else {
      if (show_state == ui::SHOW_STATE_MAXIMIZED ||
          show_state == ui::SHOW_STATE_FULLSCREEN) {
        return RespondNow(Error(tabs_constants::kInvalidWindowStateError));
      }
      browser->window()->Deactivate();
    }
  }

  if (params->update_info.draw_attention)
    browser->window()->FlashFrame(*params->update_info.draw_attention);

  return RespondNow(OneArgument(ExtensionTabUtil::CreateWindowValueForExtension(
      *browser, extension(), ExtensionTabUtil::kDontPopulateTabs,
      source_context_type())));
}

ExtensionFunction::ResponseAction WindowsRemoveFunction::Run() {
  std::unique_ptr<windows::Remove::Params> params(
      windows::Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Browser* browser = nullptr;
  std::string error;
  if (!windows_util::GetBrowserFromWindowID(this, params->window_id,
                                            WindowController::kNoWindowFilter,
                                            &browser, &error)) {
    return RespondNow(Error(error));
  }

  if (platform_util::IsBrowserLockedFullscreen(browser) &&
      !ExtensionHasLockedFullscreenPermission(extension())) {
    return RespondNow(
        Error(tabs_constants::kMissingLockWindowFullscreenPrivatePermission));
  }

  WindowController* controller = browser->extension_window_controller();
  WindowController::Reason reason;
  if (!controller->CanClose(&reason)) {
    return RespondNow(Error(reason == WindowController::REASON_NOT_EDITABLE
                                ? tabs_constants::kTabStripNotEditableError
                                : kUnknownErrorDoNotUse));
  }
  controller->window()->Close();
  return RespondNow(NoArguments());
}

// Tabs ------------------------------------------------------------------------

ExtensionFunction::ResponseAction TabsGetSelectedFunction::Run() {
  // windowId defaults to "current" window.
  int window_id = extension_misc::kCurrentWindowId;

  std::unique_ptr<tabs::GetSelected::Params> params(
      tabs::GetSelected::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  if (params->window_id.get())
    window_id = *params->window_id;

  Browser* browser = NULL;
  std::string error;
  if (!GetBrowserFromWindowID(this, window_id, &browser, &error))
    return RespondNow(Error(error));

  TabStripModel* tab_strip = browser->tab_strip_model();
  WebContents* contents = tab_strip->GetActiveWebContents();
  if (!contents)
    return RespondNow(Error(tabs_constants::kNoSelectedTabError));
  return RespondNow(ArgumentList(tabs::Get::Results::Create(
      *CreateTabObjectHelper(contents, extension(), source_context_type(),
                             tab_strip, tab_strip->active_index()))));
}

ExtensionFunction::ResponseAction TabsGetAllInWindowFunction::Run() {
  std::unique_ptr<tabs::GetAllInWindow::Params> params(
      tabs::GetAllInWindow::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  // windowId defaults to "current" window.
  int window_id = extension_misc::kCurrentWindowId;
  if (params->window_id.get())
    window_id = *params->window_id;

  Browser* browser = NULL;
  std::string error;
  if (!GetBrowserFromWindowID(this, window_id, &browser, &error))
    return RespondNow(Error(error));

  return RespondNow(OneArgument(ExtensionTabUtil::CreateTabList(
      browser, extension(), source_context_type())));
}

ExtensionFunction::ResponseAction TabsQueryFunction::Run() {
  std::unique_ptr<tabs::Query::Params> params(
      tabs::Query::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool loading_status_set = params->query_info.status != tabs::TAB_STATUS_NONE;
  bool loading = params->query_info.status == tabs::TAB_STATUS_LOADING;

  URLPatternSet url_patterns;
  if (params->query_info.url.get()) {
    std::vector<std::string> url_pattern_strings;
    if (params->query_info.url->as_string)
      url_pattern_strings.push_back(*params->query_info.url->as_string);
    else if (params->query_info.url->as_strings)
      url_pattern_strings.swap(*params->query_info.url->as_strings);
    // It is o.k. to use URLPattern::SCHEME_ALL here because this function does
    // not grant access to the content of the tabs, only to seeing their URLs
    // and meta data.
    std::string error;
    if (!url_patterns.Populate(url_pattern_strings, URLPattern::SCHEME_ALL,
                               true, &error)) {
      return RespondNow(Error(error));
    }
  }

  std::string title;
  if (params->query_info.title.get())
    title = *params->query_info.title;

  int window_id = extension_misc::kUnknownWindowId;
  if (params->query_info.window_id.get())
    window_id = *params->query_info.window_id;

  int index = -1;
  if (params->query_info.index.get())
    index = *params->query_info.index;

  std::string window_type;
  if (params->query_info.window_type != tabs::WINDOW_TYPE_NONE)
    window_type = tabs::ToString(params->query_info.window_type);

  std::unique_ptr<base::ListValue> result(new base::ListValue());
  Profile* profile = Profile::FromBrowserContext(browser_context());
  Browser* last_active_browser =
      chrome::FindAnyBrowser(profile, include_incognito_information());
  Browser* current_browser =
      ChromeExtensionFunctionDetails(this).GetCurrentBrowser();
  for (auto* browser : *BrowserList::GetInstance()) {
    if (!profile->IsSameProfile(browser->profile()))
      continue;

    if (!browser->window())
      continue;

    if (!include_incognito_information() && profile != browser->profile())
      continue;

    if (!browser->extension_window_controller()->IsVisibleToTabsAPIForExtension(
            extension(), false /*allow_dev_tools_windows*/)) {
      continue;
    }

    if (window_id >= 0 && window_id != ExtensionTabUtil::GetWindowId(browser))
      continue;

    if (window_id == extension_misc::kCurrentWindowId &&
        browser != current_browser) {
      continue;
    }

    if (!MatchesBool(params->query_info.current_window.get(),
                     browser == current_browser)) {
      continue;
    }

    if (!MatchesBool(params->query_info.last_focused_window.get(),
                     browser == last_active_browser)) {
      continue;
    }

    if (!window_type.empty() &&
        window_type !=
            browser->extension_window_controller()->GetWindowTypeText()) {
      continue;
    }

    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int i = 0; i < tab_strip->count(); ++i) {
      WebContents* web_contents = tab_strip->GetWebContentsAt(i);

      if (index > -1 && i != index)
        continue;

      if (!web_contents) {
        continue;
      }

      if (!MatchesBool(params->query_info.highlighted.get(),
                       tab_strip->IsTabSelected(i))) {
        continue;
      }

      if (!MatchesBool(params->query_info.active.get(),
                       i == tab_strip->active_index())) {
        continue;
      }

      if (!MatchesBool(params->query_info.pinned.get(),
                       tab_strip->IsTabPinned(i))) {
        continue;
      }

      auto* audible_helper =
          RecentlyAudibleHelper::FromWebContents(web_contents);
      if (!MatchesBool(params->query_info.audible.get(),
                       audible_helper->WasRecentlyAudible())) {
        continue;
      }

      auto* tab_lifecycle_unit_external =
          resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
              web_contents);

      if (!MatchesBool(params->query_info.discarded.get(),
                       tab_lifecycle_unit_external->IsDiscarded())) {
        continue;
      }

      if (!MatchesBool(params->query_info.auto_discardable.get(),
                       tab_lifecycle_unit_external->IsAutoDiscardable())) {
        continue;
      }

      if (!MatchesBool(params->query_info.muted.get(),
                       web_contents->IsAudioMuted())) {
        continue;
      }

      if (!title.empty() || !url_patterns.is_empty()) {
        // "title" and "url" properties are considered privileged data and can
        // only be checked if the extension has the "tabs" permission or it has
        // access to the WebContents's origin. Otherwise, this tab is considered
        // not matched.
        if (!extension_->permissions_data()->HasAPIPermissionForTab(
                ExtensionTabUtil::GetTabId(web_contents),
                APIPermission::kTab) &&
            !extension_->permissions_data()->HasHostPermission(
                web_contents->GetURL())) {
          continue;
        }

        if (!title.empty() &&
            !base::MatchPattern(web_contents->GetTitle(),
                                base::UTF8ToUTF16(title))) {
          continue;
        }

        if (!url_patterns.is_empty() &&
            !url_patterns.MatchesURL(web_contents->GetURL())) {
          continue;
        }
      }

      if (loading_status_set && loading != web_contents->IsLoading())
        continue;

      result->Append(CreateTabObjectHelper(web_contents, extension(),
                                           source_context_type(), tab_strip, i)
                         ->ToValue());
    }
  }

  return RespondNow(OneArgument(std::move(result)));
}

ExtensionFunction::ResponseAction TabsCreateFunction::Run() {
  std::unique_ptr<tabs::Create::Params> params(
      tabs::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ExtensionTabUtil::OpenTabParams options;
  AssignOptionalValue(params->create_properties.window_id, &options.window_id);
  AssignOptionalValue(params->create_properties.opener_tab_id,
                      &options.opener_tab_id);
  AssignOptionalValue(params->create_properties.selected, &options.active);
  // The 'active' property has replaced the 'selected' property.
  AssignOptionalValue(params->create_properties.active, &options.active);
  AssignOptionalValue(params->create_properties.pinned, &options.pinned);
  AssignOptionalValue(params->create_properties.index, &options.index);
  AssignOptionalValue(params->create_properties.url, &options.url);

  std::string error;
  std::unique_ptr<base::DictionaryValue> result(
      ExtensionTabUtil::OpenTab(this, options, user_gesture(), &error));
  if (!result)
    return RespondNow(Error(error));

  // Return data about the newly created tab.
  return RespondNow(has_callback() ? OneArgument(std::move(result))
                                   : NoArguments());
}

ExtensionFunction::ResponseAction TabsDuplicateFunction::Run() {
  std::unique_ptr<tabs::Duplicate::Params> params(
      tabs::Duplicate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  int tab_id = params->tab_id;

  Browser* browser = NULL;
  TabStripModel* tab_strip = NULL;
  int tab_index = -1;
  std::string error;
  if (!GetTabById(tab_id, browser_context(), include_incognito_information(),
                  &browser, &tab_strip, NULL, &tab_index, &error)) {
    return RespondNow(Error(error));
  }

  WebContents* new_contents = chrome::DuplicateTabAt(browser, tab_index);
  if (!has_callback())
    return RespondNow(NoArguments());

  // Duplicated tab may not be in the same window as the original, so find
  // the window and the tab.
  TabStripModel* new_tab_strip = NULL;
  int new_tab_index = -1;
  ExtensionTabUtil::GetTabStripModel(new_contents,
                                     &new_tab_strip,
                                     &new_tab_index);
  if (!new_tab_strip || new_tab_index == -1) {
    return RespondNow(Error(kUnknownErrorDoNotUse));
  }

  return RespondNow(ArgumentList(tabs::Get::Results::Create(
      *CreateTabObjectHelper(new_contents, extension(), source_context_type(),
                             new_tab_strip, new_tab_index))));
}

ExtensionFunction::ResponseAction TabsGetFunction::Run() {
  std::unique_ptr<tabs::Get::Params> params(tabs::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  int tab_id = params->tab_id;

  TabStripModel* tab_strip = NULL;
  WebContents* contents = NULL;
  int tab_index = -1;
  std::string error;
  if (!GetTabById(tab_id, browser_context(), include_incognito_information(),
                  NULL, &tab_strip, &contents, &tab_index, &error)) {
    return RespondNow(Error(error));
  }

  return RespondNow(ArgumentList(tabs::Get::Results::Create(
      *CreateTabObjectHelper(contents, extension(), source_context_type(),
                             tab_strip, tab_index))));
}

ExtensionFunction::ResponseAction TabsGetCurrentFunction::Run() {
  DCHECK(dispatcher());

  // Return the caller, if it's a tab. If not the result isn't an error but an
  // empty tab (hence returning true).
  WebContents* caller_contents = GetSenderWebContents();
  std::unique_ptr<base::ListValue> results;
  if (caller_contents && ExtensionTabUtil::GetTabId(caller_contents) >= 0) {
    results = tabs::Get::Results::Create(*CreateTabObjectHelper(
        caller_contents, extension(), source_context_type(), nullptr, -1));
  }
  return RespondNow(results ? ArgumentList(std::move(results)) : NoArguments());
}

ExtensionFunction::ResponseAction TabsHighlightFunction::Run() {
  std::unique_ptr<tabs::Highlight::Params> params(
      tabs::Highlight::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Get the window id from the params; default to current window if omitted.
  int window_id = extension_misc::kCurrentWindowId;
  if (params->highlight_info.window_id.get())
    window_id = *params->highlight_info.window_id;

  Browser* browser = NULL;
  std::string error;
  if (!GetBrowserFromWindowID(this, window_id, &browser, &error))
    return RespondNow(Error(error));

  TabStripModel* tabstrip = browser->tab_strip_model();
  ui::ListSelectionModel selection;
  int active_index = -1;

  if (params->highlight_info.tabs.as_integers) {
    std::vector<int>& tab_indices = *params->highlight_info.tabs.as_integers;
    // Create a new selection model as we read the list of tab indices.
    for (size_t i = 0; i < tab_indices.size(); ++i) {
      if (!HighlightTab(tabstrip, &selection, &active_index, tab_indices[i],
                        &error)) {
        return RespondNow(Error(error));
      }
    }
  } else {
    EXTENSION_FUNCTION_VALIDATE(params->highlight_info.tabs.as_integer);
    if (!HighlightTab(tabstrip, &selection, &active_index,
                      *params->highlight_info.tabs.as_integer, &error)) {
      return RespondNow(Error(error));
    }
  }

  // Make sure they actually specified tabs to select.
  if (selection.empty())
    return RespondNow(Error(tabs_constants::kNoHighlightedTabError));

  selection.set_active(active_index);
  browser->tab_strip_model()->SetSelectionFromModel(std::move(selection));
  return RespondNow(OneArgument(ExtensionTabUtil::CreateWindowValueForExtension(
      *browser, extension(), ExtensionTabUtil::kPopulateTabs,
      source_context_type())));
}

bool TabsHighlightFunction::HighlightTab(TabStripModel* tabstrip,
                                         ui::ListSelectionModel* selection,
                                         int* active_index,
                                         int index,
                                         std::string* error) {
  // Make sure the index is in range.
  if (!tabstrip->ContainsIndex(index)) {
    *error = ErrorUtils::FormatErrorMessage(
        tabs_constants::kTabIndexNotFoundError, base::NumberToString(index));
    return false;
  }

  // By default, we make the first tab in the list active.
  if (*active_index == -1)
    *active_index = index;

  selection->AddIndexToSelection(index);
  return true;
}

TabsUpdateFunction::TabsUpdateFunction() : web_contents_(NULL) {
}

ExtensionFunction::ResponseAction TabsUpdateFunction::Run() {
  std::unique_ptr<tabs::Update::Params> params(
      tabs::Update::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tab_id = -1;
  WebContents* contents = NULL;
  if (!params->tab_id.get()) {
    Browser* browser = ChromeExtensionFunctionDetails(this).GetCurrentBrowser();
    if (!browser)
      return RespondNow(Error(tabs_constants::kNoCurrentWindowError));
    contents = browser->tab_strip_model()->GetActiveWebContents();
    if (!contents)
      return RespondNow(Error(tabs_constants::kNoSelectedTabError));
    tab_id = SessionTabHelper::IdForTab(contents).id();
  } else {
    tab_id = *params->tab_id;
  }

  int tab_index = -1;
  TabStripModel* tab_strip = NULL;
  Browser* browser = nullptr;
  std::string error;
  if (!GetTabById(tab_id, browser_context(), include_incognito_information(),
                  &browser, &tab_strip, &contents, &tab_index, &error)) {
    return RespondNow(Error(error));
  }

  if (!ExtensionTabUtil::BrowserSupportsTabs(browser))
    return RespondNow(Error(tabs_constants::kNoCurrentWindowError));

  web_contents_ = contents;

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  // Navigate the tab to a new location if the url is different.
  if (params->update_properties.url.get()) {
    std::string updated_url = *params->update_properties.url;
    if (browser->profile()->IsIncognitoProfile() &&
        !IsURLAllowedInIncognito(GURL(updated_url), browser->profile())) {
      return RespondNow(Error(ErrorUtils::FormatErrorMessage(
          tabs_constants::kURLsNotAllowedInIncognitoError, updated_url)));
    }
    if (!UpdateURL(updated_url, tab_id, &error))
      return RespondNow(Error(error));
  }

  bool active = false;
  // TODO(rafaelw): Setting |active| from js doesn't make much sense.
  // Move tab selection management up to window.
  if (params->update_properties.selected.get())
    active = *params->update_properties.selected;

  // The 'active' property has replaced 'selected'.
  if (params->update_properties.active.get())
    active = *params->update_properties.active;

  if (active) {
    if (tab_strip->active_index() != tab_index) {
      tab_strip->ActivateTabAt(tab_index);
      DCHECK_EQ(contents, tab_strip->GetActiveWebContents());
    }
  }

  if (params->update_properties.highlighted.get()) {
    bool highlighted = *params->update_properties.highlighted;
    if (highlighted != tab_strip->IsTabSelected(tab_index))
      tab_strip->ToggleSelectionAt(tab_index);
  }

  if (params->update_properties.pinned.get()) {
    bool pinned = *params->update_properties.pinned;
    tab_strip->SetTabPinned(tab_index, pinned);

    // Update the tab index because it may move when being pinned.
    tab_index = tab_strip->GetIndexOfWebContents(contents);
  }

  if (params->update_properties.muted.get() &&
      !chrome::SetTabAudioMuted(contents, *params->update_properties.muted,
                                TabMutedReason::EXTENSION, extension()->id())) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        tabs_constants::kCannotUpdateMuteCaptured,
        base::NumberToString(tab_id))));
  }

  if (params->update_properties.opener_tab_id.get()) {
    int opener_id = *params->update_properties.opener_tab_id;
    WebContents* opener_contents = NULL;
    if (opener_id == tab_id)
      return RespondNow(Error("Cannot set a tab's opener to itself."));
    if (!ExtensionTabUtil::GetTabById(opener_id, browser_context(),
                                      include_incognito_information(), nullptr,
                                      nullptr, &opener_contents, nullptr)) {
      return RespondNow(Error(ErrorUtils::FormatErrorMessage(
          tabs_constants::kTabNotFoundError, base::NumberToString(opener_id))));
    }

    if (tab_strip->GetIndexOfWebContents(opener_contents) ==
        TabStripModel::kNoTab) {
      return RespondNow(
          Error("Tab opener must be in the same window as the updated tab."));
    }
    tab_strip->SetOpenerOfWebContentsAt(tab_index, opener_contents);
  }

  if (params->update_properties.auto_discardable.get()) {
    bool state = *params->update_properties.auto_discardable;
    resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
        web_contents_)
        ->SetAutoDiscardable(state);
  }

  return RespondNow(GetResult());
}

bool TabsUpdateFunction::UpdateURL(const std::string& url_string,
                                   int tab_id,
                                   std::string* error) {
  GURL url =
      ExtensionTabUtil::ResolvePossiblyRelativeURL(url_string, extension());

  if (!url.is_valid()) {
    *error = ErrorUtils::FormatErrorMessage(tabs_constants::kInvalidUrlError,
                                            url_string);
    return false;
  }

  // Don't let the extension crash the browser or renderers.
  if (ExtensionTabUtil::IsKillURL(url)) {
    *error = tabs_constants::kNoCrashBrowserError;
    return false;
  }

  const bool is_javascript_scheme = url.SchemeIs(url::kJavaScriptScheme);
  UMA_HISTOGRAM_BOOLEAN("Extensions.ApiTabUpdateJavascript",
                        is_javascript_scheme);
  // JavaScript URLs are forbidden in chrome.tabs.update().
  if (is_javascript_scheme) {
    *error = tabs_constants::kJavaScriptUrlsNotAllowedInTabsUpdate;
    return false;
  }

  NavigationController::LoadURLParams load_params(url);

  // Treat extension-initiated navigations as renderer-initiated so that the URL
  // does not show in the omnibox until it commits.  This avoids URL spoofs
  // since URLs can be opened on behalf of untrusted content.
  load_params.is_renderer_initiated = true;
  // All renderer-initiated navigations need to have an initiator origin.
  load_params.initiator_origin = url::Origin::Create(
      Extension::GetBaseURLFromExtensionId(extension()->id()));
  // |source_site_instance| needs to be set so that a renderer process
  // compatible with |initiator_origin| is picked by Site Isolation.
  load_params.source_site_instance = content::SiteInstance::CreateForURL(
      web_contents_->GetBrowserContext(),
      load_params.initiator_origin->GetURL());

  web_contents_->GetController().LoadURLWithParams(load_params);

  DCHECK_EQ(url,
            web_contents_->GetController().GetPendingEntry()->GetVirtualURL());

  return true;
}

ExtensionFunction::ResponseValue TabsUpdateFunction::GetResult() {
  if (!has_callback())
    return NoArguments();

  return ArgumentList(tabs::Get::Results::Create(*CreateTabObjectHelper(
      web_contents_, extension(), source_context_type(), nullptr, -1)));
}

void TabsUpdateFunction::OnExecuteCodeFinished(
    const std::string& error,
    const GURL& url,
    const base::ListValue& script_result) {
  if (!error.empty()) {
    Respond(Error(error));
    return;
  }

  return Respond(GetResult());
}

ExtensionFunction::ResponseAction TabsMoveFunction::Run() {
  std::unique_ptr<tabs::Move::Params> params(
      tabs::Move::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int new_index = params->move_properties.index;
  int* window_id = params->move_properties.window_id.get();
  std::unique_ptr<base::ListValue> tab_values(new base::ListValue());

  size_t num_tabs = 0;
  std::string error;
  if (params->tab_ids.as_integers) {
    std::vector<int>& tab_ids = *params->tab_ids.as_integers;
    num_tabs = tab_ids.size();
    for (size_t i = 0; i < tab_ids.size(); ++i) {
      if (!MoveTab(tab_ids[i], &new_index, i, tab_values.get(), window_id,
                   &error)) {
        return RespondNow(Error(error));
      }
    }
  } else {
    EXTENSION_FUNCTION_VALIDATE(params->tab_ids.as_integer);
    num_tabs = 1;
    if (!MoveTab(*params->tab_ids.as_integer, &new_index, 0, tab_values.get(),
                 window_id, &error)) {
      return RespondNow(Error(error));
    }
  }

  // TODO(devlin): It's weird that whether or not the method provides a callback
  // can determine its success (as we return errors below).
  if (!has_callback())
    return RespondNow(NoArguments());

  if (num_tabs == 0)
    return RespondNow(Error("No tabs given."));
  if (num_tabs == 1) {
    std::unique_ptr<base::Value> value;
    CHECK(tab_values->Remove(0, &value));
    return RespondNow(OneArgument(std::move(value)));
  }

  // Return the results as an array if there are multiple tabs.
  return RespondNow(OneArgument(std::move(tab_values)));
}

bool TabsMoveFunction::MoveTab(int tab_id,
                               int* new_index,
                               int iteration,
                               base::ListValue* tab_values,
                               int* window_id,
                               std::string* error) {
  Browser* source_browser = NULL;
  TabStripModel* source_tab_strip = NULL;
  WebContents* contents = NULL;
  int tab_index = -1;
  if (!GetTabById(tab_id, browser_context(), include_incognito_information(),
                  &source_browser, &source_tab_strip, &contents, &tab_index,
                  error)) {
    return false;
  }

  // Don't let the extension move the tab if the user is dragging tabs.
  if (!source_browser->window()->IsTabStripEditable()) {
    *error = tabs_constants::kTabStripNotEditableError;
    return false;
  }

  // Insert the tabs one after another.
  *new_index += iteration;

  if (window_id) {
    Browser* target_browser = NULL;

    if (!GetBrowserFromWindowID(this, *window_id, &target_browser, error))
      return false;

    if (!target_browser->window()->IsTabStripEditable()) {
      *error = tabs_constants::kTabStripNotEditableError;
      return false;
    }

    // TODO(crbug.com/990158): Rather than calling is_type_normal(), should
    // this call SupportsWindowFeature(Browser::FEATURE_TABSTRIP)?
    if (!target_browser->is_type_normal()) {
      *error = tabs_constants::kCanOnlyMoveTabsWithinNormalWindowsError;
      return false;
    }

    if (target_browser->profile() != source_browser->profile()) {
      *error = tabs_constants::kCanOnlyMoveTabsWithinSameProfileError;
      return false;
    }

    // If windowId is different from the current window, move between windows.
    if (ExtensionTabUtil::GetWindowId(target_browser) !=
        ExtensionTabUtil::GetWindowId(source_browser)) {
      TabStripModel* target_tab_strip = target_browser->tab_strip_model();
      std::unique_ptr<content::WebContents> web_contents =
          source_tab_strip->DetachWebContentsAt(tab_index);
      if (!web_contents) {
        *error = ErrorUtils::FormatErrorMessage(
            tabs_constants::kTabNotFoundError, base::NumberToString(tab_id));
        return false;
      }

      // Clamp move location to the last position.
      // This is ">" because it can append to a new index position.
      // -1 means set the move location to the last position.
      if (*new_index > target_tab_strip->count() || *new_index < 0)
        *new_index = target_tab_strip->count();

      content::WebContents* web_contents_raw = web_contents.get();

      *new_index = target_tab_strip->InsertWebContentsAt(
          *new_index, std::move(web_contents), TabStripModel::ADD_NONE);

      if (has_callback()) {
        tab_values->Append(CreateTabObjectHelper(web_contents_raw, extension(),
                                                 source_context_type(),
                                                 target_tab_strip, *new_index)
                               ->ToValue());
      }

      return true;
    }
  }

  // Perform a simple within-window move.
  // Clamp move location to the last position.
  // This is ">=" because the move must be to an existing location.
  // -1 means set the move location to the last position.
  if (*new_index >= source_tab_strip->count() || *new_index < 0)
    *new_index = source_tab_strip->count() - 1;

  if (*new_index != tab_index)
    *new_index =
        source_tab_strip->MoveWebContentsAt(tab_index, *new_index, false);

  if (has_callback()) {
    tab_values->Append(CreateTabObjectHelper(contents, extension(),
                                             source_context_type(),
                                             source_tab_strip, *new_index)
                           ->ToValue());
  }

  return true;
}

ExtensionFunction::ResponseAction TabsReloadFunction::Run() {
  std::unique_ptr<tabs::Reload::Params> params(
      tabs::Reload::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool bypass_cache = false;
  if (params->reload_properties.get() &&
      params->reload_properties->bypass_cache.get()) {
    bypass_cache = *params->reload_properties->bypass_cache;
  }

  content::WebContents* web_contents = NULL;

  // If |tab_id| is specified, look for it. Otherwise default to selected tab
  // in the current window.
  Browser* current_browser =
      ChromeExtensionFunctionDetails(this).GetCurrentBrowser();
  if (!params->tab_id.get()) {
    if (!current_browser)
      return RespondNow(Error(tabs_constants::kNoCurrentWindowError));

    if (!ExtensionTabUtil::GetDefaultTab(current_browser, &web_contents, NULL))
      return RespondNow(Error(kUnknownErrorDoNotUse));
  } else {
    int tab_id = *params->tab_id;

    Browser* browser = NULL;
    std::string error;
    if (!GetTabById(tab_id, browser_context(), include_incognito_information(),
                    &browser, NULL, &web_contents, NULL, &error)) {
      return RespondNow(Error(error));
    }
  }

  if (web_contents->ShowingInterstitialPage()) {
    // This does as same as Browser::ReloadInternal.
    NavigationEntry* entry = web_contents->GetController().GetVisibleEntry();
    GURL reload_url = entry ? entry->GetURL() : GURL(url::kAboutBlankURL);
    OpenURLParams params(reload_url, Referrer(),
                         WindowOpenDisposition::CURRENT_TAB,
                         ui::PAGE_TRANSITION_RELOAD, false);
    current_browser->OpenURL(params);
  } else {
    web_contents->GetController().Reload(
        bypass_cache ? content::ReloadType::BYPASSING_CACHE
                     : content::ReloadType::NORMAL,
        true);
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsRemoveFunction::Run() {
  std::unique_ptr<tabs::Remove::Params> params(
      tabs::Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string error;
  if (params->tab_ids.as_integers) {
    std::vector<int>& tab_ids = *params->tab_ids.as_integers;
    for (size_t i = 0; i < tab_ids.size(); ++i) {
      if (!RemoveTab(tab_ids[i], &error))
        return RespondNow(Error(error));
    }
  } else {
    EXTENSION_FUNCTION_VALIDATE(params->tab_ids.as_integer);
    if (!RemoveTab(*params->tab_ids.as_integer, &error))
      return RespondNow(Error(error));
  }
  return RespondNow(NoArguments());
}

bool TabsRemoveFunction::RemoveTab(int tab_id, std::string* error) {
  Browser* browser = NULL;
  WebContents* contents = NULL;
  if (!GetTabById(tab_id, browser_context(), include_incognito_information(),
                  &browser, nullptr, &contents, nullptr, error)) {
    return false;
  }

  // Don't let the extension remove a tab if the user is dragging tabs around.
  if (!browser->window()->IsTabStripEditable()) {
    *error = tabs_constants::kTabStripNotEditableError;
    return false;
  }
  // There's a chance that the tab is being dragged, or we're in some other
  // nested event loop. This code path ensures that the tab is safely closed
  // under such circumstances, whereas |TabStripModel::CloseWebContentsAt()|
  // does not.
  contents->Close();
  return true;
}

TabsCaptureVisibleTabFunction::TabsCaptureVisibleTabFunction()
    : chrome_details_(this) {
}

bool TabsCaptureVisibleTabFunction::IsScreenshotEnabled() const {
  PrefService* service = chrome_details_.GetProfile()->GetPrefs();
  if (service->GetBoolean(prefs::kDisableScreenshots)) {
    return false;
  }
  return true;
}

bool TabsCaptureVisibleTabFunction::ClientAllowsTransparency() {
  return false;
}

WebContents* TabsCaptureVisibleTabFunction::GetWebContentsForID(
    int window_id,
    std::string* error) {
  Browser* browser = NULL;
  if (!GetBrowserFromWindowID(chrome_details_, window_id, &browser, error))
    return nullptr;

  WebContents* contents = browser->tab_strip_model()->GetActiveWebContents();
  if (!contents) {
    *error = "No active web contents to capture";
    return nullptr;
  }

  if (!extension()->permissions_data()->CanCaptureVisiblePage(
          contents->GetLastCommittedURL(),
          SessionTabHelper::IdForTab(contents).id(), error,
          extensions::CaptureRequirement::kActiveTabOrAllUrls)) {
    return nullptr;
  }
  return contents;
}

ExtensionFunction::ResponseAction TabsCaptureVisibleTabFunction::Run() {
  using api::extension_types::ImageDetails;

  EXTENSION_FUNCTION_VALIDATE(args_);

  int context_id = extension_misc::kCurrentWindowId;
  args_->GetInteger(0, &context_id);

  std::unique_ptr<ImageDetails> image_details;
  if (args_->GetSize() > 1) {
    base::Value* spec = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->Get(1, &spec) && spec);
    image_details = ImageDetails::FromValue(*spec);
  }

  std::string error;
  WebContents* contents = GetWebContentsForID(context_id, &error);
  if (!contents)
    return RespondNow(Error(error));

  const CaptureResult capture_result = CaptureAsync(
      contents, image_details.get(),
      base::BindOnce(&TabsCaptureVisibleTabFunction::CopyFromSurfaceComplete,
                     this));
  if (capture_result == OK) {
    // CopyFromSurfaceComplete might have already responded.
    return did_respond() ? AlreadyResponded() : RespondLater();
  }

  return RespondNow(Error(CaptureResultToErrorMessage(capture_result)));
}

void TabsCaptureVisibleTabFunction::OnCaptureSuccess(const SkBitmap& bitmap) {
  std::string base64_result;
  if (!EncodeBitmap(bitmap, &base64_result)) {
    OnCaptureFailure(FAILURE_REASON_ENCODING_FAILED);
    return;
  }

  Respond(OneArgument(std::make_unique<base::Value>(std::move(base64_result))));
}

void TabsCaptureVisibleTabFunction::OnCaptureFailure(CaptureResult result) {
  Respond(Error(CaptureResultToErrorMessage(result)));
}

// static.
std::string TabsCaptureVisibleTabFunction::CaptureResultToErrorMessage(
    CaptureResult result) {
  const char* reason_description = "internal error";
  switch (result) {
    case FAILURE_REASON_READBACK_FAILED:
      reason_description = "image readback failed";
      break;
    case FAILURE_REASON_ENCODING_FAILED:
      reason_description = "encoding failed";
      break;
    case FAILURE_REASON_VIEW_INVISIBLE:
      reason_description = "view is invisible";
      break;
    case FAILURE_REASON_SCREEN_SHOTS_DISABLED:
      return tabs_constants::kScreenshotsDisabled;
    case OK:
      NOTREACHED() << "CaptureResultToErrorMessage should not be called"
                      " with a successful result";
      return kUnknownErrorDoNotUse;
  }
  return ErrorUtils::FormatErrorMessage("Failed to capture tab: *",
                                        reason_description);
}

void TabsCaptureVisibleTabFunction::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kDisableScreenshots, false);
}

ExtensionFunction::ResponseAction TabsDetectLanguageFunction::Run() {
  std::unique_ptr<tabs::DetectLanguage::Params> params(
      tabs::DetectLanguage::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tab_id = 0;
  Browser* browser = NULL;
  WebContents* contents = NULL;

  // If |tab_id| is specified, look for it. Otherwise default to selected tab
  // in the current window.
  std::string error;
  if (params->tab_id.get()) {
    tab_id = *params->tab_id;
    if (!GetTabById(tab_id, browser_context(), include_incognito_information(),
                    &browser, nullptr, &contents, nullptr, &error)) {
      return RespondNow(Error(error));
    }
    // TODO(devlin): Can this happen? GetTabById() should return false if
    // |browser| or |contents| is null.
    if (!browser || !contents)
      return RespondNow(Error(kUnknownErrorDoNotUse));
  } else {
    browser = ChromeExtensionFunctionDetails(this).GetCurrentBrowser();
    if (!browser)
      return RespondNow(Error(tabs_constants::kNoCurrentWindowError));
    contents = browser->tab_strip_model()->GetActiveWebContents();
    if (!contents)
      return RespondNow(Error(tabs_constants::kNoSelectedTabError));
  }

  if (contents->GetController().NeedsReload()) {
    // If the tab hasn't been loaded, don't wait for the tab to load.
    return RespondNow(
        Error(tabs_constants::kCannotDetermineLanguageOfUnloadedTab));
  }

  AddRef();  // Balanced in RespondWithLanguage().

  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(contents);
  if (!chrome_translate_client->GetLanguageState()
           .original_language()
           .empty()) {
    // Delay the callback invocation until after the current JS call has
    // returned.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &TabsDetectLanguageFunction::RespondWithLanguage, this,
            chrome_translate_client->GetLanguageState().original_language()));
    return RespondLater();
  }

  // The tab contents does not know its language yet. Let's wait until it
  // receives it, or until the tab is closed/navigates to some other page.

  // Observe the WebContents' lifetime and navigations.
  Observe(contents);
  // Wait until the language is determined.
  chrome_translate_client->translate_driver()->AddObserver(this);
  is_observing_ = true;

  return RespondLater();
}

void TabsDetectLanguageFunction::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  // Call RespondWithLanguage() with an empty string as we want to guarantee the
  // callback is called for every API call the extension made.
  RespondWithLanguage(std::string());
}

void TabsDetectLanguageFunction::WebContentsDestroyed() {
  // Call RespondWithLanguage() with an empty string as we want to guarantee the
  // callback is called for every API call the extension made.
  RespondWithLanguage(std::string());
}

void TabsDetectLanguageFunction::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  RespondWithLanguage(details.adopted_language);
}

void TabsDetectLanguageFunction::RespondWithLanguage(
    const std::string& language) {
  // Stop observing.
  if (is_observing_) {
    ChromeTranslateClient::FromWebContents(web_contents())
        ->translate_driver()
        ->RemoveObserver(this);
    Observe(nullptr);
  }

  Respond(OneArgument(std::make_unique<base::Value>(language)));
  Release();  // Balanced in Run()
}

ExecuteCodeInTabFunction::ExecuteCodeInTabFunction()
    : chrome_details_(this), execute_tab_id_(-1) {
}

ExecuteCodeInTabFunction::~ExecuteCodeInTabFunction() {}

ExecuteCodeFunction::InitResult ExecuteCodeInTabFunction::Init() {
  if (init_result_)
    return init_result_.value();

  // |tab_id| is optional so it's ok if it's not there.
  int tab_id = -1;
  if (args_->GetInteger(0, &tab_id) && tab_id < 0)
    return set_init_result(VALIDATION_FAILURE);

  // |details| are not optional.
  base::DictionaryValue* details_value = NULL;
  if (!args_->GetDictionary(1, &details_value))
    return set_init_result(VALIDATION_FAILURE);
  std::unique_ptr<InjectDetails> details(new InjectDetails());
  if (!InjectDetails::Populate(*details_value, details.get()))
    return set_init_result(VALIDATION_FAILURE);

  // If the tab ID wasn't given then it needs to be converted to the
  // currently active tab's ID.
  if (tab_id == -1) {
    Browser* browser = chrome_details_.GetCurrentBrowser();
    // Can happen during shutdown.
    if (!browser)
      return set_init_result_error(tabs_constants::kNoCurrentWindowError);
    content::WebContents* web_contents = NULL;
    // Can happen during shutdown.
    if (!ExtensionTabUtil::GetDefaultTab(browser, &web_contents, &tab_id))
      return set_init_result_error(tabs_constants::kNoTabInBrowserWindowError);
  }

  execute_tab_id_ = tab_id;
  details_ = std::move(details);
  set_host_id(HostID(HostID::EXTENSIONS, extension()->id()));
  return set_init_result(SUCCESS);
}

bool ExecuteCodeInTabFunction::CanExecuteScriptOnPage(std::string* error) {
  content::WebContents* contents = nullptr;

  // If |tab_id| is specified, look for the tab. Otherwise default to selected
  // tab in the current window.
  CHECK_GE(execute_tab_id_, 0);
  if (!GetTabById(execute_tab_id_, browser_context(),
                  include_incognito_information(), nullptr, nullptr, &contents,
                  nullptr, error)) {
    return false;
  }

  CHECK(contents);

  int frame_id = details_->frame_id ? *details_->frame_id
                                    : ExtensionApiFrameIdMap::kTopFrameId;
  content::RenderFrameHost* rfh =
      ExtensionApiFrameIdMap::GetRenderFrameHostById(contents, frame_id);
  if (!rfh) {
    *error = ErrorUtils::FormatErrorMessage(
        tabs_constants::kFrameNotFoundError, base::NumberToString(frame_id),
        base::NumberToString(execute_tab_id_));
    return false;
  }

  // Content scripts declared in manifest.json can access frames at about:-URLs
  // if the extension has permission to access the frame's origin, so also allow
  // programmatic content scripts at about:-URLs for allowed origins.
  GURL effective_document_url(rfh->GetLastCommittedURL());
  bool is_about_url = effective_document_url.SchemeIs(url::kAboutScheme);
  if (is_about_url && details_->match_about_blank &&
      *details_->match_about_blank) {
    effective_document_url = GURL(rfh->GetLastCommittedOrigin().Serialize());
  }

  if (!effective_document_url.is_valid()) {
    // Unknown URL, e.g. because no load was committed yet. Allow for now, the
    // renderer will check again and fail the injection if needed.
    return true;
  }

  // NOTE: This can give the wrong answer due to race conditions, but it is OK,
  // we check again in the renderer.
  if (!extension()->permissions_data()->CanAccessPage(effective_document_url,
                                                      execute_tab_id_, error)) {
    if (is_about_url &&
        extension()->permissions_data()->active_permissions().HasAPIPermission(
            APIPermission::kTab)) {
      *error = ErrorUtils::FormatErrorMessage(
          manifest_errors::kCannotAccessAboutUrl,
          rfh->GetLastCommittedURL().spec(),
          rfh->GetLastCommittedOrigin().Serialize());
    }
    return false;
  }

  return true;
}

ScriptExecutor* ExecuteCodeInTabFunction::GetScriptExecutor(
    std::string* error) {
  Browser* browser = nullptr;
  content::WebContents* contents = nullptr;

  bool success = GetTabById(execute_tab_id_, browser_context(),
                            include_incognito_information(), &browser, nullptr,
                            &contents, nullptr, error) &&
                 contents && browser;

  if (!success)
    return nullptr;

  return TabHelper::FromWebContents(contents)->script_executor();
}

bool ExecuteCodeInTabFunction::IsWebView() const {
  return false;
}

const GURL& ExecuteCodeInTabFunction::GetWebViewSrc() const {
  return GURL::EmptyGURL();
}

bool TabsExecuteScriptFunction::ShouldInsertCSS() const {
  return false;
}

bool TabsInsertCSSFunction::ShouldInsertCSS() const {
  return true;
}

ExtensionFunction::ResponseAction TabsSetZoomFunction::Run() {
  std::unique_ptr<tabs::SetZoom::Params> params(
      tabs::SetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  std::string error;
  WebContents* web_contents =
      GetTabsAPIDefaultWebContents(this, tab_id, &error);
  if (!web_contents)
    return RespondNow(Error(error));

  GURL url(web_contents->GetVisibleURL());
  if (extension()->permissions_data()->IsRestrictedUrl(url, &error))
    return RespondNow(Error(error));

  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);
  double zoom_level =
      params->zoom_factor > 0
          ? blink::PageZoomFactorToZoomLevel(params->zoom_factor)
          : zoom_controller->GetDefaultZoomLevel();

  auto client = base::MakeRefCounted<ExtensionZoomRequestClient>(extension());
  if (!zoom_controller->SetZoomLevelByClient(zoom_level, client)) {
    // Tried to zoom a tab in disabled mode.
    return RespondNow(Error(tabs_constants::kCannotZoomDisabledTabError));
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsGetZoomFunction::Run() {
  std::unique_ptr<tabs::GetZoom::Params> params(
      tabs::GetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  std::string error;
  WebContents* web_contents =
      GetTabsAPIDefaultWebContents(this, tab_id, &error);
  if (!web_contents)
    return RespondNow(Error(error));

  double zoom_level =
      ZoomController::FromWebContents(web_contents)->GetZoomLevel();
  double zoom_factor = blink::PageZoomLevelToZoomFactor(zoom_level);

  return RespondNow(ArgumentList(tabs::GetZoom::Results::Create(zoom_factor)));
}

ExtensionFunction::ResponseAction TabsSetZoomSettingsFunction::Run() {
  using api::tabs::ZoomSettings;

  std::unique_ptr<tabs::SetZoomSettings::Params> params(
      tabs::SetZoomSettings::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  std::string error;
  WebContents* web_contents =
      GetTabsAPIDefaultWebContents(this, tab_id, &error);
  if (!web_contents)
    return RespondNow(Error(error));

  GURL url(web_contents->GetVisibleURL());
  if (extension()->permissions_data()->IsRestrictedUrl(url, &error))
    return RespondNow(Error(error));

  // "per-origin" scope is only available in "automatic" mode.
  if (params->zoom_settings.scope == tabs::ZOOM_SETTINGS_SCOPE_PER_ORIGIN &&
      params->zoom_settings.mode != tabs::ZOOM_SETTINGS_MODE_AUTOMATIC &&
      params->zoom_settings.mode != tabs::ZOOM_SETTINGS_MODE_NONE) {
    return RespondNow(Error(tabs_constants::kPerOriginOnlyInAutomaticError));
  }

  // Determine the correct internal zoom mode to set |web_contents| to from the
  // user-specified |zoom_settings|.
  ZoomController::ZoomMode zoom_mode = ZoomController::ZOOM_MODE_DEFAULT;
  switch (params->zoom_settings.mode) {
    case tabs::ZOOM_SETTINGS_MODE_NONE:
    case tabs::ZOOM_SETTINGS_MODE_AUTOMATIC:
      switch (params->zoom_settings.scope) {
        case tabs::ZOOM_SETTINGS_SCOPE_NONE:
        case tabs::ZOOM_SETTINGS_SCOPE_PER_ORIGIN:
          zoom_mode = ZoomController::ZOOM_MODE_DEFAULT;
          break;
        case tabs::ZOOM_SETTINGS_SCOPE_PER_TAB:
          zoom_mode = ZoomController::ZOOM_MODE_ISOLATED;
      }
      break;
    case tabs::ZOOM_SETTINGS_MODE_MANUAL:
      zoom_mode = ZoomController::ZOOM_MODE_MANUAL;
      break;
    case tabs::ZOOM_SETTINGS_MODE_DISABLED:
      zoom_mode = ZoomController::ZOOM_MODE_DISABLED;
  }

  ZoomController::FromWebContents(web_contents)->SetZoomMode(zoom_mode);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsGetZoomSettingsFunction::Run() {
  std::unique_ptr<tabs::GetZoomSettings::Params> params(
      tabs::GetZoomSettings::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  std::string error;
  WebContents* web_contents =
      GetTabsAPIDefaultWebContents(this, tab_id, &error);
  if (!web_contents)
    return RespondNow(Error(error));
  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);

  ZoomController::ZoomMode zoom_mode = zoom_controller->zoom_mode();
  api::tabs::ZoomSettings zoom_settings;
  ZoomModeToZoomSettings(zoom_mode, &zoom_settings);
  zoom_settings.default_zoom_factor.reset(
      new double(blink::PageZoomLevelToZoomFactor(
          zoom_controller->GetDefaultZoomLevel())));

  return RespondNow(
      ArgumentList(api::tabs::GetZoomSettings::Results::Create(zoom_settings)));
}

ExtensionFunction::ResponseAction TabsDiscardFunction::Run() {
  std::unique_ptr<tabs::Discard::Params> params(
      tabs::Discard::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  WebContents* contents = nullptr;
  // If |tab_id| is given, find the web_contents respective to it.
  // Otherwise invoke discard function in TabManager with null web_contents
  // that will discard the least important tab.
  if (params->tab_id) {
    int tab_id = *params->tab_id;
    std::string error;
    if (!GetTabById(tab_id, browser_context(), include_incognito_information(),
                    nullptr, nullptr, &contents, nullptr, &error)) {
      return RespondNow(Error(error));
    }
  }
  // Discard the tab.
  contents =
      g_browser_process->GetTabManager()->DiscardTabByExtension(contents);

  // Create the Tab object and return it in case of success.
  if (contents) {
    return RespondNow(
        ArgumentList(tabs::Discard::Results::Create(*CreateTabObjectHelper(
            contents, extension(), source_context_type(), nullptr, -1))));
  }

  // Return appropriate error message otherwise.
  return RespondNow(Error(params->tab_id
                              ? ErrorUtils::FormatErrorMessage(
                                    tabs_constants::kCannotDiscardTab,
                                    base::NumberToString(*params->tab_id))
                              : tabs_constants::kCannotFindTabToDiscard));
}

TabsDiscardFunction::TabsDiscardFunction() {}
TabsDiscardFunction::~TabsDiscardFunction() {}

ExtensionFunction::ResponseAction TabsGoForwardFunction::Run() {
  std::unique_ptr<tabs::GoForward::Params> params(
      tabs::GoForward::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tab_id = params->tab_id ? *params->tab_id : -1;
  std::string error;
  WebContents* web_contents =
      GetTabsAPIDefaultWebContents(this, tab_id, &error);
  if (!web_contents)
    return RespondNow(Error(error));

  NavigationController& controller = web_contents->GetController();
  if (!controller.CanGoForward())
    return RespondNow(Error(tabs_constants::kNotFoundNextPageError));

  controller.GoForward();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsGoBackFunction::Run() {
  std::unique_ptr<tabs::GoBack::Params> params(
      tabs::GoBack::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tab_id = params->tab_id ? *params->tab_id : -1;
  std::string error;
  WebContents* web_contents =
      GetTabsAPIDefaultWebContents(this, tab_id, &error);
  if (!web_contents)
    return RespondNow(Error(error));

  NavigationController& controller = web_contents->GetController();
  if (!controller.CanGoBack())
    return RespondNow(Error(tabs_constants::kNotFoundNextPageError));

  controller.GoBack();
  return RespondNow(NoArguments());
}

}  // namespace extensions
