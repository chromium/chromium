// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/windows_util.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/split_tab_id.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "components/webapps/common/web_app_id.h"
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
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/file_reader.h"
#include "extensions/browser/script_executor.h"
#include "extensions/common/api/extension_types.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#include "extensions/common/message_bundle.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/wm/window_pin_util.h"
#include "chrome/browser/ui/browser_command_controller.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using content::BrowserThread;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;
using tabs::TabModel;

namespace extensions {

namespace windows = api::windows;
namespace tabs = api::tabs;

using api::extension_types::InjectDetails;

namespace {

constexpr char kTabIndexNotFoundError[] = "No tab at index: *.";
constexpr char kCannotFindTabToDiscard[] = "Cannot find a tab to discard.";
constexpr char kNoHighlightedTabError[] = "No highlighted tab";
constexpr char kInvalidWindowTypeError[] = "Invalid value for type";
constexpr char kCannotUpdateMuteCaptured[] =
    "Cannot update mute state for tab *, tab has audio or video currently "
    "being captured";
constexpr char kWindowCreateSupportsOnlySingleIwaUrlError[] =
    "When creating a window for a URL with the 'isolated-app:' scheme, only "
    "one tab can be added to the window.";
constexpr char kWindowCreateCannotParseIwaUrlError[] =
    "Unable to parse 'isolated-app:' URL: %s";
constexpr char kWindowCreateCannotUseTabIdWithIwaError[] =
    "Creating a new window for an Isolated Web App does not support adding a "
    "tab by its ID.";
constexpr char kWindowCreateCannotMoveIwaTabError[] =
    "The tab of an Isolated Web App cannot be moved to a new window.";

// Returns the last active browser with the given `profile`. If
// `include_incognito_information` is true, this will also return a browser
// that crosses the incognito boundary.
BrowserWindowInterface* GetLastActiveBrowserWithProfile(
    Profile* profile,
    bool include_incognito_information) {
  std::vector<BrowserWindowInterface*> all_browsers =
      GetBrowserWindowInterfacesOrderedByActivation();
  for (auto* browser : all_browsers) {
    if (browser->GetProfile() == profile ||
        (include_incognito_information &&
         profile->IsSameOrParent(browser->GetProfile()))) {
      return browser;
    }
  }

  return nullptr;
}

// Returns true if either |boolean| is disengaged, or if |boolean| and
// |value| are equal. This function is used to check if a tab's parameters match
// those of the browser.
bool MatchesBool(const std::optional<bool>& boolean, bool value) {
  return !boolean || *boolean == value;
}

ui::mojom::WindowShowState ConvertToWindowShowState(
    windows::WindowState state) {
  switch (state) {
    case windows::WindowState::kNormal:
      return ui::mojom::WindowShowState::kNormal;
    case windows::WindowState::kMinimized:
      return ui::mojom::WindowShowState::kMinimized;
    case windows::WindowState::kMaximized:
      return ui::mojom::WindowShowState::kMaximized;
    case windows::WindowState::kFullscreen:
    case windows::WindowState::kLockedFullscreen:
      return ui::mojom::WindowShowState::kFullscreen;
    case windows::WindowState::kNone:
      return ui::mojom::WindowShowState::kDefault;
  }
  NOTREACHED();
}

bool IsValidStateForWindowsCreateFunction(
    const windows::Create::Params::CreateData* create_data) {
  if (!create_data) {
    return true;
  }

  bool has_bound = create_data->left || create_data->top ||
                   create_data->width || create_data->height;

  switch (create_data->state) {
    case windows::WindowState::kMinimized:
      // If minimised, default focused state should be unfocused.
      return !(create_data->focused && *create_data->focused) && !has_bound;
    case windows::WindowState::kMaximized:
    case windows::WindowState::kFullscreen:
    case windows::WindowState::kLockedFullscreen:
      // If maximised/fullscreen, default focused state should be focused.
      return !(create_data->focused && !*create_data->focused) && !has_bound;
    case windows::WindowState::kNormal:
    case windows::WindowState::kNone:
      return true;
  }
  NOTREACHED();
}

// Moves the given tab to the |target_browser|. On success, returns the
// new index of the tab in the target tabstrip. On failure, returns -1.
// Assumes that the caller has already checked whether the target window is
// different from the source.
int MoveTabToWindow(ExtensionFunction* function,
                    int tab_id,
                    Browser* target_browser,
                    int new_index,
                    std::string* error) {
  WindowController* source_window = nullptr;
  int source_index = -1;
  if (!tabs_internal::GetTabById(tab_id, function->browser_context(),
                                 function->include_incognito_information(),
                                 &source_window, nullptr, &source_index,
                                 error) ||
      !source_window) {
    return -1;
  }

  if (!ExtensionTabUtil::IsTabStripEditable()) {
    *error = ExtensionTabUtil::kTabStripNotEditableError;
    return -1;
  }

  // TODO(crbug.com/40638654): Rather than calling is_type_normal(), should
  // this call SupportsWindowFeature(Browser::FEATURE_TABSTRIP)?
  if (!target_browser->is_type_normal()) {
    *error = ExtensionTabUtil::kCanOnlyMoveTabsWithinNormalWindowsError;
    return -1;
  }

  if (target_browser->profile() != source_window->profile()) {
    *error = ExtensionTabUtil::kCanOnlyMoveTabsWithinSameProfileError;
    return -1;
  }

  TabStripModel* target_tab_strip =
      ExtensionTabUtil::GetEditableTabStripModel(target_browser);
  DCHECK(target_tab_strip);

  // Clamp move location to the last position.
  // This is ">" because it can append to a new index position.
  // -1 means set the move location to the last position.
  int target_index = new_index;
  if (target_index > target_tab_strip->count() || target_index < 0) {
    target_index = target_tab_strip->count();
  }

  if (target_tab_strip->SupportsTabGroups()) {
    std::optional<tab_groups::TabGroupId> next_tab_dst_group =
        target_tab_strip->GetTabGroupForTab(target_index);
    std::optional<tab_groups::TabGroupId> prev_tab_dst_group =
        target_tab_strip->GetTabGroupForTab(target_index - 1);

    // Group contiguity is not respected in the target tabstrip.
    if (next_tab_dst_group.has_value() && prev_tab_dst_group.has_value() &&
        next_tab_dst_group == prev_tab_dst_group) {
      *error = tabs_constants::kInvalidTabIndexBreaksGroupContiguity;
      return -1;
    }
  }

  Browser* source_browser = source_window->GetBrowser();
  if (!source_browser) {
    *error = ExtensionTabUtil::kCanOnlyMoveTabsWithinNormalWindowsError;
    return -1;
  }

  std::unique_ptr<TabModel> detached_tab =
      source_browser->tab_strip_model()->DetachTabAtForInsertion(source_index);
  if (!detached_tab) {
    *error = ErrorUtils::FormatErrorMessage(ExtensionTabUtil::kTabNotFoundError,
                                            base::NumberToString(tab_id));
    return -1;
  }

  return target_tab_strip->InsertDetachedTabAt(
      target_index, std::move(detached_tab), AddTabTypes::ADD_NONE);
}

// This function sets the state of the browser window to a "locked"
// fullscreen state (where the user can't exit fullscreen) in response to a
// call to either chrome.windows.create or chrome.windows.update when the
// screen is set locked. This is only necessary for ChromeOS and is
// restricted to allowlisted extensions.
void SetLockedFullscreenState(Browser* browser, bool pinned) {
#if BUILDFLAG(IS_CHROMEOS)
  aura::Window* window = browser->window()->GetNativeWindow();
  DCHECK(window);

  CHECK_NE(GetWindowPinType(window), chromeos::WindowPinType::kPinned)
      << "Extensions only set Trusted Pinned";

  // As this gets triggered from extensions, we might encounter this case.
  if (IsWindowPinned(window) == pinned) {
    return;
  }

  if (pinned) {
    // Pins from extension are always trusted.
    PinWindow(window, /*trusted=*/true);
  } else {
    UnpinWindow(window);
  }

  // Update the set of available browser commands.
  browser->command_controller()->LockedFullscreenStateChanged();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

// Returns whether the given `bounds` intersect with at least 50% of all the
// displays.
bool WindowBoundsIntersectDisplays(const gfx::Rect& bounds) {
  // Bail if `bounds` has an overflown area.
  auto checked_area = bounds.size().GetCheckedArea();
  if (!checked_area.IsValid()) {
    return false;
  }

  int intersect_area = 0;
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    gfx::Rect display_bounds = display.bounds();
    display_bounds.Intersect(bounds);
    intersect_area += display_bounds.size().GetArea();
  }
  return intersect_area >= (bounds.size().GetArea() / 2);
}

class ScopedPinBrowserAtFront {
 public:
  explicit ScopedPinBrowserAtFront(Browser* browser)
      : browser_(browser->AsWeakPtr()) {
    old_z_order_level_ = browser_->window()->GetZOrderLevel();
    browser_->window()->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
  }

  ~ScopedPinBrowserAtFront() {
    if (browser_) {
      browser_->window()->SetZOrderLevel(old_z_order_level_);
    }
  }

 private:
  base::WeakPtr<Browser> browser_;
  ui::ZOrderLevel old_z_order_level_;
};

}  // namespace

// Windows ---------------------------------------------------------------------

ExtensionFunction::ResponseAction WindowsCreateFunction::Run() {
  std::optional<windows::Create::Params> params =
      windows::Create::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  std::vector<GURL> urls;
  int tab_index = -1;

  DCHECK(extension() || source_context_type() == mojom::ContextType::kWebUi ||
         source_context_type() == mojom::ContextType::kUntrustedWebUi);
  std::optional<windows::Create::Params::CreateData>& create_data =
      params->create_data;

  std::optional<web_app::IsolatedWebAppUrlInfo> isolated_web_app_url_info;

  // Look for optional url.
  if (create_data && create_data->url) {
    std::vector<std::string> url_strings;
    // First, get all the URLs the client wants to open.
    if (create_data->url->as_string) {
      url_strings.push_back(std::move(*create_data->url->as_string));
    } else if (create_data->url->as_strings) {
      url_strings = std::move(*create_data->url->as_strings);
    }

    // Second, resolve, validate and convert them to GURLs.
    for (auto& url_string : url_strings) {
      auto url = ExtensionTabUtil::PrepareURLForNavigation(
          url_string, extension(), browser_context());
      if (!url.has_value()) {
        return RespondNow(Error(std::move(url.error())));
      }
      if (url->SchemeIs(chrome::kIsolatedAppScheme)) {
        if (url_strings.size() > 1) {
          return RespondNow(Error(kWindowCreateSupportsOnlySingleIwaUrlError));
        }

        base::expected<web_app::IsolatedWebAppUrlInfo, std::string>
            maybe_url_info = web_app::IsolatedWebAppUrlInfo::Create(*url);
        if (!maybe_url_info.has_value()) {
          return RespondNow(
              Error(base::StringPrintf(kWindowCreateCannotParseIwaUrlError,
                                       maybe_url_info.error().c_str())));
        }
        isolated_web_app_url_info = *maybe_url_info;
      }
      urls.push_back(*url);
    }
  }

  // Decide whether we are opening a normal window or an incognito window.
  std::string error;
  Profile* calling_profile = Profile::FromBrowserContext(browser_context());
  windows_util::IncognitoResult incognito_result =
      windows_util::ShouldOpenIncognitoWindow(
          calling_profile,
          create_data && create_data->incognito
              ? std::optional<bool>(*create_data->incognito)
              : std::nullopt,
          &urls, &error);
  if (incognito_result == windows_util::IncognitoResult::kError) {
    return RespondNow(Error(std::move(error)));
  }

  Profile* window_profile =
      incognito_result == windows_util::IncognitoResult::kIncognito
          ? calling_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)
          : calling_profile;

  // Look for optional tab id.
  WindowController* source_window = nullptr;
  if (create_data && create_data->tab_id) {
    if (isolated_web_app_url_info.has_value()) {
      return RespondNow(Error(kWindowCreateCannotUseTabIdWithIwaError));
    }

    // Find the tab. `tab_index` will later be used to move the tab into the
    // created window.
    content::WebContents* web_contents = nullptr;
    if (!tabs_internal::GetTabById(*create_data->tab_id, calling_profile,
                                   include_incognito_information(),
                                   &source_window, &web_contents, &tab_index,
                                   &error)) {
      return RespondNow(Error(std::move(error)));
    }
    if (!source_window) {
      // The source window can be null for prerender tabs.
      return RespondNow(Error(tabs_constants::kInvalidWindowStateError));
    }

    Browser* source_browser = source_window->GetBrowser();
    if (!source_browser) {
      return RespondNow(
          Error(ExtensionTabUtil::kCanOnlyMoveTabsWithinNormalWindowsError));
    }

    if (web_app::AppBrowserController* controller =
            source_browser->app_controller();
        controller && controller->IsIsolatedWebApp()) {
      return RespondNow(Error(kWindowCreateCannotMoveIwaTabError));
    }

    if (!ExtensionTabUtil::IsTabStripEditable()) {
      return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
    }

    if (source_window->profile() != window_profile) {
      return RespondNow(
          Error(ExtensionTabUtil::kCanOnlyMoveTabsWithinSameProfileError));
    }

    if (DevToolsWindow::IsDevToolsWindow(web_contents)) {
      return RespondNow(Error(tabs_constants::kNotAllowedForDevToolsError));
    }
  }

  if (!IsValidStateForWindowsCreateFunction(base::OptionalToPtr(create_data))) {
    return RespondNow(Error(tabs_constants::kInvalidWindowStateError));
  }

  Browser::Type window_type = Browser::TYPE_NORMAL;

  gfx::Rect window_bounds;
  bool focused = true;
  std::string extension_id;

  if (create_data) {
    // Figure out window type before figuring out bounds so that default
    // bounds can be set according to the window type.
    switch (create_data->type) {
      // TODO(stevenjb): Remove 'panel' from windows.json.
      case windows::CreateType::kPanel:
      case windows::CreateType::kPopup:
        window_type = Browser::TYPE_POPUP;
        if (isolated_web_app_url_info.has_value()) {
          return RespondNow(Error(kInvalidWindowTypeError));
        }
        if (extension()) {
          extension_id = extension()->id();
        }
        break;
      case windows::CreateType::kNone:
      case windows::CreateType::kNormal:
        break;
      default:
        return RespondNow(Error(kInvalidWindowTypeError));
    }

    // Initialize default window bounds according to window type.
    ui::mojom::WindowShowState ignored_show_state =
        ui::mojom::WindowShowState::kDefault;
    WindowSizer::GetBrowserWindowBoundsAndShowState(
        gfx::Rect(), nullptr, &window_bounds, &ignored_show_state);

    // Update the window bounds based on the create parameters.
    bool set_window_position = false;
    bool set_window_size = false;
    if (create_data->left) {
      window_bounds.set_x(*create_data->left);
      set_window_position = true;
    }
    if (create_data->top) {
      window_bounds.set_y(*create_data->top);
      set_window_position = true;
    }
    if (create_data->width) {
      window_bounds.set_width(*create_data->width);
      set_window_size = true;
    }
    if (create_data->height) {
      window_bounds.set_height(*create_data->height);
      set_window_size = true;
    }

    // If the extension specified the window size but no position, adjust the
    // window to fit in the display.
    if (!set_window_position && set_window_size) {
      const display::Display& display =
          display::Screen::GetScreen()->GetDisplayMatching(window_bounds);
      window_bounds.AdjustToFit(display.bounds());
    }

    // Immediately fail if the window bounds don't intersect the displays.
    if ((set_window_position || set_window_size) &&
        !WindowBoundsIntersectDisplays(window_bounds)) {
      return RespondNow(Error(tabs_constants::kInvalidWindowBoundsError));
    }

    if (create_data->focused) {
      focused = *create_data->focused;
    }

    // Record the window height and width to determine if we
    // can set a mininimum value for them (crbug.com/1369103).
    UMA_HISTOGRAM_COUNTS_1000("Extensions.CreateWindowWidth",
                              window_bounds.width());
    UMA_HISTOGRAM_COUNTS_1000("Extensions.CreateWindowHeight",
                              window_bounds.height());
  }

  // Create a new BrowserWindow if possible.
  if (Browser::GetCreationStatusForProfile(window_profile) !=
      Browser::CreationStatus::kOk) {
    return RespondNow(Error(ExtensionTabUtil::kBrowserWindowNotAllowed));
  }
  Browser::CreateParams create_params(window_type, window_profile,
                                      user_gesture());
  if (isolated_web_app_url_info.has_value()) {
    // For Isolated Web Apps, the actual navigating-to URL will be the app's
    // start_url to prevent deep-linking attacks, while the original URL will be
    // accessible via window.launchQueue; for this reason the browser is marked
    // trusted.
    create_params = Browser::CreateParams::CreateForApp(
        web_app::GenerateApplicationNameFromAppId(
            isolated_web_app_url_info->app_id()),
        /*trusted_source=*/true, window_bounds, window_profile, user_gesture());
  } else if (extension_id.empty()) {
    create_params.initial_bounds = window_bounds;
  } else {
    // extension_id is only set for CREATE_TYPE_POPUP.
    create_params = Browser::CreateParams::CreateForAppPopup(
        web_app::GenerateApplicationNameFromAppId(extension_id),
        /*trusted_source=*/false, window_bounds, window_profile,
        user_gesture());
  }
  create_params.initial_show_state = ui::mojom::WindowShowState::kNormal;
  if (create_data && create_data->state != windows::WindowState::kNone) {
    if (create_data->state == windows::WindowState::kLockedFullscreen &&
        !tabs_internal::ExtensionHasLockedFullscreenPermission(extension())) {
      return RespondNow(
          Error(tabs_internal::kMissingLockWindowFullscreenPrivatePermission));
    }
    create_params.initial_show_state =
        ConvertToWindowShowState(create_data->state);
  }

  Browser* new_window = Browser::Create(create_params);
  if (!new_window) {
    return RespondNow(Error(ExtensionTabUtil::kBrowserWindowNotAllowed));
  }

  auto create_nav_params =
      [&](const GURL& url) -> base::expected<NavigateParams, std::string> {
    NavigateParams navigate_params(new_window, url, ui::PAGE_TRANSITION_LINK);
    navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    // Ensure that these navigations will not get 'captured' into PWA windows,
    // as this means that `new_window` could be ignored. It may be
    // useful/desired in the future to allow this behavior, but this may require
    // an API change, or at least a re-write of how these navigations are called
    // to be compatible with the navigation capturing behavior.
    navigate_params.pwa_navigation_capturing_force_off = true;

    // Depending on the |setSelfAsOpener| option, we need to put the new
    // contents in the same BrowsingInstance as their opener.  See also
    // https://crbug.com/713888.
    bool set_self_as_opener = create_data->set_self_as_opener &&  // present?
                              *create_data->set_self_as_opener;  // set to true?
    if (set_self_as_opener) {
      if (is_from_service_worker()) {
        // TODO(crbug.com/40636155): Add test for this.
        return base::unexpected(
            "Cannot specify setSelfAsOpener Service Worker extension.");
      }
      if (isolated_web_app_url_info) {
        return base::unexpected(
            "Cannot specify setSelfAsOpener for isolated-app:// URLs.");
      }
      // TODO(crbug.com/40636155): Add tests for checking opener SiteInstance
      // behavior from a SW based extension's extension frame (e.g. from popup).
      // See ExtensionApiTest.WindowsCreate* tests for details.
      navigate_params.initiator_origin =
          extension() ? extension()->origin()
                      : render_frame_host()->GetLastCommittedOrigin();
      navigate_params.opener = render_frame_host();
      navigate_params.source_site_instance =
          render_frame_host()->GetSiteInstance();
    }

    return navigate_params;
  };

  if (!isolated_web_app_url_info) {
    for (const GURL& url : urls) {
      ASSIGN_OR_RETURN(
          NavigateParams navigate_params, create_nav_params(url),
          [&](const std::string& error) { return RespondNow(Error(error)); });
      Navigate(&navigate_params);
    }
  } else {
    CHECK_EQ(urls.size(), 1U);
    const GURL& original_url = urls[0];

    const webapps::AppId& iwa_id = isolated_web_app_url_info->app_id();
    web_app::WebAppRegistrar& registrar =
        web_app::WebAppProvider::GetForWebApps(window_profile)
            ->registrar_unsafe();

    // TODO(crbug.com/424128443): create an dummy tab in the browser so that the
    // returned window's tab count is always equal to 1 -- this will limit the
    // extension's ability to figure out which IWAs are installed without the
    // `tabs` permission.
    if (registrar.IsIsolated(iwa_id)) {
      ASSIGN_OR_RETURN(
          NavigateParams navigate_params,
          create_nav_params(registrar.GetAppStartUrl(iwa_id)),
          [&](const std::string& error) { return RespondNow(Error(error)); });
      base::WeakPtr<content::NavigationHandle> handle =
          Navigate(&navigate_params);
      CHECK(handle);
      web_app::EnqueueLaunchParams(
          handle->GetWebContents(), iwa_id, original_url,
          /*wait_for_navigation_to_complete=*/true, handle->NavigationStart());
    }
  }

  const TabModel* tab = nullptr;
  // Move the tab into the created window only if it's an empty popup or it's
  // a tabbed window.
  if (window_type == Browser::TYPE_NORMAL || urls.empty()) {
    if (source_window && source_window->GetBrowser()) {
      TabStripModel* source_tab_strip =
          source_window->GetBrowser()->tab_strip_model();
      CHECK(!isolated_web_app_url_info.has_value());
      std::unique_ptr<TabModel> detached_tab =
          source_tab_strip->DetachTabAtForInsertion(tab_index);
      tab = detached_tab.get();
      TabStripModel* target_tab_strip =
          ExtensionTabUtil::GetEditableTabStripModel(new_window);
      if (!target_tab_strip) {
        return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
      }
      target_tab_strip->InsertDetachedTabAt(
          urls.size(), std::move(detached_tab), AddTabTypes::ADD_NONE);
    }
  }
  // Create a new tab if the created window is still empty. Don't create a new
  // tab when it is intended to create an empty popup.
  if (!tab && urls.empty() && window_type == Browser::TYPE_NORMAL) {
    chrome::NewTab(new_window);
  }
  chrome::SelectNumberedTab(
      new_window, 0,
      TabStripUserGestureDetails(
          TabStripUserGestureDetails::GestureType::kNone));

  if (focused) {
    new_window->window()->Show();
  } else {
    // Show an unfocused new window.
    BrowserList* const browser_list = BrowserList::GetInstance();
    Browser* last_active_browser = browser_list->GetLastActive();

    // On some OSes the new unfocused window is shown on top by default.
    // ScopedPinBrowserAtFront prevents the new browser from being shown above
    // the old active browser.
    if (last_active_browser && last_active_browser->IsActive()) {
      ScopedPinBrowserAtFront scoper(last_active_browser);
      new_window->window()->ShowInactive();
    } else {
      new_window->window()->ShowInactive();
    }
  }

// Despite creating the window with initial_show_state() ==
// ui::mojom::WindowShowState::kMinimized above, on Linux the window is not
// created as minimized.
// TODO(crbug.com/40254339): Remove this workaround when linux is fixed.
// TODO(crbug.com/40254339): Find a fix for wayland as well.
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)
  if (new_window->initial_show_state() ==
      ui::mojom::WindowShowState::kMinimized) {
    new_window->window()->Minimize();
  }
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)

  // Lock the window fullscreen only after the new tab has been created
  // (otherwise the tabstrip is empty), and window()->show() has been called
  // (otherwise that resets the locked mode for devices in tablet mode).
  if (create_data &&
      create_data->state == windows::WindowState::kLockedFullscreen) {
    SetLockedFullscreenState(new_window, /*pinned=*/true);
  }

  if (new_window->profile()->IsOffTheRecord() &&
      !browser_context()->IsOffTheRecord() &&
      !include_incognito_information()) {
    // Don't expose incognito windows if extension itself works in non-incognito
    // profile and CanCrossIncognito isn't allowed.
    return RespondNow(WithArguments(base::Value()));
  }

  return RespondNow(
      WithArguments(ExtensionTabUtil::CreateWindowValueForExtension(
          *new_window, extension(), WindowController::kPopulateTabs,
          source_context_type())));
}

ExtensionFunction::ResponseAction WindowsUpdateFunction::Run() {
  std::optional<windows::Update::Params> params =
      windows::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  WindowController* window_controller = nullptr;
  std::string error;
  if (!windows_util::GetControllerFromWindowID(
          this, params->window_id, WindowController::GetAllWindowFilter(),
          &window_controller, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  Browser* browser = window_controller->GetBrowser();
  if (!browser) {
    return RespondNow(Error(ExtensionTabUtil::kNoCrashBrowserError));
  }

  // Don't allow locked fullscreen operations on a window without the proper
  // permission (also don't allow any operations on a locked window if the
  // extension doesn't have the permission).
  const bool is_locked_fullscreen =
      platform_util::IsBrowserLockedFullscreen(browser);
  if ((params->update_info.state == windows::WindowState::kLockedFullscreen ||
       is_locked_fullscreen) &&
      !tabs_internal::ExtensionHasLockedFullscreenPermission(extension())) {
    return RespondNow(
        Error(tabs_internal::kMissingLockWindowFullscreenPrivatePermission));
  }

  // Before changing any of a window's state, validate the update parameters.
  // This prevents Chrome from performing "half" an update.

  // Update the window bounds if the bounds from the update parameters intersect
  // the displays.
  gfx::Rect window_bounds = browser->window()->IsMinimized()
                                ? browser->window()->GetRestoredBounds()
                                : browser->window()->GetBounds();
  bool set_window_bounds = false;
  if (params->update_info.left) {
    window_bounds.set_x(*params->update_info.left);
    set_window_bounds = true;
  }
  if (params->update_info.top) {
    window_bounds.set_y(*params->update_info.top);
    set_window_bounds = true;
  }
  if (params->update_info.width) {
    window_bounds.set_width(*params->update_info.width);
    set_window_bounds = true;
  }
  if (params->update_info.height) {
    window_bounds.set_height(*params->update_info.height);
    set_window_bounds = true;
  }

  if (set_window_bounds && !WindowBoundsIntersectDisplays(window_bounds)) {
    return RespondNow(Error(tabs_constants::kInvalidWindowBoundsError));
  }

  ui::mojom::WindowShowState show_state =
      ConvertToWindowShowState(params->update_info.state);
  if (set_window_bounds &&
      (show_state == ui::mojom::WindowShowState::kMinimized ||
       show_state == ui::mojom::WindowShowState::kMaximized ||
       show_state == ui::mojom::WindowShowState::kFullscreen)) {
    return RespondNow(Error(tabs_constants::kInvalidWindowStateError));
  }

  if (params->update_info.focused) {
    bool focused = *params->update_info.focused;
    // A window cannot be focused and minimized, or not focused and maximized
    // or fullscreened.
    if (focused && show_state == ui::mojom::WindowShowState::kMinimized) {
      return RespondNow(Error(tabs_constants::kInvalidWindowStateError));
    }
    if (!focused && (show_state == ui::mojom::WindowShowState::kMaximized ||
                     show_state == ui::mojom::WindowShowState::kFullscreen)) {
      return RespondNow(Error(tabs_constants::kInvalidWindowStateError));
    }
  }

  // Parameters are valid. Now to perform the actual updates.

  // state will be WINDOW_STATE_NONE if the state parameter wasn't passed from
  // the JS side, and in that case we don't want to change the locked state.
  if (is_locked_fullscreen &&
      params->update_info.state != windows::WindowState::kLockedFullscreen &&
      params->update_info.state != windows::WindowState::kNone) {
    SetLockedFullscreenState(browser, /*pinned=*/false);
  } else if (!is_locked_fullscreen &&
             params->update_info.state ==
                 windows::WindowState::kLockedFullscreen) {
    SetLockedFullscreenState(browser, /*pinned=*/true);
  }

  if (show_state != ui::mojom::WindowShowState::kFullscreen &&
      show_state != ui::mojom::WindowShowState::kDefault) {
    BrowserExtensionWindowController::From(browser)->SetFullscreenMode(
        false, extension()->url());
  }

  switch (show_state) {
    case ui::mojom::WindowShowState::kMinimized:
      browser->window()->Minimize();
      break;
    case ui::mojom::WindowShowState::kMaximized:
      browser->window()->Maximize();
      break;
    case ui::mojom::WindowShowState::kFullscreen:
      if (browser->window()->IsMinimized() ||
          browser->window()->IsMaximized()) {
        browser->window()->Restore();
      }
      BrowserExtensionWindowController::From(browser)->SetFullscreenMode(
          true, extension()->url());
      break;
    case ui::mojom::WindowShowState::kNormal:
      browser->window()->Restore();
      break;
    default:
      break;
  }

  if (set_window_bounds) {
    // TODO(varkha): Updating bounds during a drag can cause problems and a more
    // general solution is needed. See http://crbug.com/251813 .
    browser->window()->SetBounds(window_bounds);
  }

  if (params->update_info.focused) {
    if (*params->update_info.focused) {
      browser->window()->Activate();
    } else {
      browser->window()->Deactivate();
    }
  }

  if (params->update_info.draw_attention) {
    browser->window()->FlashFrame(*params->update_info.draw_attention);
  }

  return RespondNow(
      WithArguments(window_controller->CreateWindowValueForExtension(
          extension(), WindowController::kDontPopulateTabs,
          source_context_type())));
}

// Tabs ------------------------------------------------------------------------

ExtensionFunction::ResponseAction TabsQueryFunction::Run() {
  std::optional<tabs::Query::Params> params =
      tabs::Query::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  bool loading_status_set = params->query_info.status != tabs::TabStatus::kNone;

  URLPatternSet url_patterns;
  if (params->query_info.url) {
    std::vector<std::string> url_pattern_strings;
    if (params->query_info.url->as_string) {
      url_pattern_strings.push_back(*params->query_info.url->as_string);
    } else if (params->query_info.url->as_strings) {
      url_pattern_strings.swap(*params->query_info.url->as_strings);
    }
    // It is o.k. to use URLPattern::SCHEME_ALL here because this function does
    // not grant access to the content of the tabs, only to seeing their URLs
    // and meta data.
    std::string error;
    if (!url_patterns.Populate(url_pattern_strings, URLPattern::SCHEME_ALL,
                               true, &error)) {
      return RespondNow(Error(std::move(error)));
    }
  }

  std::string title = params->query_info.title.value_or(std::string());

  int window_id = extension_misc::kUnknownWindowId;
  if (params->query_info.window_id) {
    window_id = *params->query_info.window_id;
  }

  std::optional<int> group_id = std::nullopt;
  if (params->query_info.group_id) {
    group_id = *params->query_info.group_id;
  }

  std::optional<int> split_id = std::nullopt;
  if (params->query_info.split_view_id) {
    split_id = *params->query_info.split_view_id;
  }

  int index = -1;
  if (params->query_info.index) {
    index = *params->query_info.index;
  }

  std::string window_type;
  if (params->query_info.window_type != tabs::WindowType::kNone) {
    window_type = tabs::ToString(params->query_info.window_type);
  }

  base::Value::List result;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  BrowserWindowInterface* last_active_browser =
      GetLastActiveBrowserWithProfile(profile, include_incognito_information());

  // Note that the current browser is allowed to be null: you can still query
  // the tabs in this case.
  BrowserWindowInterface* current_browser = nullptr;
  WindowController* current_window_controller =
      ChromeExtensionFunctionDetails(this).GetCurrentWindowController();
  if (current_window_controller) {
    current_browser = current_window_controller->GetBrowserWindowInterface();
    // Note: current_browser may still be null.
  }

  // Historically, we queried browsers in creation order. Maintain that behavior
  // (for now).
  std::vector<BrowserWindowInterface*> all_browsers =
      GetAllBrowserWindowInterfaces();
  for (auto* browser : all_browsers) {
    if (!profile->IsSameOrParent(browser->GetProfile())) {
      continue;
    }

    if (!browser->GetWindow()) {
      continue;
    }

    if (!include_incognito_information() && profile != browser->GetProfile()) {
      continue;
    }

    WindowController* window_controller =
        BrowserExtensionWindowController::From(browser);
    CHECK(window_controller);
    if (!window_controller->IsVisibleToTabsAPIForExtension(
            extension(), /*allow_dev_tools_windows=*/false)) {
      continue;
    }

    if (window_id >= 0 && window_id != ExtensionTabUtil::GetWindowId(browser)) {
      continue;
    }

    if (window_id == extension_misc::kCurrentWindowId &&
        browser != current_browser) {
      continue;
    }

    if (!MatchesBool(params->query_info.current_window,
                     browser == current_browser)) {
      continue;
    }

    if (!MatchesBool(params->query_info.last_focused_window,
                     browser == last_active_browser)) {
      continue;
    }

    if (!window_type.empty() &&
        window_type != window_controller->GetWindowTypeText()) {
      continue;
    }

    TabListInterface* tab_list = TabListInterface::From(browser);
    for (int i = 0; i < tab_list->GetTabCount(); ++i) {
      if (index > -1 && i != index) {
        continue;
      }

      ::tabs::TabInterface* tab = tab_list->GetTab(i);
      CHECK(tab);
      content::WebContents* web_contents = tab->GetContents();

      if (!web_contents) {
        continue;
      }

      if (!MatchesBool(params->query_info.highlighted, tab->IsSelected())) {
        continue;
      }

      if (!MatchesBool(params->query_info.active, tab->IsActivated())) {
        continue;
      }

      if (!MatchesBool(params->query_info.pinned, tab->IsPinned())) {
        continue;
      }

      if (group_id.has_value()) {
        std::optional<tab_groups::TabGroupId> group = tab->GetGroup();
        if (group_id.value() == -1) {
          if (group.has_value()) {
            continue;
          }
        } else if (!group.has_value()) {
          continue;
        } else if (ExtensionTabUtil::GetGroupId(group.value()) !=
                   group_id.value()) {
          continue;
        }
      }

      if (split_id.has_value()) {
        std::optional<split_tabs::SplitTabId> split = tab->GetSplit();
        if (split_id.value() == -1) {
          if (split.has_value()) {
            continue;
          }
        } else if (!split.has_value() ||
                   ExtensionTabUtil::GetSplitId(split.value()) !=
                       split_id.value()) {
          continue;
        }
      }

      auto* audible_helper =
          RecentlyAudibleHelper::FromWebContents(web_contents);
      if (!MatchesBool(params->query_info.audible,
                       audible_helper->WasRecentlyAudible())) {
        continue;
      }

      auto* tab_lifecycle_unit_external =
          resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
              web_contents);

      if (!MatchesBool(params->query_info.frozen,
                       tab_lifecycle_unit_external->GetTabState() ==
                           ::mojom::LifecycleUnitState::FROZEN)) {
        continue;
      }

      if (!MatchesBool(params->query_info.discarded,
                       tab_lifecycle_unit_external->GetTabState() ==
                           ::mojom::LifecycleUnitState::DISCARDED)) {
        continue;
      }

      if (!MatchesBool(params->query_info.auto_discardable,
                       tab_lifecycle_unit_external->IsAutoDiscardable())) {
        continue;
      }

      if (!MatchesBool(params->query_info.muted,
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
                mojom::APIPermissionID::kTab) &&
            !extension_->permissions_data()->HasHostPermission(
                web_contents->GetURL())) {
          continue;
        }

        if (!title.empty() && !base::MatchPattern(web_contents->GetTitle(),
                                                  base::UTF8ToUTF16(title))) {
          continue;
        }

        if (!url_patterns.is_empty() &&
            !url_patterns.MatchesURL(web_contents->GetURL())) {
          continue;
        }
      }

      if (loading_status_set &&
          params->query_info.status !=
              ExtensionTabUtil::GetLoadingStatus(web_contents)) {
        continue;
      }

      result.Append(
          tabs_internal::CreateTabObjectHelper(
              web_contents, extension(), source_context_type(), browser, i)
              .ToValue());
    }
  }

  return RespondNow(WithArguments(std::move(result)));
}

ExtensionFunction::ResponseAction TabsCreateFunction::Run() {
  std::optional<tabs::Create::Params> params =
      tabs::Create::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow([&] {
    if (!ExtensionTabUtil::IsTabStripEditable()) {
      return Error(ExtensionTabUtil::kTabStripNotEditableError);
    }

    ExtensionTabUtil::OpenTabParams options;
    options.window_id = params->create_properties.window_id;
    options.opener_tab_id = params->create_properties.opener_tab_id;
    options.active = params->create_properties.selected;
    // The 'active' property has replaced the 'selected' property.
    options.active = params->create_properties.active;
    options.pinned = params->create_properties.pinned;
    options.index = params->create_properties.index;
    options.url = params->create_properties.url;

    auto result = ExtensionTabUtil::OpenTab(this, options, user_gesture());
    if (!result.has_value()) {
      return Error(result.error());
    }

#if BUILDFLAG(FULL_SAFE_BROWSING)
    tabs_internal::NotifyExtensionTelemetry(
        Profile::FromBrowserContext(browser_context()), extension(),
        safe_browsing::TabsApiInfo::CREATE,
        /*current_url=*/std::string(), options.url.value_or(std::string()),
        js_callstack());
#endif

    // Return data about the newly created tab.
    return has_callback() ? WithArguments(std::move(*result)) : NoArguments();
  }());
}

ExtensionFunction::ResponseAction TabsDuplicateFunction::Run() {
  std::optional<tabs::Duplicate::Params> params =
      tabs::Duplicate::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!ExtensionTabUtil::IsTabStripEditable()) {
    return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
  }
  int tab_id = params->tab_id;

  WindowController* window = nullptr;
  int tab_index = -1;
  std::string error;
  if (!tabs_internal::GetTabById(tab_id, browser_context(),
                                 include_incognito_information(), &window,
                                 nullptr, &tab_index, &error)) {
    return RespondNow(Error(std::move(error)));
  }
  if (!window) {
    return RespondNow(Error(tabs_constants::kInvalidWindowStateError));
  }
  Browser* browser = window->GetBrowser();

  if (!browser || !ExtensionTabUtil::IsTabStripEditable()) {
    return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
  }

  if (!chrome::CanDuplicateTabAt(browser, tab_index)) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        tabs_constants::kCannotDuplicateTab, base::NumberToString(tab_id))));
  }

  WebContents* new_contents = chrome::DuplicateTabAt(browser, tab_index);
  if (!new_contents) {
    return RespondNow(Error(kUnknownErrorDoNotUse));
  }

  if (!has_callback()) {
    return RespondNow(NoArguments());
  }

  // Duplicated tab may not be in the same window as the original, so find
  // the new window.
  TabListInterface* new_tab_list = nullptr;
  int new_tab_index = -1;
  if (!ExtensionTabUtil::GetTabListInterface(*new_contents, &new_tab_list,
                                             &new_tab_index)) {
    return RespondNow(Error(kUnknownErrorDoNotUse));
  }

  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(extension(), source_context_type(),
                                            new_contents);
  return RespondNow(
      ArgumentList(tabs::Get::Results::Create(ExtensionTabUtil::CreateTabObject(
          new_contents, scrub_tab_behavior, extension(), new_tab_list,
          new_tab_index))));
}

ExtensionFunction::ResponseAction TabsHighlightFunction::Run() {
  std::optional<tabs::Highlight::Params> params =
      tabs::Highlight::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Get the window id from the params; default to current window if omitted.
  int window_id = params->highlight_info.window_id.value_or(
      extension_misc::kCurrentWindowId);

  std::string error;
  WindowController* window_controller =
      ExtensionTabUtil::GetControllerFromWindowID(
          ChromeExtensionFunctionDetails(this), window_id, &error);
  if (!window_controller) {
    return RespondNow(Error(std::move(error)));
  }

  // Don't let the extension update the tab if the user is dragging tabs.
  TabStripModel* tab_strip_model = ExtensionTabUtil::GetEditableTabStripModel(
      window_controller->GetBrowser());
  if (!tab_strip_model) {
    return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
  }
  ui::ListSelectionModel selection;
  std::optional<size_t> active_index;

  if (params->highlight_info.tabs.as_integers) {
    std::vector<int>& tab_indices = *params->highlight_info.tabs.as_integers;
    // Create a new selection model as we read the list of tab indices.
    for (int tab_index : tab_indices) {
      if (!HighlightTab(tab_strip_model, &selection, &active_index, tab_index,
                        &error)) {
        return RespondNow(Error(std::move(error)));
      }
    }
  } else {
    EXTENSION_FUNCTION_VALIDATE(params->highlight_info.tabs.as_integer);
    if (!HighlightTab(tab_strip_model, &selection, &active_index,
                      *params->highlight_info.tabs.as_integer, &error)) {
      return RespondNow(Error(std::move(error)));
    }
  }

  // Make sure they actually specified tabs to select.
  if (selection.empty()) {
    return RespondNow(Error(kNoHighlightedTabError));
  }

  // Extend selection for any split tabs.
  for (const auto& index : selection.selected_indices()) {
    std::optional<split_tabs::SplitTabId> split_id =
        tab_strip_model->GetSplitForTab(index);
    if (!split_id.has_value()) {
      continue;
    }
    // All the tabs in a split should be contiguous.
    std::vector<::tabs::TabInterface*> split_tabs =
        tab_strip_model->GetSplitData(split_id.value())->ListTabs();
    size_t start = tab_strip_model->GetIndexOfTab(split_tabs[0]);
    selection.AddIndexRangeToSelection(start, start + split_tabs.size() - 1);
  }

  selection.set_active(active_index);
  tab_strip_model->SetSelectionFromModel(std::move(selection));
  return RespondNow(
      WithArguments(window_controller->CreateWindowValueForExtension(
          extension(), WindowController::kPopulateTabs,
          source_context_type())));
}

bool TabsHighlightFunction::HighlightTab(TabStripModel* tabstrip,
                                         ui::ListSelectionModel* selection,
                                         std::optional<size_t>* active_index,
                                         int index,
                                         std::string* error) {
  // Make sure the index is in range.
  if (!tabstrip->ContainsIndex(index)) {
    *error = ErrorUtils::FormatErrorMessage(kTabIndexNotFoundError,
                                            base::NumberToString(index));
    return false;
  }

  // By default, we make the first tab in the list active.
  if (!active_index->has_value()) {
    *active_index = static_cast<size_t>(index);
  }

  selection->AddIndexToSelection(index);
  return true;
}

TabsUpdateFunction::TabsUpdateFunction() : web_contents_(nullptr) {}

ExtensionFunction::ResponseAction TabsUpdateFunction::Run() {
  std::optional<tabs::Update::Params> params =
      tabs::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = -1;
  WebContents* contents = nullptr;
  if (!params->tab_id) {
    WindowController* window_controller =
        ChromeExtensionFunctionDetails(this).GetCurrentWindowController();
    if (!window_controller) {
      return RespondNow(Error(ExtensionTabUtil::kNoCurrentWindowError));
    }
    if (!ExtensionTabUtil::IsTabStripEditable()) {
      return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
    }
    contents = window_controller->GetActiveTab();
    if (!contents) {
      return RespondNow(Error(tabs_constants::kNoSelectedTabError));
    }
    tab_id = ExtensionTabUtil::GetTabId(contents);
  } else {
    tab_id = *params->tab_id;
  }

  int tab_index = -1;
  WindowController* window = nullptr;
  std::string error;
  if (!tabs_internal::GetTabById(tab_id, browser_context(),
                                 include_incognito_information(), &window,
                                 &contents, &tab_index, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  if (DevToolsWindow::IsDevToolsWindow(contents)) {
    return RespondNow(Error(tabs_constants::kNotAllowedForDevToolsError));
  }

  // tabs_internal::GetTabById may return a null window for prerender tabs.
  if (!window || !window->SupportsTabs()) {
    return RespondNow(Error(ExtensionTabUtil::kNoCurrentWindowError));
  }
  Browser* browser = window->GetBrowser();
  TabStripModel* tab_strip = browser->tab_strip_model();

  web_contents_ = contents;

  bool active = false;
  // TODO(rafaelw): Setting |active| from js doesn't make much sense.
  // Move tab selection management up to window.
  if (params->update_properties.selected) {
    active = *params->update_properties.selected;
  }

  // The 'active' property has replaced 'selected'.
  if (params->update_properties.active) {
    active = *params->update_properties.active;
  }

  if (active) {
    // Bug fix for crbug.com/1197888. Don't let the extension update the tab
    // if the user is dragging tabs.
    if (!ExtensionTabUtil::IsTabStripEditable()) {
      return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
    }

    if (tab_strip->active_index() != tab_index) {
      tab_strip->ActivateTabAt(tab_index);
      DCHECK_EQ(contents, tab_strip->GetActiveWebContents());
    }
  }

  if (params->update_properties.highlighted) {
    // Bug fix for crbug.com/1197888. Don't let the extension update the tab
    // if the user is dragging tabs.
    if (!ExtensionTabUtil::IsTabStripEditable()) {
      return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
    }

    if (*params->update_properties.highlighted) {
      tab_strip->SelectTabAt(tab_index);
    } else {
      tab_strip->DeselectTabAt(tab_index);
    }
  }

  if (params->update_properties.muted &&
      !SetTabAudioMuted(contents, *params->update_properties.muted,
                        TabMutedReason::EXTENSION, extension()->id())) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        kCannotUpdateMuteCaptured, base::NumberToString(tab_id))));
  }

  if (params->update_properties.opener_tab_id) {
    int opener_id = *params->update_properties.opener_tab_id;
    WebContents* opener_contents = nullptr;
    if (opener_id == tab_id) {
      return RespondNow(Error("Cannot set a tab's opener to itself."));
    }
    if (!ExtensionTabUtil::GetTabById(opener_id, browser_context(),
                                      include_incognito_information(),
                                      &opener_contents)) {
      return RespondNow(Error(
          ErrorUtils::FormatErrorMessage(ExtensionTabUtil::kTabNotFoundError,
                                         base::NumberToString(opener_id))));
    }

    // Bug fix for crbug.com/1197888. Don't let the extension update the tab
    // if the user is dragging tabs.
    if (!ExtensionTabUtil::IsTabStripEditable()) {
      return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
    }

    if (tab_strip->GetIndexOfWebContents(opener_contents) ==
        TabStripModel::kNoTab) {
      return RespondNow(
          Error("Tab opener must be in the same window as the updated tab."));
    }
    tab_strip->SetOpenerOfWebContentsAt(tab_index, opener_contents);
  }

  if (params->update_properties.auto_discardable) {
    bool state = *params->update_properties.auto_discardable;
    resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
        web_contents_)
        ->SetAutoDiscardable(state);
  }

  if (params->update_properties.pinned) {
    // Bug fix for crbug.com/1197888. Don't let the extension update the tab if
    // the user is dragging tabs.
    if (!ExtensionTabUtil::IsTabStripEditable()) {
      return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
    }

    bool pinned = *params->update_properties.pinned;
    tab_strip->SetTabPinned(tab_index, pinned);

    // Update the tab index because it may move when being pinned.
    tab_index = tab_strip->GetIndexOfWebContents(contents);
  }

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  // Navigate the tab to a new location if the url is different.
  if (params->update_properties.url) {
    std::string updated_url = *params->update_properties.url;
    if (browser->profile()->IsIncognitoProfile() &&
        !IsURLAllowedInIncognito(GURL(updated_url))) {
      return RespondNow(Error(ErrorUtils::FormatErrorMessage(
          tabs_constants::kURLsNotAllowedInIncognitoError, updated_url)));
    }

    // Get last committed or pending URL.
    std::string current_url = contents->GetVisibleURL().is_valid()
                                  ? contents->GetVisibleURL().spec()
                                  : std::string();

    if (!UpdateURL(updated_url, tab_id, &error)) {
      return RespondNow(Error(std::move(error)));
    }

#if BUILDFLAG(FULL_SAFE_BROWSING)
    tabs_internal::NotifyExtensionTelemetry(
        Profile::FromBrowserContext(browser_context()), extension(),
        safe_browsing::TabsApiInfo::UPDATE, current_url, updated_url,
        js_callstack());
#endif
  }

  return RespondNow(GetResult());
}

bool TabsUpdateFunction::UpdateURL(const std::string& url_string,
                                   int tab_id,
                                   std::string* error) {
  auto url = ExtensionTabUtil::PrepareURLForNavigation(url_string, extension(),
                                                       browser_context());
  if (!url.has_value()) {
    *error = std::move(url.error());
    return false;
  }

  NavigationController::LoadURLParams load_params(*url);

  // Treat extension-initiated navigations as renderer-initiated so that the URL
  // does not show in the omnibox until it commits.  This avoids URL spoofs
  // since URLs can be opened on behalf of untrusted content.
  load_params.is_renderer_initiated = true;
  // All renderer-initiated navigations need to have an initiator origin.
  load_params.initiator_origin = extension()->origin();
  // |source_site_instance| needs to be set so that a renderer process
  // compatible with |initiator_origin| is picked by Site Isolation.
  load_params.source_site_instance = content::SiteInstance::CreateForURL(
      web_contents_->GetBrowserContext(),
      load_params.initiator_origin->GetURL());

  // Marking the navigation as initiated via an API means that the focus
  // will stay in the omnibox - see https://crbug.com/1085779.
  load_params.transition_type = ui::PAGE_TRANSITION_FROM_API;

  base::WeakPtr<content::NavigationHandle> navigation_handle =
      web_contents_->GetController().LoadURLWithParams(load_params);
  // Navigation can fail for any number of reasons at the content layer.
  // Unfortunately, we can't provide a detailed error message here, because
  // there are too many possible triggers. At least notify the extension that
  // the update failed.
  if (!navigation_handle) {
    *error = "Navigation rejected.";
    return false;
  }

  DCHECK_EQ(*url,
            web_contents_->GetController().GetPendingEntry()->GetVirtualURL());

  return true;
}

ExtensionFunction::ResponseValue TabsUpdateFunction::GetResult() {
  if (!has_callback()) {
    return NoArguments();
  }

  return ArgumentList(
      tabs::Get::Results::Create(tabs_internal::CreateTabObjectHelper(
          web_contents_, extension(), source_context_type(), nullptr, -1)));
}

ExtensionFunction::ResponseAction TabsMoveFunction::Run() {
  std::optional<tabs::Move::Params> params = tabs::Move::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int new_index = params->move_properties.index;
  const auto& window_id = params->move_properties.window_id;
  base::Value::List tab_values;

  size_t num_tabs = 0;
  std::string error;
  if (params->tab_ids.as_integers) {
    std::vector<int>& tab_ids = *params->tab_ids.as_integers;
    num_tabs = tab_ids.size();

    for (int tab_id : tab_ids) {
      if (!MoveTab(tab_id, &new_index, tab_values, window_id, &error)) {
        return RespondNow(Error(std::move(error)));
      }
    }
  } else {
    EXTENSION_FUNCTION_VALIDATE(params->tab_ids.as_integer);
    num_tabs = 1;
    if (!MoveTab(*params->tab_ids.as_integer, &new_index, tab_values, window_id,
                 &error)) {
      return RespondNow(Error(std::move(error)));
    }
  }

  // TODO(devlin): It's weird that whether or not the method provides a callback
  // can determine its success (as we return errors below).
  if (!has_callback()) {
    return RespondNow(NoArguments());
  }

  if (num_tabs == 0) {
    return RespondNow(Error("No tabs given."));
  }
  if (num_tabs == 1) {
    CHECK_EQ(1u, tab_values.size());
    return RespondNow(WithArguments(std::move(tab_values[0])));
  }

  // Return the results as an array if there are multiple tabs.
  return RespondNow(WithArguments(std::move(tab_values)));
}

bool TabsMoveFunction::MoveTab(int tab_id,
                               int* new_index,
                               base::Value::List& tab_values,
                               const std::optional<int>& window_id,
                               std::string* error) {
  WindowController* source_window = nullptr;
  WebContents* contents = nullptr;
  int tab_index = -1;
  if (!tabs_internal::GetTabById(
          tab_id, browser_context(), include_incognito_information(),
          &source_window, &contents, &tab_index, error) ||
      !source_window) {
    return false;
  }

  if (DevToolsWindow::IsDevToolsWindow(contents)) {
    *error = tabs_constants::kNotAllowedForDevToolsError;
    return false;
  }

  // Don't let the extension move the tab if the user is dragging tabs.
  if (!ExtensionTabUtil::IsTabStripEditable()) {
    *error = ExtensionTabUtil::kTabStripNotEditableError;
    return false;
  }

  if (window_id && *window_id != ExtensionTabUtil::GetWindowIdOfTab(contents)) {
    WindowController* target_controller =
        ExtensionTabUtil::GetControllerFromWindowID(
            ChromeExtensionFunctionDetails(this), *window_id, error);
    if (!target_controller) {
      return false;
    }

    Browser* target_browser = target_controller->GetBrowser();
    int inserted_index =
        MoveTabToWindow(this, tab_id, target_browser, *new_index, error);
    if (inserted_index < 0) {
      return false;
    }

    *new_index = inserted_index;

    if (has_callback()) {
      content::WebContents* web_contents =
          target_controller->GetWebContentsAt(inserted_index);

      tab_values.Append(tabs_internal::CreateTabObjectHelper(
                            web_contents, extension(), source_context_type(),
                            target_browser, inserted_index)
                            .ToValue());
    }

    // Insert the tabs one after another.
    *new_index += 1;

    return true;
  }

  // Perform a simple within-window move.
  // Clamp move location to the last position.
  // This is ">=" because the move must be to an existing location.
  // -1 means set the move location to the last position.
  TabStripModel* source_tab_strip =
      source_window->GetBrowser()->tab_strip_model();
  if (*new_index >= source_tab_strip->count() || *new_index < 0) {
    *new_index = source_tab_strip->count() - 1;
  }

  if (*new_index != tab_index) {
    *new_index =
        source_tab_strip->MoveWebContentsAt(tab_index, *new_index, false);
  }

  if (has_callback()) {
    tab_values.Append(tabs_internal::CreateTabObjectHelper(
                          contents, extension(), source_context_type(),
                          source_window->GetBrowserWindowInterface(),
                          *new_index)
                          .ToValue());
  }

  // Insert the tabs one after another.
  *new_index += 1;

  return true;
}

ExtensionFunction::ResponseAction TabsReloadFunction::Run() {
  std::optional<tabs::Reload::Params> params =
      tabs::Reload::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  bool bypass_cache = false;
  if (params->reload_properties && params->reload_properties->bypass_cache) {
    bypass_cache = *params->reload_properties->bypass_cache;
  }

  // If |tab_id| is specified, look for it. Otherwise default to selected tab
  // in the current window.
  content::WebContents* web_contents = nullptr;
  if (!params->tab_id) {
    if (WindowController* window_controller =
            ChromeExtensionFunctionDetails(this).GetCurrentWindowController()) {
      web_contents = window_controller->GetActiveTab();
      if (!web_contents) {
        return RespondNow(Error(tabs_constants::kNoSelectedTabError));
      }
    } else {
      return RespondNow(Error(ExtensionTabUtil::kNoCurrentWindowError));
    }
  } else {
    int tab_id = *params->tab_id;

    std::string error;
    if (!tabs_internal::GetTabById(tab_id, browser_context(),
                                   include_incognito_information(), nullptr,
                                   &web_contents, nullptr, &error)) {
      return RespondNow(Error(std::move(error)));
    }
  }

  web_contents->GetController().Reload(
      bypass_cache ? content::ReloadType::BYPASSING_CACHE
                   : content::ReloadType::NORMAL,
      true);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsGroupFunction::Run() {
  std::optional<tabs::Group::Params> params =
      tabs::Group::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;

  // Get the target browser from the parameters.
  int group_id = -1;
  WindowController* target_window = nullptr;
  tab_groups::TabGroupId group = tab_groups::TabGroupId::CreateEmpty();
  if (params->options.group_id) {
    if (params->options.create_properties) {
      return RespondNow(Error(tabs_constants::kGroupParamsError));
    }

    group_id = *params->options.group_id;
    if (!ExtensionTabUtil::GetGroupById(
            group_id, browser_context(), include_incognito_information(),
            &target_window, &group, nullptr, &error)) {
      return RespondNow(Error(std::move(error)));
    }
  } else {
    int window_id = extension_misc::kCurrentWindowId;
    if (params->options.create_properties &&
        params->options.create_properties->window_id) {
      window_id = *params->options.create_properties->window_id;
    }
    target_window = ExtensionTabUtil::GetControllerFromWindowID(
        ChromeExtensionFunctionDetails(this), window_id, &error);
    if (!target_window) {
      return RespondNow(Error(std::move(error)));
    }
  }

  DCHECK(target_window);
  if (!target_window->HasEditableTabStrip()) {
    return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
  }

  // Get all tab IDs from parameters.
  std::vector<int> tab_ids;
  if (params->options.tab_ids.as_integers) {
    tab_ids = *params->options.tab_ids.as_integers;
    EXTENSION_FUNCTION_VALIDATE(!tab_ids.empty());
  } else {
    EXTENSION_FUNCTION_VALIDATE(params->options.tab_ids.as_integer);
    tab_ids.push_back(*params->options.tab_ids.as_integer);
  }

  // Get each tab's current window. All tabs will need to be moved into the
  // target window before grouping.
  std::vector<WindowController*> tab_windows;
  tab_windows.reserve(tab_ids.size());
  for (int tab_id : tab_ids) {
    WindowController* tab_window = nullptr;
    content::WebContents* web_contents = nullptr;
    if (!tabs_internal::GetTabById(tab_id, browser_context(),
                                   include_incognito_information(), &tab_window,
                                   &web_contents, nullptr, &error)) {
      return RespondNow(Error(std::move(error)));
    }
    if (tab_window) {
      tab_windows.push_back(tab_window);
    }

    if (DevToolsWindow::IsDevToolsWindow(web_contents)) {
      return RespondNow(Error(tabs_constants::kNotAllowedForDevToolsError));
    }
  }

  // Move all tabs to the target browser, appending to the end each time. Only
  // tabs that are not already in the target browser are moved.
  for (size_t i = 0; i < tab_ids.size(); ++i) {
    if (tab_windows[i] != target_window) {
      if (MoveTabToWindow(this, tab_ids[i], target_window->GetBrowser(), -1,
                          &error) < 0) {
        return RespondNow(Error(std::move(error)));
      }
    }
  }

  // Get the resulting tab indices in the target browser. We recalculate these
  // after all tabs are moved so that any callbacks are resolved and the indices
  // are final.
  std::vector<int> tab_indices;
  tab_indices.reserve(tab_ids.size());
  for (int tab_id : tab_ids) {
    int tab_index = -1;
    if (!tabs_internal::GetTabById(
            tab_id, browser_context(), include_incognito_information(),
            /*window_out=*/nullptr, /*contents_out=*/nullptr, &tab_index,
            &error)) {
      return RespondNow(Error(std::move(error)));
    }
    tab_indices.push_back(tab_index);
  }
  // Sort and dedupe these indices for processing in the tabstrip.
  std::sort(tab_indices.begin(), tab_indices.end());
  tab_indices.erase(std::unique(tab_indices.begin(), tab_indices.end()),
                    tab_indices.end());

  // Get the remaining group metadata and add the tabs to the group.
  // At this point, we assume this is a valid action due to the checks above.
  if (!ExtensionTabUtil::IsTabStripEditable()) {
    return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
  }
  TabStripModel* tab_strip = target_window->GetBrowser()->tab_strip_model();
  if (!tab_strip->SupportsTabGroups()) {
    return RespondNow(
        Error(ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError));
  }
  if (group.is_empty()) {
    group = tab_strip->AddToNewGroup(tab_indices);
    group_id = ExtensionTabUtil::GetGroupId(group);
  } else {
    tab_strip->AddToExistingGroup(tab_indices, group);
  }

  DCHECK_GT(group_id, 0);

  return RespondNow(WithArguments(group_id));
}

ExtensionFunction::ResponseAction TabsUngroupFunction::Run() {
  std::optional<tabs::Ungroup::Params> params =
      tabs::Ungroup::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<int> tab_ids;
  if (params->tab_ids.as_integers) {
    tab_ids = *params->tab_ids.as_integers;
    EXTENSION_FUNCTION_VALIDATE(!tab_ids.empty());
  } else {
    EXTENSION_FUNCTION_VALIDATE(params->tab_ids.as_integer);
    tab_ids.push_back(*params->tab_ids.as_integer);
  }

  std::string error;
  for (int tab_id : tab_ids) {
    if (!UngroupTab(tab_id, &error)) {
      return RespondNow(Error(std::move(error)));
    }
  }

  return RespondNow(NoArguments());
}

bool TabsUngroupFunction::UngroupTab(int tab_id, std::string* error) {
  WindowController* window = nullptr;
  int tab_index = -1;
  if (!tabs_internal::GetTabById(tab_id, browser_context(),
                                 include_incognito_information(), &window,
                                 nullptr, &tab_index, error) ||
      !window) {
    return false;
  }

  if (!window->HasEditableTabStrip()) {
    *error = ExtensionTabUtil::kTabStripNotEditableError;
    return false;
  }

  TabStripModel* tab_strip_model = window->GetBrowser()->tab_strip_model();
  if (!tab_strip_model->SupportsTabGroups()) {
    *error = ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError;
    return false;
  }

  tab_strip_model->RemoveFromGroup({tab_index});

  return true;
}

ExtensionFunction::ResponseAction TabsDiscardFunction::Run() {
  std::optional<tabs::Discard::Params> params =
      tabs::Discard::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  WebContents* contents = nullptr;
  // If |tab_id| is given, find the web_contents respective to it.
  // Otherwise invoke discard function in TabManager with null web_contents
  // that will discard the least important tab.
  if (params->tab_id) {
    int tab_id = *params->tab_id;
    std::string error;
    if (!tabs_internal::GetTabById(tab_id, browser_context(),
                                   include_incognito_information(), nullptr,
                                   &contents, nullptr, &error)) {
      return RespondNow(Error(std::move(error)));
    }

    if (DevToolsWindow::IsDevToolsWindow(contents)) {
      return RespondNow(Error(tabs_constants::kNotAllowedForDevToolsError));
    }
  }

  // Discard the tab.
  contents =
      g_browser_process->GetTabManager()->DiscardTabByExtension(contents);

  // Create the Tab object and return it in case of success.
  if (!contents) {
    // Return appropriate error message otherwise.
    return RespondNow(Error(params->tab_id
                                ? ErrorUtils::FormatErrorMessage(
                                      tabs_constants::kCannotDiscardTab,
                                      base::NumberToString(*params->tab_id))
                                : kCannotFindTabToDiscard));
  }

  return RespondNow(ArgumentList(
      tabs::Discard::Results::Create(tabs_internal::CreateTabObjectHelper(
          contents, extension(), source_context_type(), nullptr, -1))));
}

TabsDiscardFunction::TabsDiscardFunction() = default;
TabsDiscardFunction::~TabsDiscardFunction() = default;

}  // namespace extensions
