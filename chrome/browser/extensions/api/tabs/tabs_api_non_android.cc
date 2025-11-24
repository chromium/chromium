// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <numeric>
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
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
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
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/incognito_allowed_url.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_muted_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
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
#include "components/webapps/isolated_web_apps/scheme.h"
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
#include "ui/base/base_window.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "ash/wm/window_pin_util.h"
#include "chrome/browser/ash/boca/on_task/locked_quiz_session_manager_factory.h"
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

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kWindowCreateLockedFullscreenUrlCountMismatchError[] =
    "When creating a new window in locked fullscreen mode, exactly one URL "
    "should be supplied.";
#endif  // BUILDFLAG(IS_CHROMEOS)

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

class ScopedPinBrowserAtFront {
 public:
  explicit ScopedPinBrowserAtFront(BrowserWindowInterface* bwi)
      : bwi_(bwi->GetWeakPtr()) {
    old_z_order_level_ = bwi->GetWindow()->GetZOrderLevel();
    bwi->GetWindow()->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
  }

  ~ScopedPinBrowserAtFront() {
    if (bwi_) {
      bwi_->GetWindow()->SetZOrderLevel(old_z_order_level_);
    }
  }

 private:
  base::WeakPtr<BrowserWindowInterface> bwi_;
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
      if (url->SchemeIs(webapps::kIsolatedAppScheme)) {
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

  if (!IsValidStateForWindowsCreateFunction(base::OptionalToPtr(create_data))) {
    return RespondNow(Error(tabs_constants::kInvalidWindowStateError));
  }

  // Look for optional tab id.
  bool is_locked_fullscreen =
      create_data &&
      create_data->state == windows::WindowState::kLockedFullscreen;
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

    // Validate the tab information. Return an error if it's not valid.
    std::string tab_error =
        ValidateTab(source_window, window_profile, calling_profile,
                    web_contents, is_locked_fullscreen, urls);
    if (!tab_error.empty()) {
      return RespondNow(Error(std::move(tab_error)));
    }
  }

  if (is_locked_fullscreen) {
    if (!tabs_internal::ExtensionHasLockedFullscreenPermission(extension())) {
      return RespondNow(
          Error(tabs_internal::kMissingLockWindowFullscreenPrivatePermission));
    }

#if BUILDFLAG(IS_CHROMEOS)
    // Set up and launch the OnTask system web app if applicable. The legacy
    // setup leverages a regular browser instance today.
    if (ash::features::IsBocaOnTaskLockedQuizMigrationEnabled()) {
      if (urls.size() != 1) {
        return RespondNow(
            Error(kWindowCreateLockedFullscreenUrlCountMismatchError));
      }
      ash::boca::LockedQuizSessionManagerFactory::GetInstance()
          ->GetForBrowserContext(calling_profile)
          ->OpenLockedQuiz(
              urls.front(),
              base::BindOnce(
                  &WindowsCreateFunction::OnWindowCreatedAsynchronously, this));
      return RespondLater();
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
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
    std::string bounds_error = SetWindowBounds(*create_data, window_bounds);
    if (!bounds_error.empty()) {
      return RespondNow(Error(std::move(bounds_error)));
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
  if (GetBrowserWindowCreationStatusForProfile(*window_profile) !=
      BrowserWindowInterface::CreationStatus::kOk) {
    return RespondNow(Error(ExtensionTabUtil::kBrowserWindowNotAllowed));
  }
  BrowserWindowCreateParams create_params(window_type, *window_profile,
                                          user_gesture());

  if (isolated_web_app_url_info.has_value()) {
    create_params.type = BrowserWindowInterface::TYPE_APP;
    create_params.app_name = web_app::GenerateApplicationNameFromAppId(
        isolated_web_app_url_info->app_id());
    // For Isolated Web Apps, the actual navigating-to URL will be the app's
    // start_url to prevent deep-linking attacks, while the original URL will be
    // accessible via window.launchQueue; for this reason the browser is marked
    // trusted.
    create_params.is_trusted_source = true;
  } else if (!extension_id.empty()) {
    // extension_id is only set for CREATE_TYPE_POPUP.
    create_params.type = BrowserWindowInterface::TYPE_APP_POPUP;
    create_params.app_name =
        web_app::GenerateApplicationNameFromAppId(extension_id);
    create_params.is_trusted_source = false;
  }
  create_params.initial_bounds = window_bounds;
  create_params.initial_show_state = ui::mojom::WindowShowState::kNormal;

  if (create_data && create_data->state != windows::WindowState::kNone) {
    create_params.initial_show_state =
        tabs_internal::ConvertToWindowShowState(create_data->state);
  }

  BrowserWindowInterface* new_window =
      CreateBrowserWindow(std::move(create_params));
  if (!new_window) {
    return RespondNow(Error(ExtensionTabUtil::kBrowserWindowNotAllowed));
  }
  // NOTE: Even though `new_window` was returned, it may not be fully
  // initialized on non-desktop platforms. See documentation on
  // CreateBrowserWindow().

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
    if (registrar.AppMatches(iwa_id, web_app::WebAppFilter::IsIsolatedApp())) {
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
          ExtensionTabUtil::GetEditableTabStripModel(
              new_window->GetBrowserForMigrationOnly());
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
    // TODO(crbug.com/452431839) Make a new NewTabTypes value for
    // when new tabs are made because of an empty window.
    chrome::NewTab(new_window->GetBrowserForMigrationOnly(),
                   NewTabTypes::kNewTabCommand);
  }
  chrome::SelectNumberedTab(
      new_window->GetBrowserForMigrationOnly(), 0,
      TabStripUserGestureDetails(
          TabStripUserGestureDetails::GestureType::kNone));

  if (focused) {
    new_window->GetWindow()->Show();
  } else {
    // Show an unfocused new window.
    BrowserWindowInterface* const last_active_bwi =
        GetLastActiveBrowserWindowInterfaceWithAnyProfile();

    // On some OSes the new unfocused window is shown on top by default.
    // ScopedPinBrowserAtFront prevents the new browser from being shown above
    // the old active browser.
    if (last_active_bwi && last_active_bwi->IsActive()) {
      ScopedPinBrowserAtFront scoper(last_active_bwi);
      new_window->GetWindow()->ShowInactive();
    } else {
      new_window->GetWindow()->ShowInactive();
    }
  }

// Despite creating the window with initial_show_state() ==
// ui::mojom::WindowShowState::kMinimized above, on Linux the window is not
// created as minimized.
// TODO(crbug.com/40254339): Remove this workaround when linux is fixed.
// TODO(crbug.com/40254339): Find a fix for wayland as well.
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)
  if (new_window->GetBrowserForMigrationOnly()->initial_show_state() ==
      ui::mojom::WindowShowState::kMinimized) {
    new_window->GetWindow()->Minimize();
  }
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)

  // Lock the window fullscreen only after the new tab has been created
  // (otherwise the tabstrip is empty), and window()->show() has been called
  // (otherwise that resets the locked mode for devices in tablet mode).
  // TODO(crbug.com/438540029) - Remove once the migration is complete.
  if (create_data &&
      create_data->state == windows::WindowState::kLockedFullscreen) {
#if BUILDFLAG(IS_CHROMEOS)
    ash::boca::LockedQuizSessionManagerFactory::GetInstance()
        ->GetForBrowserContext(calling_profile)
        ->SetLockedFullscreenState(new_window->GetBrowserForMigrationOnly(),
                                   /*pinned=*/true);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  if (new_window->GetProfile()->IsOffTheRecord() &&
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

// static
std::string WindowsCreateFunction::ValidateTab(
    WindowController* source_window,
    Profile* window_profile,
    Profile* calling_profile,
    content::WebContents* web_contents,
    bool is_locked_fullscreen,
    const std::vector<GURL>& urls) {
  if (!source_window) {
    // The source window can be null for prerender tabs.
    return tabs_constants::kInvalidWindowStateError;
  }

  Browser* source_browser = source_window->GetBrowser();
  if (!source_browser) {
    return ExtensionTabUtil::kCanOnlyMoveTabsWithinNormalWindowsError;
  }

  if (web_app::AppBrowserController* controller =
          source_browser->app_controller();
      controller && controller->IsIsolatedWebApp()) {
    return kWindowCreateCannotMoveIwaTabError;
  }

  if (!ExtensionTabUtil::IsTabStripEditable()) {
    return ExtensionTabUtil::kTabStripNotEditableError;
  }

  if (source_window->profile() != window_profile) {
    return ExtensionTabUtil::kCanOnlyMoveTabsWithinSameProfileError;
  }

  if (DevToolsWindow::IsDevToolsWindow(web_contents)) {
    return tabs_constants::kNotAllowedForDevToolsError;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Tabs cannot be moved to the OnTask system web app. Only relevant for
  // locked fullscreen on ChromeOS.
  if (is_locked_fullscreen &&
      ash::features::IsBocaOnTaskLockedQuizMigrationEnabled()) {
    return ExtensionTabUtil::kCanOnlyMoveTabsWithinNormalWindowsError;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  return std::string();  // No error.
}

// static
std::string WindowsCreateFunction::SetWindowBounds(
    const api::windows::Create::Params::CreateData& create_data,
    gfx::Rect& window_bounds) {
  bool set_window_position = false;
  bool set_window_size = false;
  if (create_data.left) {
    window_bounds.set_x(*create_data.left);
    set_window_position = true;
  }
  if (create_data.top) {
    window_bounds.set_y(*create_data.top);
    set_window_position = true;
  }
  if (create_data.width) {
    window_bounds.set_width(*create_data.width);
    set_window_size = true;
  }
  if (create_data.height) {
    window_bounds.set_height(*create_data.height);
    set_window_size = true;
  }

  // If the extension specified the window size but no position, adjust the
  // window to fit in the display.
  if (!set_window_position && set_window_size) {
    const display::Display& display =
        display::Screen::Get()->GetDisplayMatching(window_bounds);
    window_bounds.AdjustToFit(display.bounds());
  }

  // Immediately fail if the window bounds don't intersect the displays.
  if ((set_window_position || set_window_size) &&
      !tabs_internal::WindowBoundsIntersectDisplays(window_bounds)) {
    return tabs_constants::kInvalidWindowBoundsError;
  }

  return std::string();  // No error.
}

#if BUILDFLAG(IS_CHROMEOS)
void WindowsCreateFunction::OnWindowCreatedAsynchronously(
    const SessionID& session_id) {
  BrowserWindowInterface* const browser =
      BrowserWindowInterface::FromSessionID(session_id);
  if (!browser) {
    RespondWithError(ExtensionTabUtil::kBrowserWindowNotAllowed);
    return;
  }
  Respond(WithArguments(ExtensionTabUtil::CreateWindowValueForExtension(
      *browser, extension(), WindowController::kPopulateTabs,
      source_context_type())));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// Tabs ------------------------------------------------------------------------

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

TabsUpdateFunction::TabsUpdateFunction() = default;

ExtensionFunction::ResponseAction TabsUpdateFunction::Run() {
  std::optional<tabs::Update::Params> params =
      tabs::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  int tab_id = -1;
  WebContents* contents = nullptr;
  if (!params->tab_id) {
    // Attempt to look up the current tab in the current window.
    if (!ComputeDefaultTabId(tab_id, contents, error)) {
      return RespondNow(Error(std::move(error)));
    }
  } else {
    tab_id = *params->tab_id;
  }

  int tab_index = -1;
  WindowController* window = nullptr;
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

  // Cache the original web contents.
  content::WebContents* original_contents = contents;

  // Update the active (aka selected) tab.
  if (!UpdateActiveTab(*params, tab_strip, tab_index, contents, error)) {
    return RespondNow(Error(std::move(error)));
  }

  // Update the highlighted tab.
  if (!UpdateHighlightedTab(*params, tab_strip, tab_index, error)) {
    return RespondNow(Error(std::move(error)));
  }

  if (params->update_properties.muted &&
      !SetTabAudioMuted(contents, *params->update_properties.muted,
                        TabMutedReason::kExtension, extension()->id())) {
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
        original_contents)
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

    // See tabs_api.cc for the implementation of UpdateURL().
    if (!UpdateURL(original_contents, updated_url, tab_id, &error)) {
      return RespondNow(Error(std::move(error)));
    }

#if BUILDFLAG(FULL_SAFE_BROWSING)
    tabs_internal::NotifyExtensionTelemetry(
        Profile::FromBrowserContext(browser_context()), extension(),
        safe_browsing::TabsApiInfo::UPDATE, current_url, updated_url,
        js_callstack());
#endif
  }

  // See tabs_api.cc for the implementation of GetResult().
  return RespondNow(GetResult(original_contents));
}

bool TabsUpdateFunction::ComputeDefaultTabId(int& tab_id,
                                             content::WebContents*& contents,
                                             std::string& error) {
  const auto* window_controller =
      ChromeExtensionFunctionDetails(this).GetCurrentWindowController();
  if (!window_controller) {
    error = ExtensionTabUtil::kNoCurrentWindowError;
    return false;
  }
  if (!ExtensionTabUtil::IsTabStripEditable()) {
    error = ExtensionTabUtil::kTabStripNotEditableError;
    return false;
  }
  contents = window_controller->GetActiveTab();
  if (!contents) {
    error = tabs_constants::kNoSelectedTabError;
    return false;
  }
  tab_id = ExtensionTabUtil::GetTabId(contents);
  return true;
}

bool TabsUpdateFunction::UpdateActiveTab(
    const api::tabs::Update::Params& params,
    TabStripModel* tab_strip,
    int tab_index,
    const content::WebContents* contents,
    std::string& error) {
  bool active = false;
  // TODO(rafaelw): Setting |active| from js doesn't make much sense.
  // Move tab selection management up to window.
  if (params.update_properties.selected) {
    active = *params.update_properties.selected;
  }

  // The 'active' property has replaced 'selected'.
  if (params.update_properties.active) {
    active = *params.update_properties.active;
  }

  if (!active) {
    // Nothing to activate.
    return true;
  }

  // Bug fix for crbug.com/1197888. Don't let the extension update the tab
  // if the user is dragging tabs.
  if (!ExtensionTabUtil::IsTabStripEditable()) {
    error = ExtensionTabUtil::kTabStripNotEditableError;
    return false;
  }

  if (tab_strip->active_index() != tab_index) {
    tab_strip->ActivateTabAt(tab_index);
    DCHECK_EQ(contents, tab_strip->GetActiveWebContents());
  }
  return true;
}

bool TabsUpdateFunction::UpdateHighlightedTab(
    const api::tabs::Update::Params& params,
    TabStripModel* tab_strip,
    int tab_index,
    std::string& error) {
  if (!params.update_properties.highlighted.has_value()) {
    // Nothing to highlight.
    return true;
  }

  // Bug fix for crbug.com/1197888. Don't let the extension update the tab
  // if the user is dragging tabs.
  if (!ExtensionTabUtil::IsTabStripEditable()) {
    error = ExtensionTabUtil::kTabStripNotEditableError;
    return false;
  }

  if (params.update_properties.highlighted.value()) {
    tab_strip->SelectTabAt(tab_index);
  } else {
    tab_strip->DeselectTabAt(tab_index);
  }
  return true;
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
      if (tabs_internal::MoveTabToWindow(
              this, tab_ids[i], target_window->GetBrowser(), -1, &error) < 0) {
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

  std::optional<split_tabs::SplitTabId> split_id =
      tab_strip_model->GetSplitForTab(tab_index);
  if (split_id.has_value()) {
    // If the tab is part of a split view, ungroup both tabs.
    gfx::Range index_range =
        tab_strip_model->GetSplitData(split_id.value())->GetIndexRange();
    std::vector<int> split_indices(index_range.length());
    std::iota(split_indices.begin(), split_indices.end(),
              static_cast<int>(index_range.start()));
    tab_strip_model->RemoveFromGroup(split_indices);
  } else {
    tab_strip_model->RemoveFromGroup({tab_index});
  }

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
