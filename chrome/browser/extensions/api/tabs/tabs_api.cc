// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_api.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/types/expected_macros.h"
#include "base/types/optional_util.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/windows_util.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/browser_window_util.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/open_tab_helper.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-forward.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/incognito_allowed_url.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_muted_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/base_window.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/display/screen.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "base/strings/stringprintf.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "ash/wm/window_pin_util.h"
#include "chrome/browser/ash/boca/on_task/locked_quiz_session_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace tabs = api::tabs;
namespace windows = api::windows;

constexpr char kCannotDetermineLanguageOfUnloadedTab[] =
    "Cannot determine language: tab not loaded";
constexpr char kFrameNotFoundError[] = "No frame with id * in tab *.";
constexpr char kCannotUpdateMuteCaptured[] =
    "Cannot update mute state for tab *, tab has audio or video currently "
    "being captured";

namespace {

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kWindowCreateLockedFullscreenUrlCountMismatchError[] =
    "When creating a new window in locked fullscreen mode, exactly one URL "
    "should be supplied.";
#endif  // BUILDFLAG(IS_CHROMEOS)

constexpr char kInvalidWindowTypeError[] = "Invalid value for type";
constexpr char kNoHighlightedTabError[] = "No highlighted tab";
constexpr char kTabIndexNotFoundError[] = "No tab at index: *.";
constexpr char kCannotFindTabToDiscard[] = "Cannot find a tab to discard.";

#if !BUILDFLAG(IS_ANDROID)
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
#endif

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

// Sets the opener of the given `tab` to `opener`. Returns true on success;
// on failure, populates `error`.
bool SetOpenerOfTab(content::WebContents& tab,
                    content::WebContents& opener,
                    std::string& error) {
  // Bug fix for crbug.com/1197888. Don't let the extension update the tab
  // if the user is dragging tabs.
  if (!ExtensionTabUtil::IsTabStripEditable()) {
    error = ExtensionTabUtil::kTabStripNotEditableError;
    return false;
  }

  BrowserWindowInterface* opener_browser =
      browser_window_util::GetBrowserForTabContents(opener);
  // NOTE: This would be more efficient if there were a
  // TabListInterface::GetIndexOfWebContents() or similar, since then we could
  // just check `opener_browser->GetIndexOfWebContents(&tab)` instead of looking
  // up the tab's browser.
  BrowserWindowInterface* tab_browser =
      browser_window_util::GetBrowserForTabContents(tab);
  if (!opener_browser || opener_browser != tab_browser) {
    error = "Tab opener must be in the same window as the updated tab.";
    return false;
  }

  // TODO(https://crbug.com/371432155): Support this on desktop android.
#if !BUILDFLAG(IS_ANDROID)
  TabStripModel* tab_strip =
      tab_browser->GetBrowserForMigrationOnly()->tab_strip_model();
  int tab_index = tab_strip->GetIndexOfWebContents(&tab);
  CHECK_NE(TabStripModel::kNoTab, tab_index);
  tab_strip->SetOpenerOfTabAt(tab_index,
                              ::tabs::TabInterface::GetFromContents(&opener));
#endif

  return true;
}

#if !BUILDFLAG(IS_ANDROID)

// Returns the IsolatedWebAppUrlInfo for the given call to windows.create() if
// the call is to create a new IWA window.
// Populates `error` with an error if the call is invalid.
// Note that returning std::nullopt *can* be valid (if error is unpopulated);
// this indicates the call is not for an IWA window.
std::optional<web_app::IsolatedWebAppUrlInfo> GetIsolatedWebAppInfo(
    const std::optional<windows::Create::Params::CreateData>& create_data,
    const std::vector<GURL>& parsed_urls,
    std::string* error) {
  if (parsed_urls.size() > 1) {
    if (std::ranges::any_of(parsed_urls, [](const GURL& url) {
          return url.SchemeIs(webapps::kIsolatedAppScheme);
        })) {
      // Invalid. Can only open a single IWA URL.
      *error = kWindowCreateSupportsOnlySingleIwaUrlError;
      return std::nullopt;
    }
  }

  if (parsed_urls.empty() ||
      !parsed_urls[0].SchemeIs(webapps::kIsolatedAppScheme)) {
    // Valid; not opening an IWA.
    return std::nullopt;
  }

  base::expected<web_app::IsolatedWebAppUrlInfo, std::string> maybe_url_info =
      web_app::IsolatedWebAppUrlInfo::Create(parsed_urls[0]);

  if (!maybe_url_info.has_value()) {
    // Invalid. Failed to create IWA info.
    *error = base::StringPrintf(kWindowCreateCannotParseIwaUrlError,
                                maybe_url_info.error().c_str());
    return std::nullopt;
  }

  // Validate `create_data` params to make sure they're compatible with IWAs.
  if (create_data) {
    if (create_data->tab_id) {
      // Invalid. Can't specify tab ID with IWAs.
      *error = kWindowCreateCannotUseTabIdWithIwaError;
      return std::nullopt;
    }

    switch (create_data->type) {
      case windows::CreateType::kNone:
      case windows::CreateType::kNormal:
        break;  // Valid type.
      case windows::CreateType::kPopup:
      case windows::CreateType::kPanel:
        // Invalid window type for IWAs.
        *error = kInvalidWindowTypeError;
        return std::nullopt;
    }

    if (create_data->set_self_as_opener && *create_data->set_self_as_opener) {
      // Invalid. Can't have openers with IWAs.
      *error = "Cannot specify setSelfAsOpener for isolated-app:// URLs.";
      return std::nullopt;
    }
  }

  // Valid IWA parameters.
  return *maybe_url_info;
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

#endif  // !BUILDFLAG(IS_ANDROID)

// Returns true if either |boolean| is disengaged, or if |boolean| and
// |value| are equal. This function is used to check if a tab's parameters match
// those of the browser.
bool MatchesBool(const std::optional<bool>& boolean, bool value) {
  return !boolean || *boolean == value;
}

// Returns true if the given browser window is in locked fullscreen mode
// (a special type of fullscreen where the user is locked into one browser
// window).
// TODO(https://crbug.com/432056907): Determine if we need locked-fullscreen
// support on desktop android.
bool IsLockedFullscreen(BrowserWindowInterface* browser) {
#if BUILDFLAG(IS_CHROMEOS)
  return platform_util::IsBrowserLockedFullscreen(
      browser->GetBrowserForMigrationOnly());
#else
  return false;
#endif
}

// Returns the tab group ID for the tab at `index`. Returns nullopt if the index
// is out of range, the tab is not found, or the tab is not part of a group.
std::optional<tab_groups::TabGroupId> GetTabGroupForTab(
    TabListInterface& tab_list,
    int index) {
  if (index < 0 || index >= tab_list.GetTabCount()) {
    return std::nullopt;
  }
  ::tabs::TabInterface* tab = tab_list.GetTab(index);
  CHECK(tab);
  return tab->GetGroup();
}

// Places the window in a special type of fullscreen where the user is locked
// into one browser window based on `is_locked_fullscreen`.
void MaybeSetLockedFullscreenState(const api::windows::Update::Params& params,
                                   BrowserWindowInterface* browser,
                                   bool is_locked_fullscreen) {
#if BUILDFLAG(IS_CHROMEOS)
  // State will be WINDOW_STATE_NONE if the state parameter wasn't passed from
  // the JS side, and in that case we don't want to change the locked state.
  Browser* const target_browser = browser->GetBrowserForMigrationOnly();
  if (target_browser) {
    Profile* const browser_profile = target_browser->profile();
    if (is_locked_fullscreen &&
        params.update_info.state != windows::WindowState::kLockedFullscreen &&
        params.update_info.state != windows::WindowState::kNone) {
      ash::boca::LockedQuizSessionManagerFactory::GetInstance()
          ->GetForBrowserContext(browser_profile)
          ->SetLockedFullscreenState(target_browser,
                                     /*pinned=*/false);
    } else if (!is_locked_fullscreen &&
               params.update_info.state ==
                   windows::WindowState::kLockedFullscreen) {
      ash::boca::LockedQuizSessionManagerFactory::GetInstance()
          ->GetForBrowserContext(browser_profile)
          ->SetLockedFullscreenState(target_browser,
                                     /*pinned=*/true);
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

// Updates `window_bounds` from `params`. Returns true if bounds were set.
bool UpdateWindowBoundsFromParams(const api::windows::Update::Params& params,
                                  gfx::Rect& window_bounds) {
  bool set_window_bounds = false;
  if (params.update_info.left) {
    window_bounds.set_x(*params.update_info.left);
    set_window_bounds = true;
  }
  if (params.update_info.top) {
    window_bounds.set_y(*params.update_info.top);
    set_window_bounds = true;
  }
  if (params.update_info.width) {
    window_bounds.set_width(*params.update_info.width);
    set_window_bounds = true;
  }
  if (params.update_info.height) {
    window_bounds.set_height(*params.update_info.height);
    set_window_bounds = true;
  }
  return set_window_bounds;
}

// Moves the given tab to the `target_browser`. On success, returns the new
// index of the tab in the target tabstrip. On failure, returns -1. Assumes that
// the caller has already checked whether the target window is different from
// the source. `allow_other_window_types` indicates whether moving tabs to
// windows with types other than BrowserWindowInterface::TYPE_NORMAL is
// supported; this is allowed in certain cases (like moving a tab to a popup).
int MoveTabToWindow(ExtensionFunction* function,
                    int tab_id,
                    BrowserWindowInterface* target_browser,
                    int new_index,
                    bool allow_other_window_types,
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

  // TODO(crbug.com/40638654): Rather than calling checking against
  // TYPE_NORMAL, should this call
  // SupportsWindowFeature(Browser::kFeatureTabstrip)?
  if (!allow_other_window_types &&
      target_browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    *error = ExtensionTabUtil::kCanOnlyMoveTabsWithinNormalWindowsError;
    return -1;
  }

  if (target_browser->GetProfile() != source_window->profile()) {
    *error = ExtensionTabUtil::kCanOnlyMoveTabsWithinSameProfileError;
    return -1;
  }

  TabListInterface* target_tab_list =
      ExtensionTabUtil::GetEditableTabList(*target_browser);
  CHECK(target_tab_list);

  // Clamp move location to the last position.
  // This is ">" because it can append to a new index position.
  // -1 means set the move location to the last position.
  int target_index = new_index;
  if (target_index > target_tab_list->GetTabCount() || target_index < 0) {
    target_index = target_tab_list->GetTabCount();
  }

  TabListInterface* tab_list =
      ExtensionTabUtil::GetEditableTabList(*target_browser);
  CHECK(tab_list);
  if (ExtensionTabUtil::SupportsTabGroups(target_browser)) {
    std::optional<tab_groups::TabGroupId> next_tab_dst_group =
        GetTabGroupForTab(*tab_list, target_index);

    std::optional<tab_groups::TabGroupId> prev_tab_dst_group =
        GetTabGroupForTab(*tab_list, target_index - 1);

    // Group contiguity is not respected in the target tabstrip.
    if (next_tab_dst_group.has_value() && prev_tab_dst_group.has_value() &&
        next_tab_dst_group == prev_tab_dst_group) {
      *error = tabs_constants::kInvalidTabIndexBreaksGroupContiguity;
      return -1;
    }
  }

  BrowserWindowInterface* source_browser =
      source_window->GetBrowserWindowInterface();
  if (!source_browser) {
    *error = ExtensionTabUtil::kCanOnlyMoveTabsWithinNormalWindowsError;
    return -1;
  }

  TabListInterface* source_tab_list = TabListInterface::From(source_browser);
  ::tabs::TabInterface* tab = source_tab_list->GetTab(source_index);
  if (!tab) {
    *error = ErrorUtils::FormatErrorMessage(ExtensionTabUtil::kTabNotFoundError,
                                            base::NumberToString(tab_id));
    return -1;
  }

  source_tab_list->MoveTabToWindow(
      tab->GetHandle(), target_browser->GetSessionID(), target_index);

  // The new index may differ from `target_index` if the target index was
  // invalid for any reason, or could be -1 if the move failed.
  int final_index = target_tab_list->GetIndexOfTab(tab->GetHandle());
  return final_index;
}

bool GetTabHandleById(int tab_id,
                      content::BrowserContext& context,
                      bool include_incognito,
                      ::tabs::TabHandle* tab_handle_out,
                      std::string* error_out) {
  WindowController* window = nullptr;
  int index = -1;
  if (!tabs_internal::GetTabById(tab_id, &context, include_incognito, &window,
                                 /*contents_out=*/nullptr, &index, error_out)) {
    return false;
  }
  // Some tabs (e.g. prerendering) don't return an index or a window controller.
  if (index == -1 || !window) {
    return false;
  }
  BrowserWindowInterface* browser = window->GetBrowserWindowInterface();
  if (!browser) {
    return false;
  }
  TabListInterface* tab_list = TabListInterface::From(browser);
  if (!tab_list) {
    return false;
  }
  *tab_handle_out = tab_list->GetTab(index)->GetHandle();
  return true;
}

}  // namespace

namespace tabs_internal {

bool ExtensionHasLockedFullscreenPermission(const Extension* extension) {
  return extension && extension->permissions_data()->HasAPIPermission(
                          mojom::APIPermissionID::kLockWindowFullscreenPrivate);
}

api::tabs::Tab CreateTabObjectHelper(content::WebContents* contents,
                                     const Extension* extension,
                                     mojom::ContextType context,
                                     BrowserWindowInterface* browser,
                                     int tab_index) {
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(extension, context, contents);
  TabListInterface* tab_list =
      browser ? TabListInterface::From(browser) : nullptr;
  return ExtensionTabUtil::CreateTabObject(contents, scrub_tab_behavior,
                                           extension, tab_list, tab_index);
}

bool GetTabById(int tab_id,
                content::BrowserContext* context,
                bool include_incognito,
                WindowController** window_out,
                content::WebContents** contents_out,
                int* index_out,
                std::string* error_out) {
  if (ExtensionTabUtil::GetTabById(tab_id, context, include_incognito,
                                   window_out, contents_out, index_out)) {
    return true;
  }

  if (error_out) {
    *error_out = ErrorUtils::FormatErrorMessage(
        ExtensionTabUtil::kTabNotFoundError, base::NumberToString(tab_id));
  }

  return false;
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
void NotifyExtensionTelemetry(Profile* profile,
                              const Extension* extension,
                              safe_browsing::TabsApiInfo::ApiMethod api_method,
                              const std::string& current_url,
                              const std::string& new_url,
                              const std::optional<StackTrace>& js_callstack) {
  // Ignore API calls that are not invoked by extensions.
  if (!extension) {
    return;
  }

  auto* extension_telemetry_service =
      safe_browsing::ExtensionTelemetryService::Get(profile);

  if (!extension_telemetry_service || !extension_telemetry_service->enabled()) {
    return;
  }

  auto tabs_api_signal = std::make_unique<safe_browsing::TabsApiSignal>(
      extension->id(), api_method, current_url, new_url,
      js_callstack.value_or(StackTrace()));
  extension_telemetry_service->AddSignal(std::move(tabs_api_signal));
}
#endif

content::WebContents* GetTabsAPIDefaultWebContents(ExtensionFunction* function,
                                                   int tab_id,
                                                   std::string* error) {
  content::WebContents* web_contents = nullptr;
  if (tab_id != -1) {
    // We assume this call leaves web_contents unchanged if it is unsuccessful.
    tabs_internal::GetTabById(tab_id, function->browser_context(),
                              function->include_incognito_information(),
                              /*window_out=*/nullptr, &web_contents,
                              /*index_out=*/nullptr, error);
  } else {
    WindowController* window_controller =
        ChromeExtensionFunctionDetails(function).GetCurrentWindowController();
    if (!window_controller) {
      *error = ExtensionTabUtil::kNoCurrentWindowError;
    } else {
      web_contents = window_controller->GetActiveTab();
      if (!web_contents) {
        *error = tabs_constants::kNoSelectedTabError;
      }
    }
  }
  return web_contents;
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

// Returns whether the given `bounds` intersect with at least 50% of all the
// displays.
bool WindowBoundsIntersectDisplays(const gfx::Rect& bounds) {
  // Bail if `bounds` has an overflown area.
  auto checked_area = bounds.size().GetCheckedArea();
  if (!checked_area.IsValid()) {
    return false;
  }

  int intersect_area = 0;
  for (const auto& display : display::Screen::Get()->GetAllDisplays()) {
    gfx::Rect display_bounds = display.bounds();
    display_bounds.Intersect(bounds);
    intersect_area += display_bounds.size().GetArea();
  }
  return intersect_area >= (bounds.size().GetArea() / 2);
}

}  // namespace tabs_internal

void ZoomModeToZoomSettings(zoom::ZoomController::ZoomMode zoom_mode,
                            api::tabs::ZoomSettings* zoom_settings) {
  DCHECK(zoom_settings);
  switch (zoom_mode) {
    case zoom::ZoomController::ZOOM_MODE_DEFAULT:
      zoom_settings->mode = api::tabs::ZoomSettingsMode::kAutomatic;
      zoom_settings->scope = api::tabs::ZoomSettingsScope::kPerOrigin;
      break;
    case zoom::ZoomController::ZOOM_MODE_ISOLATED:
      zoom_settings->mode = api::tabs::ZoomSettingsMode::kAutomatic;
      zoom_settings->scope = api::tabs::ZoomSettingsScope::kPerTab;
      break;
    case zoom::ZoomController::ZOOM_MODE_MANUAL:
      zoom_settings->mode = api::tabs::ZoomSettingsMode::kManual;
      zoom_settings->scope = api::tabs::ZoomSettingsScope::kPerTab;
      break;
    case zoom::ZoomController::ZOOM_MODE_DISABLED:
      zoom_settings->mode = api::tabs::ZoomSettingsMode::kDisabled;
      zoom_settings->scope = api::tabs::ZoomSettingsScope::kPerTab;
      break;
  }
}

ExtensionFunction::ResponseAction WindowsGetFunction::Run() {
  std::optional<windows::Get::Params> params =
      windows::Get::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  tabs_internal::ApiParameterExtractor<windows::Get::Params> extractor(params);
  WindowController* window_controller = nullptr;
  std::string error;
  if (!windows_util::GetControllerFromWindowID(this, params->window_id,
                                               extractor.type_filters(),
                                               &window_controller, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  WindowController::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? WindowController::kPopulateTabs
                                : WindowController::kDontPopulateTabs;
  base::DictValue windows = window_controller->CreateWindowValueForExtension(
      extension(), populate_tab_behavior, source_context_type());
  return RespondNow(WithArguments(std::move(windows)));
}

ExtensionFunction::ResponseAction WindowsGetCurrentFunction::Run() {
  std::optional<windows::GetCurrent::Params> params =
      windows::GetCurrent::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  tabs_internal::ApiParameterExtractor<windows::GetCurrent::Params> extractor(
      params);
  WindowController* window_controller = nullptr;
  std::string error;
  if (!windows_util::GetControllerFromWindowID(
          this, extension_misc::kCurrentWindowId, extractor.type_filters(),
          &window_controller, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  WindowController::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? WindowController::kPopulateTabs
                                : WindowController::kDontPopulateTabs;
  base::DictValue windows = window_controller->CreateWindowValueForExtension(
      extension(), populate_tab_behavior, source_context_type());
  return RespondNow(WithArguments(std::move(windows)));
}

ExtensionFunction::ResponseAction WindowsGetLastFocusedFunction::Run() {
  std::optional<windows::GetLastFocused::Params> params =
      windows::GetLastFocused::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  tabs_internal::ApiParameterExtractor<windows::GetLastFocused::Params>
      extractor(params);

  BrowserWindowInterface* last_focused_browser = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (windows_util::CanOperateOnWindow(
                this, BrowserExtensionWindowController::From(browser),
                extractor.type_filters())) {
          last_focused_browser = browser;
          return false;  // Stop iterating.
        }
        return true;  // Continue iterating.
      });
  if (!last_focused_browser) {
    return RespondNow(Error(tabs_constants::kNoLastFocusedWindowError));
  }

  WindowController::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? WindowController::kPopulateTabs
                                : WindowController::kDontPopulateTabs;
  base::DictValue windows = ExtensionTabUtil::CreateWindowValueForExtension(
      *last_focused_browser, extension(), populate_tab_behavior,
      source_context_type());
  return RespondNow(WithArguments(std::move(windows)));
}

ExtensionFunction::ResponseAction WindowsGetAllFunction::Run() {
  std::optional<windows::GetAll::Params> params =
      windows::GetAll::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  tabs_internal::ApiParameterExtractor<windows::GetAll::Params> extractor(
      params);
  base::ListValue window_list;
  WindowController::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? WindowController::kPopulateTabs
                                : WindowController::kDontPopulateTabs;
  for (WindowController* controller : *WindowControllerList::GetInstance()) {
    if (!controller->GetBrowserWindowInterface() ||
        !windows_util::CanOperateOnWindow(this, controller,
                                          extractor.type_filters())) {
      continue;
    }
    window_list.Append(ExtensionTabUtil::CreateWindowValueForExtension(
        *controller->GetBrowserWindowInterface(), extension(),
        populate_tab_behavior, source_context_type()));
  }

  return RespondNow(WithArguments(std::move(window_list)));
}

WindowsCreateFunction::WindowsCreateFunction() = default;
WindowsCreateFunction::~WindowsCreateFunction() = default;

ExtensionFunction::ResponseAction WindowsCreateFunction::Run() {
  std::optional<windows::Create::Params> params =
      windows::Create::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  DCHECK(extension() || source_context_type() == mojom::ContextType::kWebUi ||
         source_context_type() == mojom::ContextType::kUntrustedWebUi);
  create_data_ = std::move(params->create_data);

  // Look for optional url.
  if (create_data_ && create_data_->url) {
    std::vector<std::string> url_strings;
    // First, get all the URLs the client wants to open.
    if (create_data_->url->as_string) {
      url_strings.push_back(std::move(*create_data_->url->as_string));
    } else if (create_data_->url->as_strings) {
      url_strings = std::move(*create_data_->url->as_strings);
    }

    // Second, resolve, validate and convert them to GURLs.
    for (auto& url_string : url_strings) {
      auto url = ExtensionTabUtil::PrepareURLForNavigation(
          url_string, extension(), browser_context());
      if (!url.has_value()) {
        return RespondNow(Error(std::move(url.error())));
      }
      urls_.push_back(*url);
    }
  }

  std::string error;

#if !BUILDFLAG(IS_ANDROID)
  isolated_web_app_url_info_ =
      GetIsolatedWebAppInfo(create_data_, urls_, &error);
  if (!error.empty()) {
    return RespondNow(Error(std::move(error)));
  }
#endif

  // Decide whether we are opening a normal window or an incognito window.
  Profile* calling_profile = Profile::FromBrowserContext(browser_context());
  windows_util::IncognitoResult incognito_result =
      windows_util::ShouldOpenIncognitoWindow(
          calling_profile,
          create_data_ && create_data_->incognito
              ? std::optional<bool>(*create_data_->incognito)
              : std::nullopt,
          &urls_, &error);
  if (incognito_result == windows_util::IncognitoResult::kError) {
    return RespondNow(Error(std::move(error)));
  }

  Profile* window_profile =
      incognito_result == windows_util::IncognitoResult::kIncognito
          ? calling_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)
          : calling_profile;

  if (!IsValidStateForWindowsCreateFunction(
          base::OptionalToPtr(create_data_))) {
    return RespondNow(Error(tabs_constants::kInvalidWindowStateError));
  }

  // Look for optional tab id.
  bool is_locked_fullscreen =
      create_data_ &&
      create_data_->state == windows::WindowState::kLockedFullscreen;
  WindowController* source_window = nullptr;
  if (create_data_ && create_data_->tab_id) {
    // Find the tab.
    content::WebContents* web_contents = nullptr;
    if (!tabs_internal::GetTabById(*create_data_->tab_id, calling_profile,
                                   include_incognito_information(),
                                   &source_window, &web_contents,
                                   /*index_out=*/nullptr, &error)) {
      return RespondNow(Error(std::move(error)));
    }

    // Validate the tab information. Return an error if it's not valid.
    std::string tab_error = ValidateTab(source_window, window_profile,
                                        web_contents, is_locked_fullscreen);
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
      if (urls_.size() != 1) {
        return RespondNow(
            Error(kWindowCreateLockedFullscreenUrlCountMismatchError));
      }
      ash::boca::LockedQuizSessionManagerFactory::GetInstance()
          ->GetForBrowserContext(calling_profile)
          ->OpenLockedQuiz(
              urls_.front(),
              base::BindOnce(
                  &WindowsCreateFunction::OnBocaWindowCreatedAsynchronously,
                  this));
      return RespondLater();
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  BrowserWindowInterface::Type window_type =
      BrowserWindowInterface::TYPE_NORMAL;

  gfx::Rect window_bounds;
  std::string extension_id;

  if (create_data_) {
    // Figure out window type before figuring out bounds so that default
    // bounds can be set according to the window type.
    switch (create_data_->type) {
      // TODO(stevenjb): Remove 'panel' from windows.json.
      case windows::CreateType::kPanel:
      case windows::CreateType::kPopup:
        window_type = BrowserWindowInterface::TYPE_POPUP;
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
      // TODO(https://crbug.com/431004500): Properly initialize window bounds.
#if !BUILDFLAG(IS_ANDROID)
    ui::mojom::WindowShowState ignored_show_state =
        ui::mojom::WindowShowState::kDefault;
    WindowSizer::GetBrowserWindowBoundsAndShowState(
        gfx::Rect(), nullptr, &window_bounds, &ignored_show_state);
#endif

    // Update the window bounds based on the create parameters.
    std::string bounds_error = SetWindowBounds(*create_data_, window_bounds);
    if (!bounds_error.empty()) {
      return RespondNow(Error(std::move(bounds_error)));
    }

    // Record the window height and width to determine if we
    // can set a mininimum value for them (crbug.com/1369103).
    UMA_HISTOGRAM_COUNTS_1000("Extensions.CreateWindowWidth",
                              window_bounds.width());
    UMA_HISTOGRAM_COUNTS_1000("Extensions.CreateWindowHeight",
                              window_bounds.height());

    set_self_as_opener_ =
        create_data_->set_self_as_opener && *create_data_->set_self_as_opener;
    if (is_from_service_worker() && set_self_as_opener_) {
      // TODO(crbug.com/40636155): Add test for this.
      return RespondNow(
          Error("Cannot specify setSelfAsOpener Service Worker extension."));
    }
  }

  // Create a new BrowserWindow if possible.
  if (GetBrowserWindowCreationStatusForProfile(*window_profile) !=
      BrowserWindowInterface::CreationStatus::kOk) {
    return RespondNow(Error(ExtensionTabUtil::kBrowserWindowNotAllowed));
  }
  BrowserWindowCreateParams create_params(window_type, *window_profile,
                                          user_gesture());

  bool initialized_type = false;
#if !BUILDFLAG(IS_ANDROID)
  if (isolated_web_app_url_info_.has_value()) {
    create_params.type = BrowserWindowInterface::TYPE_APP;
    create_params.app_name = web_app::GenerateApplicationNameFromAppId(
        isolated_web_app_url_info_->app_id());
    // For Isolated Web Apps, the actual navigating-to URL will be the app's
    // start_url to prevent deep-linking attacks, while the original URL will be
    // accessible via window.launchQueue; for this reason the browser is marked
    // trusted.
    create_params.is_trusted_source = true;
    initialized_type = true;
  }
#endif

  if (!initialized_type && !extension_id.empty()) {
    // extension_id is only set for CREATE_TYPE_POPUP.

    // On non-Android platforms, we use TYPE_APP_POPUP. On Android, this is
    // unsupported, so we use TYPE_POPUP.
    // TODO(https://crbug.com/469000733): Investigate if we can just use
    // TYPE_POPUP everywhere.
    create_params.type =
#if BUILDFLAG(IS_ANDROID)
        BrowserWindowInterface::TYPE_POPUP;
#else
        BrowserWindowInterface::TYPE_APP_POPUP;
#endif

    // TODO(https://crbug.com/431004500): Initialize app name on android, or
    // verify this is unnecessary.
#if !BUILDFLAG(IS_ANDROID)
    create_params.app_name =
        web_app::GenerateApplicationNameFromAppId(extension_id);
#endif
    create_params.is_trusted_source = false;
    initialized_type = true;
  }
  create_params.initial_bounds = window_bounds;
  create_params.initial_show_state = ui::mojom::WindowShowState::kNormal;

  if (create_data_ && create_data_->state != windows::WindowState::kNone) {
    create_params.initial_show_state =
        tabs_internal::ConvertToWindowShowState(create_data_->state);
  }

#if !BUILDFLAG(IS_ANDROID)
  BrowserWindowInterface* new_window =
      CreateBrowserWindow(std::move(create_params));
  ExtensionFunction::ResponseValue response =
      OnBrowserWindowCreated(new_window);
  return RespondNow(std::move(response));
#else

  CHECK(create_params.type == BrowserWindowInterface::TYPE_NORMAL ||
        create_params.type == BrowserWindowInterface::TYPE_POPUP)
      << "Unexpected window type: " << static_cast<int>(create_params.type);

  CreateBrowserWindow(
      std::move(create_params),
      base::BindOnce(
          &WindowsCreateFunction::OnBrowserWindowCreatedAsynchronously, this));
  return RespondLater();
#endif  // BUILDFLAG(IS_ANDROID)
}

#if BUILDFLAG(IS_ANDROID)
void WindowsCreateFunction::OnBrowserWindowCreatedAsynchronously(
    BrowserWindowInterface* new_window) {
  ExtensionFunction::ResponseValue response =
      OnBrowserWindowCreated(new_window);
  Respond(std::move(response));
}
#endif

ExtensionFunction::ResponseValue WindowsCreateFunction::OnBrowserWindowCreated(
    BrowserWindowInterface* new_window) {
  if (!new_window) {
    return Error(ExtensionTabUtil::kBrowserWindowNotAllowed);
  }
  // NOTE: Even though `new_window` was returned, it may not be fully
  // initialized on non-desktop platforms. See documentation on
  // CreateBrowserWindow().

  auto create_nav_params = [&](const GURL& url, bool is_first_nav) {
    NavigateParams navigate_params(new_window, url, ui::PAGE_TRANSITION_LINK);

    navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

#if BUILDFLAG(IS_ANDROID)
    // On Android, new windows are created with a single empty tab. As such,
    // when navigating, we need to navigate that first tab, instead of adding
    // new ones. Otherwise, we'd end up with one extra tab in the new window.
    if (is_first_nav) {
      navigate_params.disposition = WindowOpenDisposition::CURRENT_TAB;
    }
#endif
    // Ensure that these navigations will not get 'captured' into PWA windows,
    // as this means that `new_window` could be ignored. It may be
    // useful/desired in the future to allow this behavior, but this may require
    // an API change, or at least a re-write of how these navigations are called
    // to be compatible with the navigation capturing behavior.
    navigate_params.pwa_navigation_capturing_force_off = true;

    if (OpenTabHelper::MaybeSetPdfNavigateParams(*this, navigate_params)) {
      return navigate_params;
    }

    if (set_self_as_opener_) {
      // Depending on the `setSelfAsOpener` option, we need to put the new
      // contents in the same BrowsingInstance as their opener.  See also
      // https://crbug.com/40516654.
      //
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

  bool navigated = false;
#if !BUILDFLAG(IS_ANDROID)
  if (isolated_web_app_url_info_) {
    CHECK_EQ(urls_.size(), 1U);
    const GURL& original_url = urls_[0];

    const webapps::AppId& iwa_id = isolated_web_app_url_info_->app_id();
    web_app::WebAppRegistrar& registrar =
        web_app::WebAppProvider::GetForWebApps(new_window->GetProfile())
            ->registrar_unsafe();

    // TODO(crbug.com/424128443): create an dummy tab in the browser so that the
    // returned window's tab count is always equal to 1 -- this will limit the
    // extension's ability to figure out which IWAs are installed without the
    // `tabs` permission.
    if (registrar.AppMatches(iwa_id, web_app::WebAppFilter::IsIsolatedApp())) {
      NavigateParams navigate_params = create_nav_params(
          registrar.GetAppStartUrl(iwa_id), /*is_first_nav=*/true);
      base::WeakPtr<content::NavigationHandle> handle =
          Navigate(&navigate_params);
      CHECK(handle);
      web_app::EnqueueLaunchParams(
          handle->GetWebContents(), iwa_id, original_url,
          /*wait_for_navigation_to_complete=*/true, handle->NavigationStart());
    }
    navigated = true;
  }
#endif

  if (!navigated) {
    bool is_first_nav = true;
    for (const GURL& url : urls_) {
      NavigateParams navigate_params = create_nav_params(url, is_first_nav);
      is_first_nav = false;
      Navigate(&navigate_params);
    }
  }

  TabListInterface* tab_list = TabListInterface::From(new_window);
  CHECK(tab_list);

#if !BUILDFLAG(IS_ANDROID)
  bool moved_tab = false;
#endif
  // Move the tab into the created window only if it's an empty popup or it's
  // a tabbed window.
  if (new_window->GetType() == BrowserWindowInterface::TYPE_NORMAL ||
      urls_.empty()) {
    if (create_data_ && create_data_->tab_id) {
      std::string error;
      // -1 means "move tab to the end", which is what we want.
      int new_index = -1;
      if (MoveTabToWindow(this, *create_data_->tab_id, new_window, new_index,
                          /*allow_other_window_types=*/true, &error) < 0) {
        return Error(std::move(error));
      }

#if BUILDFLAG(IS_ANDROID)
      // On Android, a new window is created with a single default tab. If urls_
      // is empty, it means:
      //
      // (1) We haven't navigated, which would have navigated the default tab to
      // a URL;
      //
      // (2) There should be only 2 tabs: the default tab and the tab with
      // "create_data_->tab_id".
      //
      // As the tab with "create_data_->tab_id" is added to the end of the tab
      // list, we close the first (default) tab to match the behavior on other
      // platforms: the new window should only have the tab with
      // "create_data_->tab_id".
      //
      // TODO(crbug.com/477611601): Remove this logic when a new Android window
      // has no tabs, like Windows/Mac/Linux.
      if (urls_.empty()) {
        CHECK(tab_list->GetTabCount() == 2);
        tab_list->CloseTab(tab_list->GetTab(0)->GetHandle());
      }
#else
      moved_tab = true;
#endif
    }
  }

  // Create a new tab if the created window is still empty. Don't create a new
  // tab when it is intended to create an empty popup.
  // TODO(https://crbug.com/431004500): Port to desktop android.
#if !BUILDFLAG(IS_ANDROID)
  if (!moved_tab && urls_.empty() &&
      new_window->GetType() == Browser::TYPE_NORMAL) {
    // TODO(crbug.com/452431839) Make a new NewTabTypes value for
    // when new tabs are made because of an empty window.
    chrome::NewTab(new_window->GetBrowserForMigrationOnly(),
                   NewTabTypes::kNewTabCommand);
  }
#endif

  // Select the first tab in the window, if there's at least one tab. There may
  // be no tabs, since we allow the creation of an empty popup above.
  if (tab_list->GetTabCount() > 0) {
    tab_list->ActivateTab(tab_list->GetTab(0)->GetHandle());
  }

  bool focused = true;
  if (create_data_ && create_data_->focused) {
    focused = *create_data_->focused;
  }

  if (focused) {
    new_window->GetWindow()->Show();
  } else {
    // TODO(https://crbug.com/431004500): Port to desktop android.
#if !BUILDFLAG(IS_ANDROID)
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
#endif
  }

// Despite creating the window with initial_show_state() ==
// ui::mojom::WindowShowState::kMinimized above, on Linux the window is not
// created as minimized.
// TODO(crbug.com/40254339): Remove this workaround when linux is fixed.
// TODO(crbug.com/40254339): Find a fix for wayland as well.
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(SUPPORTS_OZONE_X11)
  if (new_window->GetBrowserForMigrationOnly()->initial_show_state() ==
      ui::mojom::WindowShowState::kMinimized) {
    new_window->GetWindow()->Minimize();
  }
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(SUPPORTS_OZONE_X11)

  // Lock the window fullscreen only after the new tab has been created
  // (otherwise the tabstrip is empty), and window()->show() has been called
  // (otherwise that resets the locked mode for devices in tablet mode).
  // TODO(crbug.com/438540029) - Remove once the migration is complete.
  if (create_data_ &&
      create_data_->state == windows::WindowState::kLockedFullscreen) {
#if BUILDFLAG(IS_CHROMEOS)
    ash::boca::LockedQuizSessionManagerFactory::GetInstance()
        ->GetForBrowserContext(Profile::FromBrowserContext(browser_context()))
        ->SetLockedFullscreenState(new_window->GetBrowserForMigrationOnly(),
                                   /*pinned=*/true);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  if (new_window->GetProfile()->IsOffTheRecord() &&
      !browser_context()->IsOffTheRecord() &&
      !include_incognito_information()) {
    // Don't expose incognito windows if extension itself works in non-incognito
    // profile and CanCrossIncognito isn't allowed.
    return WithArguments(base::Value());
  }

  return WithArguments(ExtensionTabUtil::CreateWindowValueForExtension(
      *new_window, extension(), WindowController::kPopulateTabs,
      source_context_type()));
}

// static
std::string WindowsCreateFunction::ValidateTab(
    WindowController* source_window,
    Profile* window_profile,
    content::WebContents* web_contents,
    bool is_locked_fullscreen) {
  if (!source_window) {
    // The source window can be null for prerender tabs.
    return tabs_constants::kInvalidWindowStateError;
  }

  if (!source_window->GetBrowserWindowInterface()) {
    return ExtensionTabUtil::kCanOnlyMoveTabsWithinNormalWindowsError;
  }

#if !BUILDFLAG(IS_ANDROID)
  Browser* source_browser = source_window->GetBrowser();
  if (web_app::AppBrowserController* controller =
          source_browser->app_controller();
      controller && controller->IsIsolatedWebApp()) {
    return kWindowCreateCannotMoveIwaTabError;
  }
#endif

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
void WindowsCreateFunction::OnBocaWindowCreatedAsynchronously(
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

  BrowserWindowInterface* browser =
      window_controller->GetBrowserWindowInterface();
  if (!browser) {
    return RespondNow(Error(ExtensionTabUtil::kNoCrashBrowserError));
  }
  ui::BaseWindow* browser_window = browser->GetWindow();

  // Don't allow locked fullscreen operations on a window without the proper
  // permission (also don't allow any operations on a locked window if the
  // extension doesn't have the permission).
  const bool is_locked_fullscreen = IsLockedFullscreen(browser);
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
  gfx::Rect window_bounds = browser_window->IsMinimized()
                                ? browser_window->GetRestoredBounds()
                                : browser_window->GetBounds();
  const bool set_window_bounds =
      UpdateWindowBoundsFromParams(*params, window_bounds);

  if (set_window_bounds &&
      !tabs_internal::WindowBoundsIntersectDisplays(window_bounds)) {
    return RespondNow(Error(tabs_constants::kInvalidWindowBoundsError));
  }

  ui::mojom::WindowShowState show_state =
      tabs_internal::ConvertToWindowShowState(params->update_info.state);
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
  MaybeSetLockedFullscreenState(*params, browser, is_locked_fullscreen);

  UpdateWindowState(*params, browser, window_controller, show_state,
                    set_window_bounds, window_bounds);

  return RespondNow(
      WithArguments(window_controller->CreateWindowValueForExtension(
          extension(), WindowController::kDontPopulateTabs,
          source_context_type())));
}

void WindowsUpdateFunction::UpdateWindowState(
    const api::windows::Update::Params& params,
    BrowserWindowInterface* browser,
    WindowController* window_controller,
    ui::mojom::WindowShowState show_state,
    bool set_window_bounds,
    const gfx::Rect& window_bounds) {
  ui::BaseWindow* browser_window = browser->GetWindow();

  if (show_state != ui::mojom::WindowShowState::kFullscreen &&
      show_state != ui::mojom::WindowShowState::kDefault) {
    window_controller->SetFullscreenMode(false, extension()->url());
  }

  switch (show_state) {
    case ui::mojom::WindowShowState::kMinimized:
      browser_window->Minimize();
      break;
    case ui::mojom::WindowShowState::kMaximized:
      browser_window->Maximize();
      break;
    case ui::mojom::WindowShowState::kFullscreen:
      if (browser_window->IsMinimized() || browser_window->IsMaximized()) {
        browser_window->Restore();
      }
      window_controller->SetFullscreenMode(true, extension()->url());
      break;
    case ui::mojom::WindowShowState::kNormal:
      browser_window->Restore();
      break;
    default:
      break;
  }

  if (set_window_bounds) {
    // TODO(varkha): Updating bounds during a drag can cause problems and a more
    // general solution is needed. See http://crbug.com/40322435 .
    browser_window->SetBounds(window_bounds);
  }

  if (params.update_info.focused) {
    if (*params.update_info.focused) {
      browser_window->Activate();
    } else {
      browser_window->Deactivate();
    }
  }

  if (params.update_info.draw_attention) {
    browser_window->FlashFrame(*params.update_info.draw_attention);
  }
}

ExtensionFunction::ResponseAction WindowsRemoveFunction::Run() {
  std::optional<windows::Remove::Params> params =
      windows::Remove::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  WindowController* window_controller = nullptr;
  std::string error;
  if (!windows_util::GetControllerFromWindowID(
          this, params->window_id, WindowController::kNoWindowFilter,
          &window_controller, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  // TODO(https://crbug.com/432056907): Determine if we need locked-fullscreen
  // support on desktop android.
#if !BUILDFLAG(IS_ANDROID)
  if (window_controller->GetBrowser() &&
      platform_util::IsBrowserLockedFullscreen(
          window_controller->GetBrowser()) &&
      !tabs_internal::ExtensionHasLockedFullscreenPermission(extension())) {
    return RespondNow(
        Error(tabs_internal::kMissingLockWindowFullscreenPrivatePermission));
  }
#endif

  TabListInterface* tab_list =
      TabListInterface::From(window_controller->GetBrowserWindowInterface());
  if (tab_list && !tab_list->IsThisTabListEditable()) {
    return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
  }
  window_controller->window()->Close();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsGetFunction::Run() {
  std::optional<tabs::Get::Params> params = tabs::Get::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  int tab_id = params->tab_id;

  WindowController* window = nullptr;
  content::WebContents* contents = nullptr;
  int tab_index = -1;
  std::string error;
  if (!tabs_internal::GetTabById(tab_id, browser_context(),
                                 include_incognito_information(), &window,
                                 &contents, &tab_index, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  return RespondNow(ArgumentList(
      tabs::Get::Results::Create(tabs_internal::CreateTabObjectHelper(
          contents, extension(), source_context_type(),
          window ? window->GetBrowserWindowInterface() : nullptr, tab_index))));
}

ExtensionFunction::ResponseAction TabsGetCurrentFunction::Run() {
  DCHECK(dispatcher());

  // If called from a tab, return the details from that tab. If not called from
  // a tab, return nothing (making the returned value undefined to the
  // extension), rather than an error.
  content::WebContents* caller_contents = GetSenderWebContents();
  if (caller_contents && ExtensionTabUtil::GetTabId(caller_contents) >= 0) {
    return RespondNow(ArgumentList(
        tabs::Get::Results::Create(tabs_internal::CreateTabObjectHelper(
            caller_contents, extension(), source_context_type(), nullptr,
            -1))));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsGetSelectedFunction::Run() {
  // windowId defaults to "current" window.
  int window_id = extension_misc::kCurrentWindowId;

  std::optional<tabs::GetSelected::Params> params =
      tabs::GetSelected::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (params->window_id) {
    window_id = *params->window_id;
  }

  std::string error;
  WindowController* window_controller =
      ExtensionTabUtil::GetControllerFromWindowID(
          ChromeExtensionFunctionDetails(this), window_id, &error);
  if (!window_controller) {
    return RespondNow(Error(std::move(error)));
  }

  BrowserWindowInterface* browser =
      window_controller->GetBrowserWindowInterface();
  if (!browser) {
    return RespondNow(Error(ExtensionTabUtil::kNoCrashBrowserError));
  }
  TabListInterface* tab_list = ExtensionTabUtil::GetEditableTabList(*browser);
  if (!tab_list) {
    return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
  }
  ::tabs::TabInterface* tab = tab_list->GetActiveTab();
  if (!tab) {
    return RespondNow(Error(tabs_constants::kNoSelectedTabError));
  }

  return RespondNow(ArgumentList(
      tabs::Get::Results::Create(tabs_internal::CreateTabObjectHelper(
          tab->GetContents(), extension(), source_context_type(), browser,
          tab_list->GetActiveIndex()))));
}

ExtensionFunction::ResponseAction TabsGetAllInWindowFunction::Run() {
  std::optional<tabs::GetAllInWindow::Params> params =
      tabs::GetAllInWindow::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  // windowId defaults to "current" window.
  int window_id = extension_misc::kCurrentWindowId;
  if (params->window_id) {
    window_id = *params->window_id;
  }

  std::string error;
  WindowController* window_controller =
      ExtensionTabUtil::GetControllerFromWindowID(
          ChromeExtensionFunctionDetails(this), window_id, &error);
  if (!window_controller) {
    return RespondNow(Error(std::move(error)));
  }

  return RespondNow(WithArguments(
      window_controller->CreateTabList(extension(), source_context_type())));
}

ExtensionFunction::ResponseAction TabsQueryFunction::Run() {
  std::optional<tabs::Query::Params> params =
      tabs::Query::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  query_info_ = std::move(params->query_info);

  URLPatternSet url_patterns;
  if (query_info_.url) {
    std::vector<std::string> url_pattern_strings;
    if (query_info_.url->as_string) {
      url_pattern_strings.push_back(*query_info_.url->as_string);
    } else if (query_info_.url->as_strings) {
      url_pattern_strings.swap(*query_info_.url->as_strings);
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

  int window_id = extension_misc::kUnknownWindowId;
  if (query_info_.window_id) {
    window_id = *query_info_.window_id;
  }

  int index = -1;
  if (query_info_.index) {
    index = *query_info_.index;
  }

  std::string window_type;
  if (query_info_.window_type != tabs::WindowType::kNone) {
    window_type = tabs::ToString(query_info_.window_type);
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  BrowserWindowInterface* last_active_browser =
      browser_window_util::GetLastActiveBrowserWithProfile(
          *profile, include_incognito_information());

  // Note that the current browser is allowed to be null: you can still query
  // the tabs in this case.
  BrowserWindowInterface* current_browser = nullptr;
  WindowController* current_window_controller =
      ChromeExtensionFunctionDetails(this).GetCurrentWindowController();
  if (current_window_controller) {
    current_browser = current_window_controller->GetBrowserWindowInterface();
    // Note: current_browser may still be null.
  }

  base::ListValue result =
      BuildTabList(current_browser, last_active_browser, url_patterns,
                   window_type, window_id, index);

  return RespondNow(WithArguments(std::move(result)));
}

base::ListValue TabsQueryFunction::BuildTabList(
    BrowserWindowInterface* current_browser,
    BrowserWindowInterface* last_active_browser,
    const URLPatternSet& url_patterns,
    const std::string& window_type,
    int window_id,
    int tab_index) {
  base::ListValue result;
  // Historically, we queried browsers in creation order. Maintain that behavior
  // (for now).
  std::vector<BrowserWindowInterface*> all_browsers =
      GetAllBrowserWindowInterfaces();
  for (auto* browser : all_browsers) {
    if (!MatchesWindow(browser, current_browser, last_active_browser,
                       window_type, window_id)) {
      continue;
    }

    TabListInterface* tab_list = TabListInterface::From(browser);
    for (int i = 0; i < tab_list->GetTabCount(); ++i) {
      if (tab_index > -1 && i != tab_index) {
        continue;
      }

      ::tabs::TabInterface* tab = tab_list->GetTab(i);
      CHECK(tab);

      if (!MatchesTab(tab, url_patterns)) {
        continue;
      }

      result.Append(tabs_internal::CreateTabObjectHelper(
                        tab->GetContents(), extension(), source_context_type(),
                        browser, i)
                        .ToValue());
    }
  }
  return result;
}

bool TabsQueryFunction::MatchesWindow(
    BrowserWindowInterface* candidate_browser,
    BrowserWindowInterface* current_browser,
    BrowserWindowInterface* last_active_browser,
    const std::string& target_window_type,
    int target_window_id) {
  // First, check if the profile matches.
  Profile* candidate_profile = candidate_browser->GetProfile();
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (!profile->IsSameOrParent(candidate_profile)) {
    return false;
  }
  if (!include_incognito_information() && profile != candidate_profile) {
    return false;
  }

  if (!candidate_browser->GetWindow()) {
    return false;
  }

  WindowController* window_controller =
      BrowserExtensionWindowController::From(candidate_browser);
  CHECK(window_controller);
  if (!window_controller->IsVisibleToTabsAPIForExtension(
          extension(), /*include_dev_tools_windows=*/false)) {
    return false;
  }

  // Note: `target_window_id` may be -1 or -2, which indicate unknown and
  // current windows.
  if (target_window_id >= 0 &&
      target_window_id != ExtensionTabUtil::GetWindowId(candidate_browser)) {
    return false;
  }

  if (target_window_id == extension_misc::kCurrentWindowId &&
      candidate_browser != current_browser) {
    return false;
  }

  if (!MatchesBool(query_info_.current_window,
                   candidate_browser == current_browser)) {
    return false;
  }

  if (!MatchesBool(query_info_.last_focused_window,
                   candidate_browser == last_active_browser)) {
    return false;
  }

  if (!target_window_type.empty() &&
      target_window_type != window_controller->GetWindowTypeText()) {
    return false;
  }

  return true;
}

bool TabsQueryFunction::MatchesTab(::tabs::TabInterface* candidate_tab,
                                   const URLPatternSet& target_url_patterns) {
  content::WebContents* web_contents = candidate_tab->GetContents();

  if (!web_contents) {
    return false;
  }

  if (!MatchesBool(query_info_.highlighted, candidate_tab->IsSelected())) {
    return false;
  }

  if (!MatchesBool(query_info_.active, candidate_tab->IsActivated())) {
    return false;
  }

  if (!MatchesBool(query_info_.pinned, candidate_tab->IsPinned())) {
    return false;
  }

  if (query_info_.group_id.has_value()) {
    std::optional<tab_groups::TabGroupId> group = candidate_tab->GetGroup();
    if (query_info_.group_id.value() == -1) {
      if (group.has_value()) {
        return false;
      }
    } else if (!group.has_value()) {
      return false;
    } else if (ExtensionTabUtil::GetGroupId(group.value()) !=
               query_info_.group_id.value()) {
      return false;
    }
  }

  if (query_info_.split_view_id.has_value()) {
    std::optional<split_tabs::SplitTabId> split = candidate_tab->GetSplit();
    if (query_info_.split_view_id.value() == -1) {
      if (split.has_value()) {
        return false;
      }
    } else if (!split.has_value() ||
               ExtensionTabUtil::GetSplitId(split.value()) !=
                   query_info_.split_view_id.value()) {
      return false;
    }
  }

  auto* audible_helper = RecentlyAudibleHelper::FromWebContents(web_contents);
  if (!MatchesBool(query_info_.audible, audible_helper->WasRecentlyAudible())) {
    return false;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  auto* tab_lifecycle_unit_external =
      resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
          web_contents);

  if (!MatchesBool(query_info_.frozen,
                   tab_lifecycle_unit_external->GetTabState() ==
                       ::mojom::LifecycleUnitState::FROZEN)) {
    return false;
  }

  if (!MatchesBool(query_info_.discarded,
                   tab_lifecycle_unit_external->GetTabState() ==
                       ::mojom::LifecycleUnitState::DISCARDED)) {
    return false;
  }

  if (!MatchesBool(query_info_.auto_discardable,
                   tab_lifecycle_unit_external->IsAutoDiscardable())) {
    return false;
  }
#endif

  if (!MatchesBool(query_info_.muted, web_contents->IsAudioMuted())) {
    return false;
  }

  bool check_title = query_info_.title && !query_info_.title->empty();
  if (check_title || !target_url_patterns.is_empty()) {
    // "title" and "url" properties are considered privileged data and can
    // only be checked if the extension has the "tabs" permission or it has
    // access to the WebContents's origin. Otherwise, this tab is considered
    // not matched.
    if (!extension_->permissions_data()->HasAPIPermissionForTab(
            ExtensionTabUtil::GetTabId(web_contents),
            mojom::APIPermissionID::kTab) &&
        !extension_->permissions_data()->HasHostPermission(
            web_contents->GetURL())) {
      return false;
    }

    if (check_title &&
        !base::MatchPattern(web_contents->GetTitle(),
                            base::UTF8ToUTF16(*query_info_.title))) {
      return false;
    }

    if (!target_url_patterns.is_empty() &&
        !target_url_patterns.MatchesURL(web_contents->GetURL())) {
      return false;
    }
  }

  if (query_info_.status != tabs::TabStatus::kNone &&
      query_info_.status != ExtensionTabUtil::GetLoadingStatus(web_contents)) {
    return false;
  }

  return true;
}

TabsCreateFunction::TabsCreateFunction() = default;
TabsCreateFunction::~TabsCreateFunction() = default;

ExtensionFunction::ResponseAction TabsCreateFunction::Run() {
  std::optional<tabs::Create::Params> params =
      tabs::Create::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!ExtensionTabUtil::IsTabStripEditable()) {
    return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
  }

  const tabs::Create::Params::CreateProperties& create_properties =
      params->create_properties;

  // The 'active' property has replaced the 'selected' property.
  active_ = create_properties.active ? create_properties.active
                                     : create_properties.selected;

  pinned_ = create_properties.pinned;
  index_ = create_properties.index;
  original_url_ = std::move(create_properties.url);

  validated_url_ = GURL(chrome::kChromeUINewTabURL);
  if (original_url_) {
    base::expected<GURL, std::string> maybe_url =
        ExtensionTabUtil::PrepareURLForNavigation(*original_url_, extension(),
                                                  browser_context());
    if (!maybe_url.has_value()) {
      return RespondNow(Error(maybe_url.error()));
    }
    validated_url_ = std::move(maybe_url.value());
  }

  opener_tab_id_ = create_properties.opener_tab_id;

  // TODO(jstritar): Add a constant, chrome.tabs.TAB_ID_ACTIVE, that
  // represents the active tab.
  content::WebContents* opener = nullptr;
  if (opener_tab_id_) {
    if (!ExtensionTabUtil::GetTabById(*opener_tab_id_, browser_context(),
                                      include_incognito_information(), nullptr,
                                      &opener, nullptr)) {
      return RespondNow(Error(ErrorUtils::FormatErrorMessage(
          ExtensionTabUtil::kTabNotFoundError,
          base::NumberToString(*opener_tab_id_))));
    }
  }

  // Try to find a suitable browser.
  // TODO(https://crbug.com/468223125): This is a wild set of tangle
  // conditions, most of which are inconsistent.

  BrowserWindowInterface* browser = nullptr;
  std::string error;

  // windowId defaults to "current" window.
  if (WindowController* controller =
          ExtensionTabUtil::GetControllerFromWindowID(
              ChromeExtensionFunctionDetails(this),
              create_properties.window_id.value_or(
                  extension_misc::kCurrentWindowId),
              &error)) {
    browser = controller->GetBrowserWindowInterface();
  }

  // We didn't find a browser. Bail.
  // TODO(https://crbug.com/468223125): This isn't consistent, since sometimes
  // we *will* create a new browser below.
  if (!browser) {
    return RespondNow(Error(std::move(error)));
  }

  // We can't load extension URLs into incognito windows unless the extension
  // uses split mode. Special case to fall back to a tabbed window or, if
  // needed, create one.
  bool needs_original_profile = false;
  if (validated_url_.SchemeIs(kExtensionScheme) &&
      (!extension() || !IncognitoInfo::IsSplitMode(extension()))) {
    needs_original_profile = true;
  }

  bool fallback_to_tabbed_browser = false;
  bool create_if_needed = false;

  // Check if the browser is valid. If it isn't, reset `browser` and possibly
  // find a replacement.

#if !BUILDFLAG(IS_ANDROID)
  // TODO(https://crbug.com/468223125): Why do we check if it's not a normal
  // browser *and* it's attempting to close? Should that be *or*? This goes
  // back to the dawn of time, AKA the initial implementation in 2014:
  // https://codereview.chromium.org/245933002.
  if (browser && browser->GetType() != BrowserWindowInterface::TYPE_NORMAL &&
      browser->GetBrowserForMigrationOnly()->IsAttemptingToCloseBrowser()) {
    browser = nullptr;
    fallback_to_tabbed_browser = true;
  }
#endif

  if (browser && needs_original_profile &&
      browser->GetProfile()->IsOffTheRecord()) {
    browser = nullptr;
    fallback_to_tabbed_browser = true;
    create_if_needed = true;
  }

  // This check (for the opener) comes last. It will fail (by design) if
  // we're intending to create a new browser; that's good, because the new
  // browser would never match the one with the opener.
  if (opener) {
    BrowserWindowInterface* opener_browser =
        browser_window_util::GetBrowserForTabContents(*opener);
    if (!opener_browser || opener_browser != browser) {
      return RespondNow(
          Error("Tab opener must be in the same window as the updated tab."));
    }
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  Profile* profile_to_use =
      needs_original_profile ? profile->GetOriginalProfile() : profile;

  if (!browser && fallback_to_tabbed_browser) {
    // Don't include incognito information if we need the original profile,
    // since the goal is to find a non-incognito browser.
    bool include_incognito =
        include_incognito_information() && !needs_original_profile;
    browser = browser_window_util::GetLastActiveNormalBrowserWithProfile(
        *profile_to_use, include_incognito);
  }

  // Found a suitable browser. Use it!
  if (browser) {
    OpenTabInBrowser(*browser, opener);
    // OpenTabInBrowser() will respond.
    return AlreadyResponded();
  }

  // No suitable existing browser.

  if (!create_if_needed) {
    return RespondNow(Error(ExtensionTabUtil::kNoCurrentWindowError));
  }

  if (GetBrowserWindowCreationStatusForProfile(*profile) !=
      BrowserWindowInterface::CreationStatus::kOk) {
    return RespondNow(Error(ExtensionTabUtil::kBrowserWindowNotAllowed));
  }

  BrowserWindowCreateParams create_params(BrowserWindowInterface::TYPE_NORMAL,
                                          *profile_to_use, user_gesture());
  CreateBrowserWindow(
      std::move(create_params),
      base::BindOnce(&TabsCreateFunction::OnBrowserWindowCreated, this));
  return RespondLater();
}

void TabsCreateFunction::OnBrowserWindowCreated(
    BrowserWindowInterface* browser) {
  if (!browser) {
    Respond(Error(ExtensionTabUtil::kBrowserWindowNotAllowed));
    return;
  }

  browser->GetWindow()->Show();

  // Re-fetch the opener, if one was specified. This call might fail if the
  // opener tab was destroyed while the window was being created. In that case,
  // we silently ignore it (we're committed at this point, since we've already
  // created a new window to show the tab).
  content::WebContents* opener = nullptr;
  if (opener_tab_id_) {
    ExtensionTabUtil::GetTabById(*opener_tab_id_, browser_context(),
                                 include_incognito_information(), nullptr,
                                 &opener, nullptr);
  }

  OpenTabInBrowser(*browser, opener);
}

void TabsCreateFunction::OpenTabInBrowser(BrowserWindowInterface& browser,
                                          content::WebContents* opener_tab) {
  OpenTabHelper::Params options;

  options.active = active_;
  options.pinned = pinned_;
  options.index = index_;

  base::expected<content::WebContents*, std::string> result =
      OpenTabHelper::OpenTab(validated_url_, browser, *this, options);
  if (!result.has_value()) {
    Respond(Error(result.error()));
    return;
  }

  content::WebContents* new_contents = result.value();

#if BUILDFLAG(FULL_SAFE_BROWSING)
  tabs_internal::NotifyExtensionTelemetry(
      Profile::FromBrowserContext(browser_context()), extension(),
      safe_browsing::TabsApiInfo::CREATE,
      /*current_url=*/std::string(), original_url_.value_or(std::string()),
      js_callstack());
#endif

  if (opener_tab) {
    std::string error;
    SetOpenerOfTab(*new_contents, *opener_tab, error);
    // Since we've already created the new browser, we ignore the error (if
    // any).
  }

  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(extension(), source_context_type(),
                                            new_contents);

  // Return data about the created tab only if the extension might use it;
  // otherwise, don't create the object as a (minor) optimization.
  if (has_callback()) {
    Respond(WithArguments(ExtensionTabUtil::CreateTabObject(
                              new_contents, scrub_tab_behavior, extension())
                              .ToValue()));
    return;
  }

  Respond(NoArguments());
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
  content::WebContents* web_contents = nullptr;
  if (!tabs_internal::GetTabById(tab_id, browser_context(),
                                 include_incognito_information(), &window,
                                 &web_contents, &tab_index, &error)) {
    return RespondNow(Error(std::move(error)));
  }
  if (!window) {
    return RespondNow(Error(tabs_constants::kInvalidWindowStateError));
  }
  BrowserWindowInterface* browser = window->GetBrowserWindowInterface();

  if (!browser || !ExtensionTabUtil::IsTabStripEditable()) {
    return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
  }

  TabListInterface* tab_list = TabListInterface::From(browser);
  if (!tab_list) {
    return RespondNow(Error(tabs_constants::kCannotDuplicateTab,
                            base::NumberToString(tab_id)));
  }
  ::tabs::TabInterface* tab_interface =
      ::tabs::TabInterface::MaybeGetFromContents(web_contents);
  // We found the tab above, so we should always, always have a TabInterface
  // for it.
  CHECK(tab_interface);

  ::tabs::TabInterface* new_tab =
      tab_list->DuplicateTab(tab_interface->GetHandle());

  if (!new_tab) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        tabs_constants::kCannotDuplicateTab, base::NumberToString(tab_id))));
  }

  if (!has_callback()) {
    return RespondNow(NoArguments());
  }

  // Duplicated tab may not be in the same window as the original, so find
  // the new window.
  TabListInterface* new_tab_list = nullptr;
  int new_tab_index = -1;
  content::WebContents* new_contents = new_tab->GetContents();
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
  TabListInterface* tab_list = ExtensionTabUtil::GetEditableTabList(
      *window_controller->GetBrowserWindowInterface());
  if (!tab_list) {
    return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
  }

  std::set<int> tab_indices;
  int active_tab_index = -1;
  if (params->highlight_info.tabs.as_integers) {
    std::vector<int>& source = *params->highlight_info.tabs.as_integers;
    // Make sure they actually specified tabs to select.
    if (source.empty()) {
      return RespondNow(Error(kNoHighlightedTabError));
    }

    // By default, we make the first tab in the list active.
    active_tab_index = source[0];

    tab_indices.insert(std::make_move_iterator(source.begin()),
                       std::make_move_iterator(source.end()));
  } else {
    EXTENSION_FUNCTION_VALIDATE(params->highlight_info.tabs.as_integer);
    int tab_index = *params->highlight_info.tabs.as_integer;
    tab_indices.insert(tab_index);
    active_tab_index = tab_index;
  }

  std::set<::tabs::TabHandle> tabs;
#if !BUILDFLAG(IS_ANDROID)
  TabStripModel* tab_strip = window_controller->GetBrowser()->tab_strip_model();
#endif
  for (int index : tab_indices) {
    // Make sure the index is in range.
    if (index < 0 || index >= tab_list->GetTabCount()) {
      return RespondNow(Error(ErrorUtils::FormatErrorMessage(
          kTabIndexNotFoundError, base::NumberToString(index))));
    }

    ::tabs::TabInterface* tab = tab_list->GetTab(index);
    CHECK(tab);
    tabs.insert(tab->GetHandle());

    // TODO(https://crbug.com/480192698): When split tabs are available on
    // android, port this logic.
#if !BUILDFLAG(IS_ANDROID)
    // Extend selection for any split tabs.
    std::optional<split_tabs::SplitTabId> split_id =
        tab_strip->GetSplitForTab(index);
    if (!split_id.has_value()) {
      continue;
    }

    // All the tabs in a split should be contiguous.
    std::vector<::tabs::TabInterface*> split_tabs =
        tab_strip->GetSplitData(split_id.value())->ListTabs();
    size_t start = tab_strip->GetIndexOfTab(split_tabs[0]);
    for (size_t i = start; i < start + split_tabs.size(); ++i) {
      ::tabs::TabInterface* split_tab = tab_list->GetTab(i);
      CHECK(split_tab);
      tabs.insert(split_tab->GetHandle());
    }
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  // We just checked all the indices above (of which active_tab_index is a
  // member), so it must be valid.
  CHECK(active_tab_index >= 0 && active_tab_index <= tab_list->GetTabCount());
  ::tabs::TabInterface* active_tab = tab_list->GetTab(active_tab_index);

  tab_list->HighlightTabs(active_tab->GetHandle(), tabs);

  return RespondNow(
      WithArguments(window_controller->CreateWindowValueForExtension(
          extension(), WindowController::kPopulateTabs,
          source_context_type())));
}

TabsUpdateFunction::TabsUpdateFunction() = default;

ExtensionFunction::ResponseAction TabsUpdateFunction::Run() {
  std::optional<tabs::Update::Params> params =
      tabs::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  int tab_id = -1;
  content::WebContents* contents = nullptr;
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
  if (!window || !ExtensionTabUtil::BrowserSupportsTabs(
                     window->GetBrowserWindowInterface())) {
    return RespondNow(Error(ExtensionTabUtil::kNoCurrentWindowError));
  }

  // Cache the original web contents.
  content::WebContents* original_contents = contents;

  // Update the active (aka selected) tab.
  TabListInterface* tab_list =
      TabListInterface::From(window->GetBrowserWindowInterface());
  CHECK(tab_list);
  if (!UpdateActiveTab(*params, *tab_list, tab_index, error)) {
    return RespondNow(Error(std::move(error)));
  }

  // Update the highlighted tab.
  ::tabs::TabInterface* target_tab = tab_list->GetTab(tab_index);
  CHECK(target_tab);
  if (!UpdateHighlightedTab(*params, *tab_list, *target_tab, error)) {
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
    content::WebContents* opener_contents = nullptr;
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

    if (!SetOpenerOfTab(*original_contents, *opener_contents, error)) {
      return RespondNow(Error(std::move(error)));
    }
  }

  // TODO(https://crbug.com/447211263): Support on desktop android.
#if !BUILDFLAG(IS_ANDROID)
  if (params->update_properties.auto_discardable) {
    bool state = *params->update_properties.auto_discardable;
    resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
        original_contents)
        ->SetAutoDiscardable(state);
  }
#endif

  if (params->update_properties.pinned) {
    // TODO(https://crbug.com/447211263): Support on desktop android.
#if !BUILDFLAG(IS_ANDROID)
    Browser* browser = window->GetBrowser();
    TabStripModel* tab_strip = browser->tab_strip_model();
#endif

    // Bug fix for crbug.com/1197888. Don't let the extension update the tab if
    // the user is dragging tabs.
    if (!ExtensionTabUtil::IsTabStripEditable()) {
      return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
    }

    // TODO(https://crbug.com/447211263): Support on desktop android.
#if !BUILDFLAG(IS_ANDROID)
    bool pinned = *params->update_properties.pinned;
    tab_strip->SetTabPinned(tab_index, pinned);

    // Update the tab index because it may move when being pinned.
    tab_index = tab_strip->GetIndexOfWebContents(contents);
#endif
  }

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  // Navigate the tab to a new location if the url is different.
  if (params->update_properties.url) {
    std::string updated_url = *params->update_properties.url;
    if (window->profile()->IsIncognitoProfile() &&
        !IsURLAllowedInIncognito(GURL(updated_url))) {
      return RespondNow(Error(ErrorUtils::FormatErrorMessage(
          tabs_constants::kURLsNotAllowedInIncognitoError, updated_url)));
    }

    // Get last committed or pending URL.
    std::string current_url = contents->GetVisibleURL().is_valid()
                                  ? contents->GetVisibleURL().spec()
                                  : std::string();

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
    TabListInterface& tab_list,
    int tab_index,
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

  CHECK_LT(tab_index, tab_list.GetTabCount());
  if (tab_list.GetActiveIndex() != tab_index) {
    tab_list.ActivateTab(tab_list.GetTab(tab_index)->GetHandle());
    DCHECK_EQ(tab_index, tab_list.GetActiveIndex());
  }
  return true;
}

bool TabsUpdateFunction::UpdateHighlightedTab(
    const api::tabs::Update::Params& params,
    TabListInterface& tab_list,
    ::tabs::TabInterface& target_tab,
    std::string& error) {
  if (!params.update_properties.highlighted.has_value()) {
    // Nothing to highlight.
    return true;
  }

  bool highlighted = params.update_properties.highlighted.value();
  if (target_tab.IsSelected() == highlighted) {
    // Tab state is already correct.
    return true;
  }

  // Bug fix for crbug.com/1197888. Don't let the extension update the tab
  // if the user is dragging tabs.
  if (!ExtensionTabUtil::IsTabStripEditable()) {
    error = ExtensionTabUtil::kTabStripNotEditableError;
    return false;
  }

  // Generate the set of tabs that should be selected. This should be the
  // current selection, plus or minus the updated tab.
  std::set<::tabs::TabHandle> selected_tabs;
  for (::tabs::TabInterface* tab : tab_list.GetAllTabs()) {
    if (tab->IsSelected()) {
      selected_tabs.insert(tab->GetHandle());
    }
  }

  // Get the list of tabs affected by this update call. This is the specified
  // tab, along with any other tabs in that tab's split.
  std::set<::tabs::TabHandle> affected_tabs;
  std::optional<split_tabs::SplitTabId> split_id = target_tab.GetSplit();
  if (split_id) {
    for (::tabs::TabInterface* tab : tab_list.GetAllTabs()) {
      if (tab->GetSplit() == split_id) {
        affected_tabs.insert(tab->GetHandle());
      }
    }
  } else {
    affected_tabs.insert(target_tab.GetHandle());
  }

  // Add or remove the affected tabs from the split.
  if (highlighted) {
    selected_tabs.insert(affected_tabs.begin(), affected_tabs.end());
  } else {
    for (auto& tab : affected_tabs) {
      selected_tabs.erase(tab);
    }
  }

  if (selected_tabs.size() == 0) {
    // We don't allow no tabs to be selected.
    // TODO(devlin): Should this be an error? It's historically been silently
    // swallowed.
    return true;
  }

  // Determine the new active tab. This is the currently-active tab, unless that
  // tab is the one being unselected, in which case we fall back to the first
  // tab in the selection.
  ::tabs::TabInterface* active_tab = tab_list.GetActiveTab();
  ::tabs::TabHandle tab_to_activate = active_tab->GetHandle();
  if (highlighted) {
    tab_to_activate = target_tab.GetHandle();
  } else if (!selected_tabs.contains(tab_to_activate)) {
    tab_to_activate = *selected_tabs.begin();
  }

  tab_list.HighlightTabs(tab_to_activate, selected_tabs);
  return true;
}

bool TabsUpdateFunction::UpdateURL(content::WebContents* web_contents,
                                   const std::string& url_string,
                                   int tab_id,
                                   std::string* error) {
  auto url = ExtensionTabUtil::PrepareURLForNavigation(url_string, extension(),
                                                       browser_context());
  if (!url.has_value()) {
    *error = std::move(url.error());
    return false;
  }

  content::NavigationController::LoadURLParams load_params(*url);

  // Treat extension-initiated navigations as renderer-initiated so that the URL
  // does not show in the omnibox until it commits.  This avoids URL spoofs
  // since URLs can be opened on behalf of untrusted content.
  load_params.is_renderer_initiated = true;
  // All renderer-initiated navigations need to have an initiator origin.
  load_params.initiator_origin = extension()->origin();
  // |source_site_instance| needs to be set so that a renderer process
  // compatible with |initiator_origin| is picked by Site Isolation.
  load_params.source_site_instance = content::SiteInstance::CreateForURL(
      web_contents->GetBrowserContext(),
      load_params.initiator_origin->GetURL());

  // Marking the navigation as initiated via an API means that the focus
  // will stay in the omnibox - see https://crbug.com/1085779.
  load_params.transition_type = ui::PAGE_TRANSITION_FROM_API;

  base::WeakPtr<content::NavigationHandle> navigation_handle =
      web_contents->GetController().LoadURLWithParams(load_params);
  // Navigation can fail for any number of reasons at the content layer.
  // Unfortunately, we can't provide a detailed error message here, because
  // there are too many possible triggers. At least notify the extension that
  // the update failed.
  if (!navigation_handle) {
    *error = "Navigation rejected.";
    return false;
  }

  DCHECK_EQ(*url,
            web_contents->GetController().GetPendingEntry()->GetVirtualURL());

  return true;
}

ExtensionFunction::ResponseValue TabsUpdateFunction::GetResult(
    content::WebContents* web_contents) {
  if (!has_callback()) {
    return NoArguments();
  }

  return ArgumentList(
      tabs::Get::Results::Create(tabs_internal::CreateTabObjectHelper(
          web_contents, extension(), source_context_type(), nullptr, -1)));
}

ExtensionFunction::ResponseAction TabsMoveFunction::Run() {
  std::optional<tabs::Move::Params> params = tabs::Move::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int new_index = params->move_properties.index;
  const auto& window_id = params->move_properties.window_id;
  base::ListValue tab_values;

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
                               base::ListValue& tab_values,
                               const std::optional<int>& window_id,
                               std::string* error) {
  WindowController* source_window = nullptr;
  content::WebContents* contents = nullptr;
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

    BrowserWindowInterface* target_browser =
        target_controller->GetBrowserWindowInterface();
    int inserted_index =
        MoveTabToWindow(this, tab_id, target_browser, *new_index,
                        /*allow_other_window_types=*/false, error);
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
  TabListInterface* source_tab_list =
      TabListInterface::From(source_window->GetBrowserWindowInterface());
  if (*new_index >= source_tab_list->GetTabCount() || *new_index < 0) {
    *new_index = source_tab_list->GetTabCount() - 1;
  }

  ::tabs::TabInterface* tab = source_tab_list->GetTab(tab_index);
  // We retrieved the tab index for the tab above, so it should always be valid.
  CHECK(tab);

  if (*new_index != tab_index) {
    source_tab_list->MoveTab(tab->GetHandle(), *new_index);
    // The actual new index may be different from requested one if the
    // requested index was invalid.
    *new_index = source_tab_list->GetIndexOfTab(tab->GetHandle());
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

class TabsRemoveFunction::WebContentsDestroyedObserver
    : public content::WebContentsObserver {
 public:
  WebContentsDestroyedObserver(extensions::TabsRemoveFunction* owner,
                               content::WebContents* watched_contents)
      : content::WebContentsObserver(watched_contents), owner_(owner) {}

  ~WebContentsDestroyedObserver() override = default;
  WebContentsDestroyedObserver(const WebContentsDestroyedObserver&) = delete;
  WebContentsDestroyedObserver& operator=(const WebContentsDestroyedObserver&) =
      delete;

  // WebContentsObserver
  void WebContentsDestroyed() override { owner_->TabDestroyed(); }

 private:
  // Guaranteed to outlive this object.
  raw_ptr<TabsRemoveFunction> owner_;
};

TabsRemoveFunction::TabsRemoveFunction() = default;
TabsRemoveFunction::~TabsRemoveFunction() = default;

ExtensionFunction::ResponseAction TabsRemoveFunction::Run() {
  std::optional<tabs::Remove::Params> params =
      tabs::Remove::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  if (params->tab_ids.as_integers) {
    std::vector<int>& tab_ids = *params->tab_ids.as_integers;
    for (int tab_id : tab_ids) {
      if (!RemoveTab(tab_id, &error)) {
        return RespondNow(Error(std::move(error)));
      }
    }
  } else {
    EXTENSION_FUNCTION_VALIDATE(params->tab_ids.as_integer);
    if (!RemoveTab(*params->tab_ids.as_integer, &error)) {
      return RespondNow(Error(std::move(error)));
    }
  }
  triggered_all_tab_removals_ = true;
  DCHECK(!did_respond());
  // WebContentsDestroyed will return the response in most cases, except when
  // the last tab closed immediately (it won't return a response because
  // |triggered_all_tab_removals_| will still be false). In this case we should
  // return the response from here.
  if (remaining_tabs_count_ == 0) {
    return RespondNow(NoArguments());
  }
  return RespondLater();
}

bool TabsRemoveFunction::RemoveTab(int tab_id, std::string* error) {
  WindowController* window = nullptr;
  content::WebContents* contents = nullptr;
  if (!tabs_internal::GetTabById(tab_id, browser_context(),
                                 include_incognito_information(), &window,
                                 &contents, nullptr, error) ||
      !window) {
    return false;
  }

  // Don't let the extension remove a tab if the user is dragging tabs around.
  // TODO(https://crbug.com/482088886): Update this to check all tab lists for
  // a profile.
  TabListInterface* tab_list =
      TabListInterface::From(window->GetBrowserWindowInterface());
  CHECK(tab_list);
  if (!tab_list->IsThisTabListEditable()) {
    *error = ExtensionTabUtil::kTabStripNotEditableError;
    return false;
  }

#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Get last committed or pending URL.
  std::string current_url = contents->GetVisibleURL().is_valid()
                                ? contents->GetVisibleURL().spec()
                                : std::string();
  tabs_internal::NotifyExtensionTelemetry(
      Profile::FromBrowserContext(browser_context()), extension(),
      safe_browsing::TabsApiInfo::REMOVE, current_url,
      /*new_url=*/std::string(), js_callstack());
#endif

  // The tab might not immediately close after calling Close() below, so we
  // should wait until WebContentsDestroyed is called before responding.
  web_contents_destroyed_observers_.push_back(
      std::make_unique<WebContentsDestroyedObserver>(this, contents));
  // Ensure that we're going to keep this class alive until
  // |remaining_tabs_count| reaches zero. This relies on WebContents::Close()
  // always (eventually) resulting in a WebContentsDestroyed() call; otherwise,
  // this function will never respond and may leak.
  AddRef();
  remaining_tabs_count_++;

  // There's a chance that the tab is being dragged, or we're in some other
  // nested event loop. This code path ensures that the tab is safely closed
  // under such circumstances, whereas |TabStripModel::CloseWebContentsAt()|
  // does not.
  contents->Close();
  return true;
}

void TabsRemoveFunction::TabDestroyed() {
  DCHECK_GT(remaining_tabs_count_, 0);
  // One of the tabs we wanted to remove had been destroyed.
  remaining_tabs_count_--;
  // If we've triggered all the tab removals we need, and this is the last tab
  // we're waiting for and we haven't sent a response (it's possible that we've
  // responded earlier in case of errors, etc.), send a response.
  if (triggered_all_tab_removals_ && remaining_tabs_count_ == 0 &&
      !did_respond()) {
    Respond(NoArguments());
  }
  Release();
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

  CHECK(target_window);
  BrowserWindowInterface* target_browser =
      target_window->GetBrowserWindowInterface();
  if (!ExtensionTabUtil::SupportsTabGroups(target_browser)) {
    return RespondNow(
        Error(ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError));
  }
  TabListInterface* tab_list = TabListInterface::From(target_browser);
  if (!tab_list) {
    return RespondNow(
        Error(ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError));
  }

  if (!ExtensionTabUtil::IsTabStripEditable()) {
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
      if (MoveTabToWindow(this, tab_ids[i],
                          target_window->GetBrowserWindowInterface(), -1,
                          /*allow_other_window_types=*/false, &error) < 0) {
        return RespondNow(Error(std::move(error)));
      }
    }
  }

  // Get the resulting tab handles in the target browser. We recalculate these
  // after all tabs are moved so that any callbacks are resolved. The set will
  // dedupe any duplicate tabs.
  std::set<::tabs::TabHandle> tab_handles;
  // TODO(https://crbug.com/447211263): Support on desktop android.
#if !BUILDFLAG(IS_ANDROID)
  TabStripModel* tab_strip = target_window->GetBrowser()->tab_strip_model();
#endif
  for (int tab_id : tab_ids) {
    ::tabs::TabHandle tab_handle;
    if (!GetTabHandleById(tab_id, *browser_context(),
                          include_incognito_information(), &tab_handle,
                          &error)) {
      return RespondNow(Error(std::move(error)));
    }

#if !BUILDFLAG(IS_ANDROID)
    if (tab_handles.count(tab_handle)) {
      continue;
    }

    ::tabs::TabInterface* tab = tab_handle.Get();
    CHECK(tab);

    const std::optional<split_tabs::SplitTabId> split_id = tab->GetSplit();
    if (split_id.has_value()) {
      const std::vector<::tabs::TabInterface*> split_tabs =
          tab_strip->GetSplitData(split_id.value())->ListTabs();
      for (::tabs::TabInterface* split_tab : split_tabs) {
        tab_handles.insert(split_tab->GetHandle());
      }
    } else {
      tab_handles.insert(tab_handle);
    }
#else
    tab_handles.insert(tab_handle);
#endif
  }

  // Get the remaining group metadata and add the tabs to the group.
  // At this point, we assume this is a valid action due to the checks above.

  // Either create a new tab group (if `group` is empty) or add to an existing
  // group. The API requires std::nullopt for a "null" group ID, so convert
  // `group` to a std::optional<>.
  std::optional<tab_groups::TabGroupId> existing_group;
  if (!group.is_empty()) {
    existing_group = group;
  }
  // AddTabsToGroup() can both create a new group or add to an existing group.
  std::optional<tab_groups::TabGroupId> final_group =
      tab_list->AddTabsToGroup(existing_group, tab_handles);
  if (!final_group) {
    return RespondNow(
        Error(ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError));
  }
  group_id = ExtensionTabUtil::GetGroupId(*final_group);
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

  if (!ExtensionTabUtil::IsTabStripEditable()) {
    *error = ExtensionTabUtil::kTabStripNotEditableError;
    return false;
  }

  BrowserWindowInterface* browser_window = window->GetBrowserWindowInterface();
  if (!ExtensionTabUtil::SupportsTabGroups(browser_window)) {
    *error = ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError;
    return false;
  }

  TabListInterface* tab_list = TabListInterface::From(browser_window);
  std::set<::tabs::TabHandle> tabs;

  ::tabs::TabInterface* tab = tab_list->GetTab(tab_index);
  CHECK(tab);
  tabs.insert(tab->GetHandle());

  // TODO(https://crbug.com/480192698): When split tabs are available on
  // android, port this logic.
#if !BUILDFLAG(IS_ANDROID)
  // Extend selection for any split tabs.
  TabStripModel* tab_strip = window->GetBrowser()->tab_strip_model();
  std::optional<split_tabs::SplitTabId> split_id =
      tab_strip->GetSplitForTab(tab_index);
  if (split_id.has_value()) {
    // All the tabs in a split should be contiguous.
    std::vector<::tabs::TabInterface*> split_tabs =
        tab_strip->GetSplitData(split_id.value())->ListTabs();
    size_t start = tab_strip->GetIndexOfTab(split_tabs[0]);
    for (size_t i = start; i < start + split_tabs.size(); ++i) {
      ::tabs::TabInterface* split_tab = tab_list->GetTab(i);
      CHECK(split_tab);
      tabs.insert(split_tab->GetHandle());
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  tab_list->Ungroup(tabs);
  return true;
}

ExtensionFunction::ResponseAction TabsDetectLanguageFunction::Run() {
  std::optional<tabs::DetectLanguage::Params> params =
      tabs::DetectLanguage::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  content::WebContents* contents = nullptr;

  // If |tab_id| is specified, look for it. Otherwise default to selected tab
  // in the current window.
  if (params->tab_id) {
    WindowController* window = nullptr;
    std::string error;
    if (!tabs_internal::GetTabById(*params->tab_id, browser_context(),
                                   include_incognito_information(), &window,
                                   &contents, nullptr, &error)) {
      return RespondNow(Error(std::move(error)));
    }
    // The window will be null for prerender tabs.
    if (!window) {
      return RespondNow(Error(kUnknownErrorDoNotUse));
    }
  } else {
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
  }

  if (contents->GetController().NeedsReload()) {
    // If the tab hasn't been loaded, don't wait for the tab to load.
    return RespondNow(Error(kCannotDetermineLanguageOfUnloadedTab));
  }

  // Language detection is asynchronous.
  return StartLanguageDetection(contents);
}

TabsDetectLanguageFunction::ResponseAction
TabsDetectLanguageFunction::StartLanguageDetection(
    content::WebContents* contents) {
  AddRef();  // Balanced in RespondWithLanguage().

  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(contents);
  if (!chrome_translate_client->GetLanguageState().source_language().empty()) {
    // Delay the callback invocation until after the current JS call has
    // returned.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &TabsDetectLanguageFunction::RespondWithLanguage, this,
            chrome_translate_client->GetLanguageState().source_language()));
    return RespondLater();
  }

  // The tab contents does not know its language yet. Let's wait until it
  // receives it, or until the tab is closed/navigates to some other page.

  // Observe the WebContents' lifetime and navigations.
  Observe(contents);
  // Wait until the language is determined.
  chrome_translate_client->GetTranslateDriver()->AddLanguageDetectionObserver(
      this);
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

void TabsDetectLanguageFunction::OnTranslateDriverDestroyed(
    translate::TranslateDriver* driver) {
  // Typically, we'd return an error in these cases, since we weren't able to
  // detect a valid language. However, this matches the behavior in other cases
  // (like the tab going away), so we aim for consistency.
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
        ->GetTranslateDriver()
        ->RemoveLanguageDetectionObserver(this);
    Observe(nullptr);
    is_observing_ = false;
  }

  Respond(WithArguments(language));
  Release();  // Balanced in Run()
}

// static
bool TabsCaptureVisibleTabFunction::disable_throttling_for_test_ = false;

TabsCaptureVisibleTabFunction::TabsCaptureVisibleTabFunction()
    : chrome_details_(this) {}

WebContentsCaptureClient::ScreenshotAccess
TabsCaptureVisibleTabFunction::GetScreenshotAccess(
    content::WebContents* web_contents) const {
  PrefService* service =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  if (service->GetBoolean(prefs::kDisableScreenshots)) {
    return ScreenshotAccess::kDisabledByPreferences;
  }

  if (ExtensionsBrowserClient::Get()->IsScreenshotRestricted(web_contents)) {
    return ScreenshotAccess::kDisabledByDlp;
  }

  return ScreenshotAccess::kEnabled;
}

bool TabsCaptureVisibleTabFunction::ClientAllowsTransparency() {
  return false;
}

content::WebContents* TabsCaptureVisibleTabFunction::GetWebContentsForID(
    int window_id,
    std::string* error) {
  WindowController* window_controller =
      ExtensionTabUtil::GetControllerFromWindowID(chrome_details_, window_id,
                                                  error);
  if (!window_controller) {
    return nullptr;
  }

  BrowserWindowInterface* browser =
      window_controller->GetBrowserWindowInterface();
  if (!browser) {
    *error = ExtensionTabUtil::kTabStripNotEditableError;
    return nullptr;
  }
  TabListInterface* tab_list = ExtensionTabUtil::GetEditableTabList(*browser);
  if (!tab_list) {
    *error = ExtensionTabUtil::kTabStripNotEditableError;
    return nullptr;
  }
  ::tabs::TabInterface* tab = tab_list->GetActiveTab();
  if (!tab) {
    *error = "No active web contents to capture";
    return nullptr;
  }
  content::WebContents* contents = tab->GetContents();

  if (!extension()->permissions_data()->CanCaptureVisiblePage(
          contents->GetLastCommittedURL(),
          sessions::SessionTabHelper::IdForTab(contents).id(), error,
          extensions::CaptureRequirement::kActiveTabOrAllUrls)) {
    return nullptr;
  }
  return contents;
}

ExtensionFunction::ResponseAction TabsCaptureVisibleTabFunction::Run() {
  using api::extension_types::ImageDetails;

  EXTENSION_FUNCTION_VALIDATE(has_args());
  int context_id = extension_misc::kCurrentWindowId;

  if (args().size() > 0 && args()[0].is_int()) {
    context_id = args()[0].GetInt();
  }

  std::optional<ImageDetails> image_details;
  if (args().size() > 1) {
    image_details = ImageDetails::FromValue(args()[1]);
  }

  std::string error;
  content::WebContents* contents = GetWebContentsForID(context_id, &error);
  if (!contents) {
    return RespondNow(Error(std::move(error)));
  }

#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Get last committed URL.
  std::string current_url = contents->GetLastCommittedURL().is_valid()
                                ? contents->GetLastCommittedURL().spec()
                                : std::string();
  tabs_internal::NotifyExtensionTelemetry(
      Profile::FromBrowserContext(browser_context()), extension(),
      safe_browsing::TabsApiInfo::CAPTURE_VISIBLE_TAB, current_url,
      /*new_url=*/std::string(), js_callstack());
#endif

  // NOTE: CaptureAsync() may invoke its callback from a background thread,
  // hence the BindPostTask().
  const CaptureResult capture_result = CaptureAsync(
      contents, base::OptionalToPtr(image_details),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &TabsCaptureVisibleTabFunction::CopyFromSurfaceComplete, this)));
  if (capture_result == OK) {
    // CopyFromSurfaceComplete might have already responded.
    return did_respond() ? AlreadyResponded() : RespondLater();
  }

  return RespondNow(Error(CaptureResultToErrorMessage(capture_result)));
}

void TabsCaptureVisibleTabFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  constexpr base::TimeDelta kSecond = base::Seconds(1);
  QuotaLimitHeuristic::Config limit = {
      tabs::MAX_CAPTURE_VISIBLE_TAB_CALLS_PER_SECOND, kSecond};

  heuristics->push_back(std::make_unique<QuotaService::TimedLimit>(
      limit, std::make_unique<QuotaLimitHeuristic::SingletonBucketMapper>(),
      "MAX_CAPTURE_VISIBLE_TAB_CALLS_PER_SECOND"));
}

bool TabsCaptureVisibleTabFunction::ShouldSkipQuotaLimiting() const {
  return user_gesture() || disable_throttling_for_test_;
}

void TabsCaptureVisibleTabFunction::OnCaptureSuccess(const SkBitmap& bitmap) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&TabsCaptureVisibleTabFunction::EncodeBitmapOnWorkerThread,
                     this, base::SingleThreadTaskRunner::GetCurrentDefault(),
                     bitmap));
}

void TabsCaptureVisibleTabFunction::EncodeBitmapOnWorkerThread(
    scoped_refptr<base::TaskRunner> reply_task_runner,
    const SkBitmap& bitmap) {
  std::optional<std::string> base64_result = EncodeBitmap(bitmap);
  reply_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&TabsCaptureVisibleTabFunction::OnBitmapEncodedOnUIThread,
                     this, std::move(base64_result)));
}

void TabsCaptureVisibleTabFunction::OnBitmapEncodedOnUIThread(
    std::optional<std::string> base64_result) {
  if (!base64_result) {
    OnCaptureFailure(FAILURE_REASON_ENCODING_FAILED);
    return;
  }

  Respond(WithArguments(std::move(base64_result.value())));
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
    case FAILURE_REASON_SCREEN_SHOTS_DISABLED_BY_DLP:
      return tabs_constants::kScreenshotsDisabledByDlp;
    case OK:
      NOTREACHED() << "CaptureResultToErrorMessage should not be called with a "
                      "successful result";
  }
  return ErrorUtils::FormatErrorMessage("Failed to capture tab: *",
                                        reason_description);
}

ExecuteCodeInTabFunction::ExecuteCodeInTabFunction() = default;
ExecuteCodeInTabFunction::~ExecuteCodeInTabFunction() = default;

ExecuteCodeFunction::InitResult ExecuteCodeInTabFunction::Init() {
  if (init_result_) {
    return init_result_.value();
  }

  if (args().size() < 2) {
    return set_init_result(VALIDATION_FAILURE);
  }

  const auto& tab_id_value = args()[0];
  // |tab_id| is optional so it's ok if it's not there.
  int tab_id = -1;
  if (tab_id_value.is_int()) {
    // But if it is present, it needs to be non-negative.
    tab_id = tab_id_value.GetInt();
    if (tab_id < 0) {
      return set_init_result(VALIDATION_FAILURE);
    }
  }

  // |details| are not optional.
  const base::Value& details_value = args()[1];
  if (!details_value.is_dict()) {
    return set_init_result(VALIDATION_FAILURE);
  }
  auto details =
      api::extension_types::InjectDetails::FromValue(details_value.GetDict());
  if (!details) {
    return set_init_result(VALIDATION_FAILURE);
  }

  // If the tab ID wasn't given then it needs to be converted to the
  // currently active tab's ID.
  if (tab_id == -1) {
    if (WindowController* window_controller =
            chrome_details_.GetCurrentWindowController()) {
      content::WebContents* web_contents = window_controller->GetActiveTab();
      if (!web_contents) {
        // Can happen during shutdown.
        return set_init_result_error(
            tabs_constants::kNoTabInBrowserWindowError);
      }
      tab_id = ExtensionTabUtil::GetTabId(web_contents);
    } else {
      // Can happen during shutdown.
      return set_init_result_error(ExtensionTabUtil::kNoCurrentWindowError);
    }
  }

  execute_tab_id_ = tab_id;
  details_ = std::move(details);
  set_host_id(
      mojom::HostID(mojom::HostID::HostType::kExtensions, extension()->id()));
  return set_init_result(SUCCESS);
}

bool ExecuteCodeInTabFunction::ShouldInsertCSS() const {
  return false;
}

bool ExecuteCodeInTabFunction::ShouldRemoveCSS() const {
  return false;
}

bool ExecuteCodeInTabFunction::CanExecuteScriptOnPage(std::string* error) {
  content::WebContents* contents = nullptr;

  // If |tab_id| is specified, look for the tab. Otherwise default to selected
  // tab in the current window.
  CHECK_GE(execute_tab_id_, 0);
  if (!tabs_internal::GetTabById(execute_tab_id_, browser_context(),
                                 include_incognito_information(), nullptr,
                                 &contents, nullptr, error)) {
    return false;
  }

  CHECK(contents);

  int frame_id = details_->frame_id ? *details_->frame_id
                                    : ExtensionApiFrameIdMap::kTopFrameId;
  content::RenderFrameHost* render_frame_host =
      ExtensionApiFrameIdMap::GetRenderFrameHostById(contents, frame_id);
  if (!render_frame_host) {
    *error = ErrorUtils::FormatErrorMessage(
        kFrameNotFoundError, base::NumberToString(frame_id),
        base::NumberToString(execute_tab_id_));
    return false;
  }

  // Content scripts declared in manifest.json can access frames at about:-URLs
  // if the extension has permission to access the frame's origin, so also allow
  // programmatic content scripts at about:-URLs for allowed origins.
  GURL effective_document_url(render_frame_host->GetLastCommittedURL());
  bool is_about_url = effective_document_url.SchemeIs(url::kAboutScheme);
  if (is_about_url && details_->match_about_blank &&
      *details_->match_about_blank) {
    effective_document_url =
        GURL(render_frame_host->GetLastCommittedOrigin().Serialize());
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
            mojom::APIPermissionID::kTab)) {
      *error = ErrorUtils::FormatErrorMessage(
          manifest_errors::kCannotAccessAboutUrl,
          render_frame_host->GetLastCommittedURL().spec(),
          render_frame_host->GetLastCommittedOrigin().Serialize());
    }
    return false;
  }

  return true;
}

ScriptExecutor* ExecuteCodeInTabFunction::GetScriptExecutor(
    std::string* error) {
  WindowController* window = nullptr;
  content::WebContents* contents = nullptr;

  bool success =
      tabs_internal::GetTabById(execute_tab_id_, browser_context(),
                                include_incognito_information(), &window,
                                &contents, nullptr, error) &&
      contents && window;

  if (!success) {
    return nullptr;
  }

  return TabHelper::FromWebContents(contents)->script_executor();
}

bool ExecuteCodeInTabFunction::IsWebView() const {
  return false;
}

int ExecuteCodeInTabFunction::GetRootFrameId() const {
  return ExtensionApiFrameIdMap::kTopFrameId;
}

const GURL& ExecuteCodeInTabFunction::GetWebViewSrc() const {
  return GURL::EmptyGURL();
}

bool TabsInsertCSSFunction::ShouldInsertCSS() const {
  return true;
}

bool TabsRemoveCSSFunction::ShouldRemoveCSS() const {
  return true;
}

ExtensionFunction::ResponseAction TabsSetZoomFunction::Run() {
  std::optional<tabs::SetZoom::Params> params =
      tabs::SetZoom::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  std::string error;
  content::WebContents* web_contents =
      tabs_internal::GetTabsAPIDefaultWebContents(this, tab_id, &error);
  if (!web_contents) {
    return RespondNow(Error(std::move(error)));
  }

  GURL url(web_contents->GetVisibleURL());
  if (extension()->permissions_data()->IsRestrictedUrl(url, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
  // Android native UI (like the new tab page) may not have a zoom controller.
  if (!zoom_controller) {
    return RespondNow(Error(tabs_constants::kCannotSetZoomThisTabError));
  }
  double zoom_level = params->zoom_factor > 0
                          ? blink::ZoomFactorToZoomLevel(params->zoom_factor)
                          : zoom_controller->GetDefaultZoomLevel();

  auto client = base::MakeRefCounted<ExtensionZoomRequestClient>(extension());
  if (!zoom_controller->SetZoomLevelByClient(zoom_level, client)) {
    // Tried to zoom a tab in disabled mode.
    return RespondNow(Error(tabs_constants::kCannotZoomDisabledTabError));
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsGetZoomFunction::Run() {
  std::optional<tabs::GetZoom::Params> params =
      tabs::GetZoom::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  std::string error;
  content::WebContents* web_contents =
      tabs_internal::GetTabsAPIDefaultWebContents(this, tab_id, &error);
  if (!web_contents) {
    return RespondNow(Error(std::move(error)));
  }

  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
  // Android native UI (like the new tab page) may not have a zoom controller.
  if (!zoom_controller) {
    return RespondNow(Error(tabs_constants::kCannotGetZoomThisTabError));
  }
  const double zoom_level = zoom_controller->GetZoomLevel();
  const double zoom_factor = blink::ZoomLevelToZoomFactor(zoom_level);

  return RespondNow(ArgumentList(tabs::GetZoom::Results::Create(zoom_factor)));
}

ExtensionFunction::ResponseAction TabsSetZoomSettingsFunction::Run() {
  using api::tabs::ZoomSettings;

  std::optional<tabs::SetZoomSettings::Params> params =
      tabs::SetZoomSettings::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  std::string error;
  content::WebContents* web_contents =
      tabs_internal::GetTabsAPIDefaultWebContents(this, tab_id, &error);
  if (!web_contents) {
    return RespondNow(Error(std::move(error)));
  }

  GURL url(web_contents->GetVisibleURL());
  if (extension()->permissions_data()->IsRestrictedUrl(url, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  // "per-origin" scope is only available in "automatic" mode.
  if (params->zoom_settings.scope == tabs::ZoomSettingsScope::kPerOrigin &&
      params->zoom_settings.mode != tabs::ZoomSettingsMode::kAutomatic &&
      params->zoom_settings.mode != tabs::ZoomSettingsMode::kNone) {
    return RespondNow(Error(tabs_constants::kPerOriginOnlyInAutomaticError));
  }

  // Determine the correct internal zoom mode to set |web_contents| to from the
  // user-specified |zoom_settings|.
  zoom::ZoomController::ZoomMode zoom_mode =
      zoom::ZoomController::ZOOM_MODE_DEFAULT;
  switch (params->zoom_settings.mode) {
    case tabs::ZoomSettingsMode::kNone:
    case tabs::ZoomSettingsMode::kAutomatic:
      switch (params->zoom_settings.scope) {
        case tabs::ZoomSettingsScope::kNone:
        case tabs::ZoomSettingsScope::kPerOrigin:
          zoom_mode = zoom::ZoomController::ZOOM_MODE_DEFAULT;
          break;
        case tabs::ZoomSettingsScope::kPerTab:
          zoom_mode = zoom::ZoomController::ZOOM_MODE_ISOLATED;
      }
      break;
    case tabs::ZoomSettingsMode::kManual:
      zoom_mode = zoom::ZoomController::ZOOM_MODE_MANUAL;
      break;
    case tabs::ZoomSettingsMode::kDisabled:
      zoom_mode = zoom::ZoomController::ZOOM_MODE_DISABLED;
  }

  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
  // Android native UI (like the new tab page) may not have a zoom controller.
  if (!zoom_controller) {
    return RespondNow(Error(tabs_constants::kCannotSetZoomThisTabError));
  }
  zoom_controller->SetZoomMode(zoom_mode);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsGetZoomSettingsFunction::Run() {
  std::optional<tabs::GetZoomSettings::Params> params =
      tabs::GetZoomSettings::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  std::string error;
  content::WebContents* web_contents =
      tabs_internal::GetTabsAPIDefaultWebContents(this, tab_id, &error);
  if (!web_contents) {
    return RespondNow(Error(std::move(error)));
  }
  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
  // Android native UI (like the new tab page) may not have a zoom controller.
  if (!zoom_controller) {
    return RespondNow(Error(tabs_constants::kCannotGetZoomThisTabError));
  }

  zoom::ZoomController::ZoomMode zoom_mode = zoom_controller->zoom_mode();
  api::tabs::ZoomSettings zoom_settings;
  ZoomModeToZoomSettings(zoom_mode, &zoom_settings);
  zoom_settings.default_zoom_factor =
      blink::ZoomLevelToZoomFactor(zoom_controller->GetDefaultZoomLevel());

  return RespondNow(
      ArgumentList(api::tabs::GetZoomSettings::Results::Create(zoom_settings)));
}

ExtensionFunction::ResponseAction TabsDiscardFunction::Run() {
  std::optional<tabs::Discard::Params> params =
      tabs::Discard::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  WindowController* window = nullptr;
  content::WebContents* contents = nullptr;

  // If `tab_id` is given, find the web_contents respective to it.
  // Otherwise invoke discard function in TabManager with null web_contents
  // that will discard the least important tab.
  if (params->tab_id) {
    int tab_id = *params->tab_id;
    std::string error;

    int tab_index = -1;
    if (!tabs_internal::GetTabById(tab_id, browser_context(),
                                   include_incognito_information(), &window,
                                   &contents, &tab_index, &error)) {
      return RespondNow(Error(std::move(error)));
    }

    if (DevToolsWindow::IsDevToolsWindow(contents)) {
      return RespondNow(Error(tabs_constants::kNotAllowedForDevToolsError));
    }

    BrowserWindowInterface* browser_window =
        window->GetBrowserWindowInterface();
    if (!browser_window ||
        !ExtensionTabUtil::BrowserSupportsTabs(browser_window)) {
      return RespondNow(Error(ExtensionTabUtil::kNoCurrentWindowError));
    }

    TabListInterface* tab_list = TabListInterface::From(browser_window);
    CHECK(tab_list);

    contents = tab_list->DiscardTab(tab_list->GetTab(tab_index)->GetHandle());
  } else {
    contents = resource_coordinator::DiscardLeastImportantTab(
        ::mojom::LifecycleUnitDiscardReason::EXTERNAL);
  }

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

ExtensionFunction::ResponseAction TabsGoForwardFunction::Run() {
  std::optional<tabs::GoForward::Params> params =
      tabs::GoForward::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  std::string error;
  content::WebContents* web_contents =
      tabs_internal::GetTabsAPIDefaultWebContents(this, tab_id, &error);
  if (!web_contents) {
    return RespondNow(Error(std::move(error)));
  }

  content::NavigationController& controller = web_contents->GetController();
  if (!controller.CanGoForward()) {
    return RespondNow(Error(tabs_constants::kNotFoundNextPageError));
  }

  controller.GoForward();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsGoBackFunction::Run() {
  std::optional<tabs::GoBack::Params> params =
      tabs::GoBack::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  std::string error;
  content::WebContents* web_contents =
      tabs_internal::GetTabsAPIDefaultWebContents(this, tab_id, &error);
  if (!web_contents) {
    return RespondNow(Error(std::move(error)));
  }

  content::NavigationController& controller = web_contents->GetController();
  if (!controller.CanGoBack()) {
    return RespondNow(Error(tabs_constants::kNotFoundNextPageError));
  }

  controller.GoBack();
  return RespondNow(NoArguments());
}

}  // namespace extensions
