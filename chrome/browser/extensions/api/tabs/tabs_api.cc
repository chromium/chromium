// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_api.h"

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notimplemented.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/optional_util.h"
#include "base/unguessable_token.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/tabs_util.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/browser_window_util.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/open_tab_helper.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-forward.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/incognito_allowed_url.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_muted_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/safe_browsing/buildflags.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_interface.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "extensions/browser/api/constants.h"
#include "extensions/browser/extension_user_activation_service.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/base_window.h"
#include "ui/base/page_transition_types.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/unload_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "ash/wm/window_pin_util.h"
#include "chrome/common/chrome_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "chrome/browser/safe_browsing/extension_telemetry/tabs_api_signal.h"
#endif

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/reporting/extension_actor_tracker.h"
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace tabs = api::tabs;

constexpr char kCannotDetermineLanguageOfUnloadedTab[] =
    "Cannot determine language: tab not loaded";
constexpr char kLanguageDetectionNotSupported[] =
    "Language detection is not supported for this page.";
constexpr char kFrameNotFoundError[] = "No frame with id * in tab *.";
constexpr char kCannotUpdateMuteCaptured[] =
    "Cannot update mute state for tab *, tab has audio or video currently "
    "being captured";

namespace {

constexpr char kNoHighlightedTabError[] = "No highlighted tab";
constexpr char kTabIndexNotFoundError[] = "No tab at index: *.";
constexpr char kCannotFindTabToDiscard[] = "Cannot find a tab to discard.";
constexpr char kCannotUnhighlightAllTabsError[] =
    "Cannot unhighlight all tabs.";
constexpr char kTabOpenerMustBeInSameWindowError[] =
    "Tab opener must be in the same window as the updated tab.";

#if !BUILDFLAG(IS_ANDROID)
constexpr char kTabsCreateIwaUrlNotAllowedError[] =
    "URLs with the 'isolated-app:' scheme cannot be opened with tabs.create. "
    "Use windows.create instead.";
constexpr char kTabsUpdateIwaUrlNotAllowedError[] =
    "Cannot navigate to a URL with the 'isolated-app:' scheme via tabs.update. "
    "Use windows.create instead.";
constexpr char kCannotDuplicateIwaTabError[] =
    "The tab of an Isolated Web App cannot be duplicated.";
#endif

// Sets the opener of the given `tab` to `opener`. Returns true on success;
// on failure, populates `error`.
bool SetOpenerOfTab(Profile& profile,
                    ::tabs::TabInterface& tab,
                    ::tabs::TabInterface& opener,
                    std::string& error) {
  // Bug fix for crbug.com/40055514. Don't let the extension update the tab
  // if the user is dragging tabs.
  if (!ExtensionTabUtil::IsTabStripEditable(profile)) {
    error = ExtensionTabUtil::kTabStripNotEditableError;
    return false;
  }

  BrowserWindowInterface* opener_browser =
      browser_window_util::GetBrowserForTabContents(*opener.GetContents());
  BrowserWindowInterface* tab_browser =
      browser_window_util::GetBrowserForTabContents(*tab.GetContents());
  if (!opener_browser || opener_browser != tab_browser) {
    error = kTabOpenerMustBeInSameWindowError;
    return false;
  }

  TabListInterface* tab_list = TabListInterface::From(tab_browser);
  CHECK(tab_list);
  tab_list->SetOpenerForTab(tab.GetHandle(), opener.GetHandle());

  return true;
}

// Returns true if either |boolean| is disengaged, or if |boolean| and
// |value| are equal. This function is used to check if a tab's parameters match
// those of the browser.
bool MatchesBool(const std::optional<bool>& boolean, bool value) {
  return !boolean || *boolean == value;
}

bool GetTabHandleById(int tab_id,
                      content::BrowserContext& context,
                      bool include_incognito,
                      ::tabs::TabHandle* tab_handle_out,
                      std::string* error_out) {
  WindowController* window = nullptr;
  int index = -1;
  if (!tabs_util::GetTabById(tab_id, &context, include_incognito, &window,
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

// Returns all tabs that are in the split indicated by `split_id` within the
// specified `tab_list`.
std::vector<::tabs::TabHandle> GetTabsInSplit(
    const split_tabs::SplitTabId& split_id,
    TabListInterface& tab_list) {
  std::vector<::tabs::TabHandle> split_tabs;
  for (::tabs::TabInterface* tab : tab_list.GetAllTabs()) {
    if (tab->GetSplit() == split_id) {
      split_tabs.push_back(tab->GetHandle());
    }
  }

  return split_tabs;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UpdateActionType {
  // Updates that are not redirects for the default search engine page.
  kOtherUpdates = 0,
  // Updates that are redirects for the default search engine page which are
  // not a result of a user gesture.
  kDSERedirectsWithoutUserGesture = 1,
  // Updates that are redirects for the default search engine page after a user
  // gesture has occurred.
  kDSERedirectsWithUserGesture = 2,
  // Updates that are redirects after the user has landed on the search engine
  // results page (SERP) for a while.
  kDSERedirectsAfterLandingOnSERP = 3,
  // The maximum value of the UpdateActionType enum.
  kMaxValue = kDSERedirectsAfterLandingOnSERP,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RemoveActionType {
  // Removals that are not for the default search engine page.
  kOtherRemovals = 0,
  // Removals of the default search engine page which are not a result of a user
  // gesture.
  kDSERemovalsWithoutUserGesture = 1,
  // Removals of the default search engine page after a user gesture has
  // occurred.
  kDSERemovalsWithUserGesture = 2,
  // Removals of the default search engine page after the user has landed on the
  // search engine results page (SERP) for a while.
  kDSERemovalsAfterLandingOnSERP = 3,
  // The maximum value of the RemoveActionType enum.
  kMaxValue = kDSERemovalsAfterLandingOnSERP,
};

bool HasUserActivation(const ExtensionId& extension_id,
                       content::BrowserContext& browser_context,
                       content::RenderFrameHost* calling_render_frame_host,
                       content::WebContents& tab_web_contents,
                       bool extension_function_user_gesture) {
  return extension_function_user_gesture ||
         ExtensionUserActivationService::Get(&browser_context)
             ->HasTransientActivation(extension_id) ||
         (calling_render_frame_host &&
          calling_render_frame_host->HasTransientUserActivation()) ||
         (tab_web_contents.GetPrimaryMainFrame() &&
          tab_web_contents.GetPrimaryMainFrame()->HasTransientUserActivation());
}

// Returns true if a Tabs API update call is a default search engine (DSE)
// redirect without a user gesture.
// An update is considered a DSE redirect if the source URL is the DSE page and
// the destination URL is not the DSE page, and the update is not a result of a
// user gesture.
bool IsDSERedirect(const ExtensionId& extension_id,
                   content::BrowserContext& browser_context,
                   content::RenderFrameHost* calling_render_frame_host,
                   content::WebContents& tab_web_contents,
                   const GURL& destination_url,
                   bool extension_function_user_gesture) {
  auto is_dse_redirect = [&browser_context, &destination_url,
                          &extension_id](const GURL& source_url) {
    return ExtensionsBrowserClient::Get()->IsDefaultSearchEngineRedirect(
        &browser_context, extension_id, source_url, destination_url);
  };

  // If there is a pending entry, proceed to checking user gestures since the
  // user may have not yet landed on the DSE page.
  content::NavigationEntry* entry =
      tab_web_contents.GetController().GetPendingEntry();
  if (!entry) {
    entry = tab_web_contents.GetController().GetLastCommittedEntry();
    // Assume no redirect if user has landed on the DSE page for a while.
    if (entry && base::Time::Now() - entry->GetTimestamp() > base::Seconds(5)) {
      base::UmaHistogramEnumeration(
          "Extensions.Tabs.UpdateAction",
          is_dse_redirect(entry->GetURL())
              ? UpdateActionType::kDSERedirectsAfterLandingOnSERP
              : UpdateActionType::kOtherUpdates);
      return false;
    }
  }
  if (!entry || !is_dse_redirect(entry->GetURL())) {
    base::UmaHistogramEnumeration("Extensions.Tabs.UpdateAction",
                                  UpdateActionType::kOtherUpdates);
    return false;
  }
  bool has_user_activation = HasUserActivation(
      extension_id, browser_context, calling_render_frame_host,
      tab_web_contents, extension_function_user_gesture);
  base::UmaHistogramEnumeration(
      "Extensions.Tabs.UpdateAction",
      has_user_activation ? UpdateActionType::kDSERedirectsWithUserGesture
                          : UpdateActionType::kDSERedirectsWithoutUserGesture);
  return !has_user_activation;
}

// Returns true if a Tabs API remove call is a default search engine (DSE)
// removal without a user gesture.
// A removal is considered a DSE removal if the source URL is the DSE page and
// the removal is not a result of a user gesture.
bool IsDSERemoval(const ExtensionId& extension_id,
                  content::BrowserContext& browser_context,
                  content::RenderFrameHost* calling_render_frame_host,
                  content::WebContents& tab_web_contents,
                  bool extension_function_user_gesture) {
  // If the tab was created in the background and NEVER became the active
  // foreground tab, it could be an automated pop-under/background tab.
  if (tab_web_contents.GetVisibility() != content::Visibility::VISIBLE &&
      tab_web_contents.HasOpener()) {
    base::UmaHistogramEnumeration("Extensions.Tabs.RemoveAction",
                                  RemoveActionType::kOtherRemovals);
    return false;
  }

  auto is_dse = [&browser_context, &extension_id](const GURL& source_url) {
    return ExtensionsBrowserClient::Get()->IsDefaultSearchEngineRedirect(
        &browser_context, extension_id, source_url, GURL());
  };

  // If there is a pending entry, proceed to checking user gestures since the
  // user may have not yet landed on the DSE page.
  content::NavigationEntry* entry =
      tab_web_contents.GetController().GetPendingEntry();
  if (!entry) {
    entry = tab_web_contents.GetController().GetLastCommittedEntry();
    // Assume no removal if user has landed on the DSE page for a while.
    if (entry && base::Time::Now() - entry->GetTimestamp() > base::Seconds(5)) {
      base::UmaHistogramEnumeration(
          "Extensions.Tabs.RemoveAction",
          is_dse(entry->GetURL())
              ? RemoveActionType::kDSERemovalsAfterLandingOnSERP
              : RemoveActionType::kOtherRemovals);
      return false;
    }
  }
  if (!entry || !is_dse(entry->GetURL())) {
    base::UmaHistogramEnumeration("Extensions.Tabs.RemoveAction",
                                  RemoveActionType::kOtherRemovals);
    return false;
  }
  bool has_user_activation = HasUserActivation(
      extension_id, browser_context, calling_render_frame_host,
      tab_web_contents, extension_function_user_gesture);
  base::UmaHistogramEnumeration(
      "Extensions.Tabs.RemoveAction",
      has_user_activation ? RemoveActionType::kDSERemovalsWithUserGesture
                          : RemoveActionType::kDSERemovalsWithoutUserGesture);
  return !has_user_activation;
}

// Helper method to generate a new tab object for the given `contents`,
// appropriately scrubbed of data for the given `extension`.
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

#if BUILDFLAG(FULL_SAFE_BROWSING)
// Notifies the safe browsing telemetry service of a relevant extension action.
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

// Gets the WebContents for `tab_id` if it is specified. Otherwise get the
// WebContents for the active tab in the `function`'s current window.
// Returns nullptr and fills `error` if failed.
content::WebContents* GetTabsAPIDefaultWebContents(ExtensionFunction* function,
                                                   int tab_id,
                                                   std::string* error) {
  content::WebContents* web_contents = nullptr;
  if (tab_id != -1) {
    // We assume this call leaves web_contents unchanged if it is unsuccessful.
    tabs_util::GetTabById(tab_id, function->browser_context(),
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

}  // namespace

void ZoomModeToZoomSettings(zoom::ZoomController::ZoomMode zoom_mode,
                            api::tabs::ZoomSettings* zoom_settings) {
  CHECK(zoom_settings, base::NotFatalUntil::M161);
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

ExtensionFunction::ResponseAction TabsGetFunction::Run() {
  std::optional<tabs::Get::Params> params = tabs::Get::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  int tab_id = params->tab_id;

  WindowController* window = nullptr;
  content::WebContents* contents = nullptr;
  int tab_index = -1;
  std::string error;
  if (!tabs_util::GetTabById(tab_id, browser_context(),
                             include_incognito_information(), &window,
                             &contents, &tab_index, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  return RespondNow(
      ArgumentList(tabs::Get::Results::Create(CreateTabObjectHelper(
          contents, extension(), source_context_type(),
          window ? window->GetBrowserWindowInterface() : nullptr, tab_index))));
}

ExtensionFunction::ResponseAction TabsGetCurrentFunction::Run() {
  CHECK(dispatcher(), base::NotFatalUntil::M161);

  // If called from a tab, return the details from that tab. If not called from
  // a tab, return nothing (making the returned value undefined to the
  // extension), rather than an error.
  content::WebContents* caller_contents = GetSenderWebContents();
  if (caller_contents && ExtensionTabUtil::GetTabId(caller_contents) >= 0) {
    return RespondNow(ArgumentList(tabs::Get::Results::Create(
        CreateTabObjectHelper(caller_contents, extension(),
                              source_context_type(), nullptr, -1))));
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

  return RespondNow(
      ArgumentList(tabs::Get::Results::Create(CreateTabObjectHelper(
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

      result.Append(CreateTabObjectHelper(tab->GetContents(), extension(),
                                          source_context_type(), browser, i)
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
  // Some browser candidates don't have window controllers.
  // https://crbug.com/501003339
  if (!window_controller) {
    return false;
  }
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
  const bool is_audible =
      audible_helper && audible_helper->WasRecentlyAudible();
  if (!MatchesBool(query_info_.audible, is_audible)) {
    return false;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  auto* tab_lifecycle_unit_external =
      resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
          web_contents);

  // TODO(https://crbug.com/505306735): Add support (or appropriately handle)
  // tab freezing, discarding, and auto-discarding on desktop android.
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

  // "title" and "url" properties are considered privileged data and can
  // only be checked if the extension has access to the tab's data.
  // Otherwise, this tab is considered not matched.
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(extension(), source_context_type(),
                                            web_contents);
  if (query_info_.title && !query_info_.title->empty()) {
    bool matches_title =
        scrub_tab_behavior.committed_info != ExtensionTabUtil::kScrubTabFully &&
        base::MatchPattern(web_contents->GetTitle(),
                           base::UTF8ToUTF16(*query_info_.title));
    if (!matches_title) {
      return false;
    }
  }

  if (!target_url_patterns.is_empty()) {
    bool matches_committed =
        scrub_tab_behavior.committed_info != ExtensionTabUtil::kScrubTabFully &&
        target_url_patterns.MatchesURL(web_contents->GetLastCommittedURL());

    content::NavigationEntry* pending_entry =
        web_contents->GetController().GetPendingEntry();
    bool matches_pending =
        pending_entry &&
        scrub_tab_behavior.pending_info != ExtensionTabUtil::kScrubTabFully &&
        target_url_patterns.MatchesURL(pending_entry->GetVirtualURL());

    // Note: It's okay to indicate the tab matched even if it only has access
    // to one of [pending, committed]. The tab will be scrubbed appropriately
    // when it's returned below.
    if (!matches_committed && !matches_pending) {
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

  const tabs::Create::Params::CreateProperties& create_properties =
      params->create_properties;

  // The 'active' property has replaced the 'selected' property.
  active_ = create_properties.active ? create_properties.active
                                     : create_properties.selected;

  pinned_ = create_properties.pinned;
  index_ = create_properties.index;
  original_url_ = std::move(create_properties.url);
  split_with_tab_id_ = create_properties.split_with_tab_id;

#if BUILDFLAG(IS_ANDROID)
  // TODO(https://crbug.com/480192698): Remove this restriction once split tabs
  // are supported on Desktop Android.
  if (split_with_tab_id_) {
    return RespondNow(Error(tabs_constants::kSplitViewCreationFailedError));
  }
#endif

  validated_url_ = chrome::ChromeUINewTabURLAsGURL();
  if (original_url_) {
    base::expected<GURL, std::string> maybe_url =
        ExtensionTabUtil::PrepareURLForNavigation(*original_url_, extension(),
                                                  browser_context());
    if (!maybe_url.has_value()) {
      return RespondNow(Error(maybe_url.error()));
    }
    validated_url_ = std::move(maybe_url.value());
  }

#if !BUILDFLAG(IS_ANDROID)
  // Isolated Web Apps must be opened at their start URL with the requested
  // URL routed via launchQueue, which is handled by `windows.create`.
  if (validated_url_.SchemeIs(webapps::kIsolatedAppScheme)) {
    return RespondNow(Error(kTabsCreateIwaUrlNotAllowedError));
  }
#endif

  opener_tab_id_ = create_properties.opener_tab_id;

  // TODO(jstritar): Add a constant, chrome.tabs.TAB_ID_ACTIVE, that
  // represents the active tab.
  content::WebContents* opener = nullptr;
  if (opener_tab_id_) {
    if (!ExtensionTabUtil::GetTabById(*opener_tab_id_, browser_context(),
                                      include_incognito_information(), nullptr,
                                      &opener, nullptr)) {
      return RespondNow(Error(ErrorUtils::FormatErrorMessage(
          kTabNotFoundError, base::NumberToString(*opener_tab_id_))));
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
    if (error.empty()) {
      error = ExtensionTabUtil::kTabStripNotEditableError;
    }
    return RespondNow(Error(std::move(error)));
  }

  if (base::FeatureList::IsEnabled(extensions_features::kApiTabsSplitView)) {
    if (split_with_tab_id_) {
      int target_index = -1;
      WindowController* target_window_controller = nullptr;
      content::WebContents* target_contents = nullptr;
      // 1. Check that the split-with tab exists.
      if (!ExtensionTabUtil::GetTabById(*split_with_tab_id_, browser_context(),
                                        include_incognito_information(),
                                        &target_window_controller,
                                        &target_contents, &target_index)) {
        return RespondNow(Error(ErrorUtils::FormatErrorMessage(
            kTabNotFoundError, base::NumberToString(*split_with_tab_id_))));
      }

      // 2. Check that the split-with tab is backed by a TabInterface
      // (standalone windows such as Document PiP do not have one) and is not
      // already in a split view.
      ::tabs::TabInterface* target_tab =
          ::tabs::TabInterface::MaybeGetFromContents(target_contents);
      if (!target_tab) {
        return RespondNow(Error(tabs_constants::kSplitViewCreationFailedError));
      }
      if (target_tab->IsSplit()) {
        return RespondNow(Error(ErrorUtils::FormatErrorMessage(
            tabs_constants::kSplitWithTabAlreadyInSplitViewError,
            base::NumberToString(*split_with_tab_id_))));
      }

      // 3. Check that the split-with tab is in the same window as the new tab.
      BrowserWindowInterface* split_with_browser =
          target_window_controller
              ? target_window_controller->GetBrowserWindowInterface()
              : nullptr;
      if (split_with_browser != browser) {
        return RespondNow(Error(ErrorUtils::FormatErrorMessage(
            tabs_constants::kSplitWithTabsMatchingStateError,
            tabs_constants::kWindowIdKey)));
      }

      // 4. Check that the index (if specified) is adjacent to the split-with
      // tab.
      if (create_properties.index) {
        int index = *create_properties.index;
        if (index < target_index || index > target_index + 1) {
          return RespondNow(
              Error(tabs_constants::kSplitWithTabIndexNotAdjacentError));
        }
      }
    }
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
      UnloadController::From(browser)->is_attempting_to_close_browser()) {
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

  // TODO(crbug.com/491910697): This is a short-term solution for Android to
  // ensure new tabs are routed to a tabbed browser when the current browser
  // is non-NORMAL (e.g., a PWA). The long-term goal is to unify this with
  // the cross-platform logic below by making the tab creation process
  // (specifically OpenTabHelper::OpenTab) asynchronous, which is required
  // on Android when a new window needs to be created.
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/496733610): Supporting CCT/PWA/TWA is currently not possible
  // in C++ browser tests on Android. Add tests once that's supported.
  if (browser && browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    browser = nullptr;
    fallback_to_tabbed_browser = true;
    create_if_needed = true;
  }
#endif

  // This check (for the opener) comes last. It will fail (by design) if
  // we're intending to create a new browser; that's good, because the new
  // browser would never match the one with the opener.
  if (opener) {
    BrowserWindowInterface* opener_browser =
        browser_window_util::GetBrowserForTabContents(*opener);
    if (!opener_browser || opener_browser != browser) {
      return RespondNow(Error(kTabOpenerMustBeInSameWindowError));
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

  if (!ExtensionTabUtil::IsTabStripEditable(*profile_to_use)) {
    return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
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

  base::WeakPtr<BrowserWindowInterface> weak_browser = browser->GetWeakPtr();
  browser->GetWindow()->Show();
  if (!weak_browser) {
    Respond(Error(ExtensionTabUtil::kNoCurrentWindowError));
    return;
  }

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
  options.split_with_tab_id = split_with_tab_id_;

  base::expected<content::WebContents*, std::string> result =
      OpenTabHelper::OpenTab(validated_url_, browser, *this, options);
  if (!result.has_value()) {
    Respond(Error(result.error()));
    return;
  }

  content::WebContents* new_contents = result.value();

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  if (extension()) {
    enterprise_connectors::ExtensionActorTracker::SetActiveActor(
        new_contents, extension_id(), static_function_name());
  }
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

#if BUILDFLAG(FULL_SAFE_BROWSING)
  NotifyExtensionTelemetry(Profile::FromBrowserContext(browser_context()),
                           extension(), safe_browsing::TabsApiInfo::CREATE,
                           /*current_url=*/std::string(),
                           original_url_.value_or(std::string()),
                           js_callstack());
#endif

  if (opener_tab) {
    std::string error;

    // We know these should never be null:
    // * We just created the tab in OpenTabHelper::OpenTab() above, and verified
    //   it returned a valid contents.
    // * We verified in TabsCreateFunction::Run() (via
    //   browser_window_util::GetBrowserForTabContents()) that `opener_tab`
    //   belongs to the target browser window, so there should always be a
    //   TabInterface for it.
    ::tabs::TabInterface* tab_interface =
        ::tabs::TabInterface::GetFromContents(new_contents);
    CHECK(tab_interface);
    ::tabs::TabInterface* opener_interface =
        ::tabs::TabInterface::GetFromContents(opener_tab);
    CHECK(opener_interface);
    Profile* profile =
        Profile::FromBrowserContext(new_contents->GetBrowserContext());
    SetOpenerOfTab(*profile, *tab_interface, *opener_interface, error);

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
  int tab_id = params->tab_id;

  WindowController* window = nullptr;
  int tab_index = -1;
  std::string error;
  content::WebContents* web_contents = nullptr;
  if (!tabs_util::GetTabById(tab_id, browser_context(),
                             include_incognito_information(), &window,
                             &web_contents, &tab_index, &error)) {
    return RespondNow(Error(std::move(error)));
  }
  if (!window) {
    return RespondNow(Error(tabs_constants::kInvalidWindowStateError));
  }
  BrowserWindowInterface* browser = window->GetBrowserWindowInterface();

  if (!browser ||
      !ExtensionTabUtil::IsTabStripEditable(*browser->GetProfile())) {
    return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
  }

#if !BUILDFLAG(IS_ANDROID)
  if (web_contents->GetLastCommittedURL().SchemeIs(
          webapps::kIsolatedAppScheme)) {
    return RespondNow(Error(kCannotDuplicateIwaTabError));
  }
#endif

  TabListInterface* tab_list = TabListInterface::From(browser);
  if (!tab_list) {
    return RespondNow(Error(tabs_constants::kCannotDuplicateTab,
                            base::NumberToString(tab_id)));
  }

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/496733610): Supporting CCT/PWA/TWA is currently not possible
  // in C++ browser tests on Android. Add tests once that's supported.
  if (browser->GetType() == BrowserWindowInterface::TYPE_CUSTOM_TAB ||
      browser->GetType() == BrowserWindowInterface::TYPE_APP) {
    return RespondNow(Error(
        tabs_constants::kAndroidCannotDuplicateTabInCctOrWebAppWindowError));
  }
#endif

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
  BrowserWindowInterface* browser =
      window_controller->GetBrowserWindowInterface();
  if (!browser) {
    return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
  }
  TabListInterface* tab_list = ExtensionTabUtil::GetEditableTabList(*browser);
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
  for (int index : tab_indices) {
    // Make sure the index is in range.
    if (index < 0 || index >= tab_list->GetTabCount()) {
      return RespondNow(Error(ErrorUtils::FormatErrorMessage(
          kTabIndexNotFoundError, base::NumberToString(index))));
    }

    ::tabs::TabInterface* tab = tab_list->GetTab(index);
    CHECK(tab);
    tabs.insert(tab->GetHandle());

    // Extend selection for any split tabs.
    std::optional<split_tabs::SplitTabId> split_id = tab->GetSplit();
    if (!split_id.has_value()) {
      continue;
    }

    // All the tabs in a split should be contiguous.
    std::vector<::tabs::TabHandle> split_tabs =
        GetTabsInSplit(*split_id, *tab_list);
    tabs.insert(split_tabs.begin(), split_tabs.end());
  }

  // We just checked all the indices above (of which active_tab_index is a
  // member), so it must be valid.
  CHECK(active_tab_index >= 0 && active_tab_index <= tab_list->GetTabCount());
  ::tabs::TabInterface* active_tab = tab_list->GetTab(active_tab_index);

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/496733610): Supporting CCT/PWA/TWA is currently not possible
  // in C++ browser tests on Android. Add tests once that's supported.
  auto browser_type = browser->GetType();
  if ((browser_type == BrowserWindowInterface::TYPE_CUSTOM_TAB ||
       browser_type == BrowserWindowInterface::TYPE_APP) &&
      active_tab_index != tab_list->GetActiveIndex()) {
    return RespondNow(Error(
        tabs_constants::kAndroidCannotHighlightTabInCctOrWebAppWindowError));
  }
#endif

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
  if (!tabs_util::GetTabById(tab_id, browser_context(),
                             include_incognito_information(), &window,
                             &contents, &tab_index, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  if (DevToolsWindow::IsDevToolsWindow(contents)) {
    return RespondNow(Error(tabs_constants::kNotAllowedForDevToolsError));
  }

  // GetTabById may return a null window for prerender tabs.
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
  if (!UpdateActiveTab(*params, *window->profile(),
                       *window->GetBrowserWindowInterface(), *tab_list,
                       tab_index, error)) {
    return RespondNow(Error(std::move(error)));
  }

  // Update the highlighted tab.
  ::tabs::TabInterface* target_tab = tab_list->GetTab(tab_index);
  CHECK(target_tab);
  if (!UpdateHighlightedTab(*params, *window->profile(), *tab_list, *target_tab,
                            error)) {
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
      return RespondNow(Error(ErrorUtils::FormatErrorMessage(
          kTabNotFoundError, base::NumberToString(opener_id))));
    }

    ::tabs::TabInterface* opener_tab =
        ::tabs::TabInterface::MaybeGetFromContents(opener_contents);
    if (!opener_tab) {
      return RespondNow(Error(kTabOpenerMustBeInSameWindowError));
    }
    if (!SetOpenerOfTab(*window->profile(), *target_tab, *opener_tab, error)) {
      return RespondNow(Error(std::move(error)));
    }
  }

  // TODO(https://crbug.com/505306735): Support on desktop android.
#if !BUILDFLAG(IS_ANDROID)
  if (params->update_properties.auto_discardable) {
    bool state = *params->update_properties.auto_discardable;
    resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
        original_contents)
        ->SetAutoDiscardable(state);
  }
#endif

  if (params->update_properties.pinned) {
    bool pinned = *params->update_properties.pinned;

    if (target_tab->IsPinned() != pinned) {
      // Bug fix for crbug.com/40055514. Don't let the extension update the tab
      // if the user is dragging tabs.
      if (!ExtensionTabUtil::IsTabStripEditable(*window->profile())) {
        return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
      }

      ::tabs::TabHandle target_handle = target_tab->GetHandle();

      if (pinned) {
        tab_list->PinTab(target_handle);
      } else {
        tab_list->UnpinTab(target_handle);
      }

      // Update the tab index because it may move when being pinned.
      tab_index = tab_list->GetIndexOfTab(target_handle);
    }
  }

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  // Navigate the tab to a new location if the url is different.
  if (params->update_properties.url) {
    std::string updated_url = *params->update_properties.url;
    if (window->profile()->IsPrimaryOTRProfileWithRegularParent() &&
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

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
    if (extension()) {
      enterprise_connectors::ExtensionActorTracker::SetActiveActor(
          original_contents, extension_id(), static_function_name());
    }
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

#if BUILDFLAG(FULL_SAFE_BROWSING)
    NotifyExtensionTelemetry(Profile::FromBrowserContext(browser_context()),
                             extension(), safe_browsing::TabsApiInfo::UPDATE,
                             current_url, updated_url, js_callstack());
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
  if (!ExtensionTabUtil::IsTabStripEditable(*window_controller->profile())) {
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
    Profile& profile,
    BrowserWindowInterface& browser,
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

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/496733610): Supporting CCT/PWA/TWA is currently not possible
  // in C++ browser tests on Android. Add tests once that's supported.
  auto browser_type = browser.GetType();
  if ((browser_type == BrowserWindowInterface::TYPE_CUSTOM_TAB ||
       browser_type == BrowserWindowInterface::TYPE_APP) &&
      tab_index != tab_list.GetActiveIndex()) {
    error = tabs_constants::kAndroidCannotActivateTabInCctOrWebAppWindowError;
    return false;
  }
#endif

  // Bug fix for crbug.com/40055514. Don't let the extension update the tab
  // if the user is dragging tabs.
  if (!ExtensionTabUtil::IsTabStripEditable(profile)) {
    error = ExtensionTabUtil::kTabStripNotEditableError;
    return false;
  }

  CHECK_LT(tab_index, tab_list.GetTabCount());
  if (tab_list.GetActiveIndex() != tab_index) {
    tab_list.ActivateTab(tab_list.GetTab(tab_index)->GetHandle());
    CHECK_EQ(tab_index, tab_list.GetActiveIndex(), base::NotFatalUntil::M161);
  }
  return true;
}

bool TabsUpdateFunction::UpdateHighlightedTab(
    const api::tabs::Update::Params& params,
    Profile& profile,
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

  // Bug fix for crbug.com/40055514. Don't let the extension update the tab
  // if the user is dragging tabs.
  if (!ExtensionTabUtil::IsTabStripEditable(profile)) {
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
  std::vector<::tabs::TabHandle> affected_tabs;
  std::optional<split_tabs::SplitTabId> split_id = target_tab.GetSplit();
  if (split_id) {
    affected_tabs = GetTabsInSplit(*split_id, tab_list);
  } else {
    affected_tabs.push_back(target_tab.GetHandle());
  }

  // Add or remove the affected tabs from the split.
  if (highlighted) {
    selected_tabs.insert(affected_tabs.begin(), affected_tabs.end());
  } else {
    for (auto& affected_tab : affected_tabs) {
      selected_tabs.erase(affected_tab);
    }
  }

  if (selected_tabs.empty()) {
    // We don't allow no tabs to be selected.
    error = kCannotUnhighlightAllTabsError;
    return false;
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

#if !BUILDFLAG(IS_ANDROID)
  // Isolated Web Apps must be opened at their start URL with the requested
  // URL routed via launchQueue, which is handled by `windows.create`.
  if (url->SchemeIs(webapps::kIsolatedAppScheme)) {
    *error = kTabsUpdateIwaUrlNotAllowedError;
    return false;
  }
#endif

  if (IsDSERedirect(extension()->id(), *browser_context(), render_frame_host(),
                    *web_contents, *url, user_gesture())) {
    ukm::builders::Extensions_Tabs_UpdateDSE(
        ukm::UkmRecorder::GetSourceIdForExtensionUrl(
            base::PassKey<TabsUpdateFunction>(), extension()->url()))
        .SetSeen(true)
        .Record(ukm::UkmRecorder::Get());
    ukm::builders::Extensions_SearchRedirect(
        ukm::UkmRecorder::GetSourceIdForRedirectUrl(
            base::PassKey<TabsUpdateFunction>(), *url))
        .SetApi(
            static_cast<int64_t>(ExtensionSearchRedirectedByApi::kTabsUpdate))
        .Record(ukm::UkmRecorder::Get());
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
  // will stay in the omnibox - see https://crbug.com/40693812.
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

  CHECK_EQ(*url,
           web_contents->GetController().GetPendingEntry()->GetVirtualURL(),
           base::NotFatalUntil::M161);

  return true;
}

ExtensionFunction::ResponseValue TabsUpdateFunction::GetResult(
    content::WebContents* web_contents) {
  if (!has_callback()) {
    return NoArguments();
  }

  return ArgumentList(tabs::Get::Results::Create(CreateTabObjectHelper(
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
  if (!tabs_util::GetTabById(tab_id, browser_context(),
                             include_incognito_information(), &source_window,
                             &contents, &tab_index, error)) {
    return false;
  }

  if (!source_window) {
    *error = tabs_constants::kInvalidWindowStateError;
    return false;
  }

  if (DevToolsWindow::IsDevToolsWindow(contents)) {
    *error = tabs_constants::kNotAllowedForDevToolsError;
    return false;
  }

  // Don't let the extension move the tab if the user is dragging tabs.
  if (!ExtensionTabUtil::IsTabStripEditable(*source_window->profile())) {
    *error = ExtensionTabUtil::kTabStripNotEditableError;
    return false;
  }

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/496733610): Supporting CCT/PWA/TWA is currently not possible
  // in C++ browser tests on Android. Add tests once that's supported
  BrowserWindowInterface* source_browser =
      source_window->GetBrowserWindowInterface();
  bool is_source_window_cct_or_app_on_android =
      source_browser &&
      (source_browser->GetType() == BrowserWindowInterface::TYPE_CUSTOM_TAB ||
       source_browser->GetType() == BrowserWindowInterface::TYPE_APP);
#endif

  WindowController* target_controller = nullptr;
  if (window_id) {
    target_controller = ExtensionTabUtil::GetControllerFromWindowID(
        ChromeExtensionFunctionDetails(this), *window_id, error);
    if (!target_controller) {
      return false;
    }
  }

  // WINDOW_ID_CURRENT can resolve to the tab's own window.
  if (target_controller && target_controller->GetWindowId() !=
                               ExtensionTabUtil::GetWindowIdOfTab(contents)) {
#if BUILDFLAG(IS_ANDROID)
    if (is_source_window_cct_or_app_on_android &&
        contents != source_window->GetActiveTab()) {
      *error = tabs_constants::
          kAndroidOnlyActiveTabCanBeMovedFromCctOrWebAppWindowError;
      return false;
    }
#endif

#if BUILDFLAG(IS_ANDROID)
    if (is_source_window_cct_or_app_on_android &&
        (!target_controller->GetBrowserWindowInterface() ||
         target_controller->GetBrowserWindowInterface()->GetType() !=
             BrowserWindowInterface::TYPE_NORMAL)) {
      *error =
          tabs_constants::kAndroidCanOnlyMoveCctOrWebAppTabsToNormalWindowError;
      return false;
    }
#endif

    BrowserWindowInterface* target_browser =
        target_controller->GetBrowserWindowInterface();
    int inserted_index =
        tabs_util::MoveTabToWindow(this, tab_id, target_browser, *new_index,
                                   /*allow_other_window_types=*/false, error);
    if (inserted_index < 0) {
      return false;
    }

    *new_index = inserted_index;

    if (has_callback()) {
      content::WebContents* web_contents =
          target_controller->GetWebContentsAt(inserted_index);

      tab_values.Append(CreateTabObjectHelper(web_contents, extension(),
                                              source_context_type(),
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

#if BUILDFLAG(IS_ANDROID)
  if (is_source_window_cct_or_app_on_android) {
    *error = tabs_constants::kAndroidCannotMoveTabsWithinCctOrWebAppWindowError;
    return false;
  }
#endif

  TabListInterface* source_tab_list =
      TabListInterface::From(source_window->GetBrowserWindowInterface());
  if (!source_tab_list) {
    *error = ExtensionTabUtil::kTabStripNotEditableError;
    return false;
  }
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
    tab_values.Append(
        CreateTabObjectHelper(contents, extension(), source_context_type(),
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
    if (!tabs_util::GetTabById(tab_id, browser_context(),
                               include_incognito_information(), nullptr,
                               &web_contents, nullptr, &error)) {
      return RespondNow(Error(std::move(error)));
    }
  }

  if (PictureInPictureWindowManager::IsChildWebContents(web_contents)) {
    return RespondNow(
        Error(tabs_constants::kNotAllowedForPictureInPictureError));
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
  CHECK(!did_respond(), base::NotFatalUntil::M161);
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
  if (!tabs_util::GetTabById(tab_id, browser_context(),
                             include_incognito_information(), &window,
                             &contents, nullptr, error) ||
      !window) {
    return false;
  }

  if (IsDSERemoval(extension()->id(), *browser_context(), render_frame_host(),
                   *contents, user_gesture())) {
    ukm::builders::Extensions_Tabs_RemoveDSE(
        ukm::UkmRecorder::GetSourceIdForExtensionUrl(
            base::PassKey<TabsRemoveFunction>(), extension()->url()))
        .SetSeen(true)
        .Record(ukm::UkmRecorder::Get());
  }

  // Don't let the extension remove a tab if the user is dragging tabs around.
  if (!ExtensionTabUtil::IsTabStripEditable(*window->profile())) {
    *error = ExtensionTabUtil::kTabStripNotEditableError;
    return false;
  }

#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Get last committed or pending URL.
  std::string current_url = contents->GetVisibleURL().is_valid()
                                ? contents->GetVisibleURL().spec()
                                : std::string();
  NotifyExtensionTelemetry(Profile::FromBrowserContext(browser_context()),
                           extension(), safe_browsing::TabsApiInfo::REMOVE,
                           current_url,
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
  CHECK_GT(remaining_tabs_count_, 0, base::NotFatalUntil::M161);
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

  if (!ExtensionTabUtil::IsTabStripEditable(*target_window->profile())) {
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
    if (!tabs_util::GetTabById(tab_id, browser_context(),
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
      if (tabs_util::MoveTabToWindow(
              this, tab_ids[i], target_window->GetBrowserWindowInterface(), -1,
              /*allow_other_window_types=*/false, &error) < 0) {
        return RespondNow(Error(std::move(error)));
      }
    }
  }

  // Get the resulting tab handles in the target browser. We recalculate these
  // after all tabs are moved so that any callbacks are resolved. The set will
  // dedupe any duplicate tabs.
  std::set<::tabs::TabHandle> tab_handles;
  for (int tab_id : tab_ids) {
    ::tabs::TabHandle tab_handle;
    if (!GetTabHandleById(tab_id, *browser_context(),
                          include_incognito_information(), &tab_handle,
                          &error)) {
      return RespondNow(Error(std::move(error)));
    }

    if (tab_handles.count(tab_handle)) {
      continue;
    }

    ::tabs::TabInterface* tab = tab_handle.Get();
    CHECK(tab);

    const std::optional<split_tabs::SplitTabId> split_id = tab->GetSplit();
    if (split_id.has_value()) {
      const std::vector<::tabs::TabHandle> split_tabs =
          GetTabsInSplit(*split_id, *tab_list);
      tab_handles.insert(split_tabs.begin(), split_tabs.end());
    } else {
      tab_handles.insert(tab_handle);
    }
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
  CHECK_GT(group_id, 0, base::NotFatalUntil::M161);

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
  if (!tabs_util::GetTabById(tab_id, browser_context(),
                             include_incognito_information(), &window, nullptr,
                             &tab_index, error) ||
      !window) {
    return false;
  }

  if (!ExtensionTabUtil::IsTabStripEditable(*window->profile())) {
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

  // Extend selection for any split tabs.
  std::optional<split_tabs::SplitTabId> split_id = tab->GetSplit();
  if (split_id.has_value()) {
    std::vector<::tabs::TabHandle> split_tabs =
        GetTabsInSplit(*split_id, *tab_list);
    tabs.insert(split_tabs.begin(), split_tabs.end());
  }

  tab_list->Ungroup(tabs);
  return true;
}

TabsCreateSplitFunction::~TabsCreateSplitFunction() = default;

ExtensionFunction::ResponseAction TabsCreateSplitFunction::Run() {
  std::optional<tabs::CreateSplit::Params> params =
      tabs::CreateSplit::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(params->tab_ids.size() == 2u);

  const std::vector<int>& tab_ids = params->tab_ids;
  if (tab_ids[0] == tab_ids[1]) {
    return RespondNow(Error(tabs_constants::kSplitWithDuplicateTabsError));
  }

  std::string error;
  WindowController* window = nullptr;
  bool pinned = false;
  std::optional<tab_groups::TabGroupId> group_id;
  int previous_tab_index = -1;
  std::vector<::tabs::TabHandle> tab_handles;
  tab_handles.reserve(tab_ids.size());

  for (size_t i = 0; i < tab_ids.size(); ++i) {
    WindowController* tab_window = nullptr;
    content::WebContents* web_contents = nullptr;
    int tab_index = -1;
    if (!tabs_util::GetTabById(tab_ids[i], browser_context(),
                               include_incognito_information(), &tab_window,
                               &web_contents, &tab_index, &error)) {
      return RespondNow(Error(std::move(error)));
    }
    // 1. Check that the tab is not a DevTools tab.
    if (DevToolsWindow::IsDevToolsWindow(web_contents)) {
      return RespondNow(Error(tabs_constants::kNotAllowedForDevToolsError));
    }
    // 2. Check that the tab is backed by a TabInterface (standalone windows
    // such as Document PiP do not have one).
    ::tabs::TabInterface* tab =
        ::tabs::TabInterface::MaybeGetFromContents(web_contents);
    if (!tab) {
      return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
    }
    // 3. Check that the tab is not already in a split view.
    if (tab->IsSplit()) {
      return RespondNow(Error(ErrorUtils::FormatErrorMessage(
          tabs_constants::kSplitWithTabAlreadyInSplitViewError,
          base::NumberToString(tab_ids[i]))));
    }

    // 4. Check that tab is in the same window, has matching pinned and group ID
    // states, and is adjacent.
    if (i == 0) {
      // Use the first tab to set the baseline state for validation.
      window = tab_window;
      pinned = tab->IsPinned();
      group_id = tab->GetGroup();
      CHECK(window);
      if (!ExtensionTabUtil::IsTabStripEditable(*window->profile())) {
        return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
      }
    } else {
      if (tab_window != window) {
        return RespondNow(Error(ErrorUtils::FormatErrorMessage(
            tabs_constants::kSplitWithTabsMatchingStateError,
            tabs_constants::kWindowIdKey)));
      }
      if (tab->IsPinned() != pinned) {
        return RespondNow(Error(ErrorUtils::FormatErrorMessage(
            tabs_constants::kSplitWithTabsMatchingStateError,
            tabs_constants::kPinnedKey)));
      }
      if (tab->GetGroup() != group_id) {
        return RespondNow(Error(ErrorUtils::FormatErrorMessage(
            tabs_constants::kSplitWithTabsMatchingStateError,
            tabs_constants::kGroupIdKey)));
      }
      if (std::abs(tab_index - previous_tab_index) != 1) {
        return RespondNow(
            Error(tabs_constants::kSplitWithTabIndexNotAdjacentError));
      }
    }
    previous_tab_index = tab_index;
    tab_handles.push_back(tab->GetHandle());
  }

  BrowserWindowInterface* browser = window->GetBrowserWindowInterface();
  CHECK(browser);
  TabListInterface* tab_list = TabListInterface::From(browser);
  std::optional<split_tabs::SplitTabId> split_id =
      tab_list ? tab_list->CreateSplit(
                     tab_handles, split_tabs::SplitTabVisualData(
                                      split_tabs::SplitTabLayout::kSideBySide))
               : std::nullopt;
  if (!split_id) {
    return RespondNow(Error(tabs_constants::kSplitViewCreationFailedError));
  }
  return RespondNow(WithArguments(ExtensionTabUtil::GetSplitId(*split_id)));
}

TabsUnsplitFunction::~TabsUnsplitFunction() = default;

ExtensionFunction::ResponseAction TabsUnsplitFunction::Run() {
  std::optional<tabs::Unsplit::Params> params =
      tabs::Unsplit::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int split_view_id = params->split_view_id;
  std::string error;
  WindowController* window = nullptr;
  split_tabs::SplitTabId split_id = split_tabs::SplitTabId::CreateEmpty();
  if (!ExtensionTabUtil::GetSplitById(split_view_id, browser_context(),
                                      include_incognito_information(), &window,
                                      &split_id, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  if (!ExtensionTabUtil::IsTabStripEditable(*window->profile())) {
    return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
  }
  // `ExtensionTabUtil::GetSplitById()` skips `WindowController`s without a
  // `BrowserWindowInterface` or `TabListInterface` (such as standalone
  // Document Picture-in-Picture windows), so both are guaranteed non-null here.
  BrowserWindowInterface* browser = window->GetBrowserWindowInterface();
  CHECK(browser);
  TabListInterface* tab_list = TabListInterface::From(browser);
  CHECK(tab_list);
  tab_list->Unsplit(split_id);

  return RespondNow(NoArguments());
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
    if (!tabs_util::GetTabById(*params->tab_id, browser_context(),
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
    if (!ExtensionTabUtil::IsTabStripEditable(*window_controller->profile())) {
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

  if (!TranslateService::IsTranslatableURL(contents->GetLastCommittedURL())) {
    return RespondNow(Error(kLanguageDetectionNotSupported));
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

base::expected<void, extensions::ScreenshotAccessError>
TabsCaptureVisibleTabFunction::GetScreenshotAccess(
    content::WebContents* web_contents) const {
  return ExtensionsBrowserClient::Get()->IsScreenshotRestricted(web_contents);
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
  NotifyExtensionTelemetry(
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
  if (!tabs_util::GetTabById(execute_tab_id_, browser_context(),
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

  bool success = tabs_util::GetTabById(execute_tab_id_, browser_context(),
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
      GetTabsAPIDefaultWebContents(this, tab_id, &error);
  if (!web_contents) {
    return RespondNow(Error(std::move(error)));
  }

  const GURL& url = web_contents->GetLastCommittedURL();
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
      GetTabsAPIDefaultWebContents(this, tab_id, &error);
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
      GetTabsAPIDefaultWebContents(this, tab_id, &error);
  if (!web_contents) {
    return RespondNow(Error(std::move(error)));
  }

  const GURL& url = web_contents->GetLastCommittedURL();
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
      GetTabsAPIDefaultWebContents(this, tab_id, &error);
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
  // Otherwise, discard the least important tab.
  if (params->tab_id) {
    int tab_id = *params->tab_id;
    std::string error;

    int tab_index = -1;
    if (!tabs_util::GetTabById(tab_id, browser_context(),
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
    // Make sure we only discard tabs from profiles the extension is allowed to
    // access.
    Profile* profile = Profile::FromBrowserContext(browser_context());
    absl::flat_hash_set<base::UnguessableToken> allowed_tokens;
    allowed_tokens.insert(profile->UniqueToken());

    if (include_incognito_information()) {
      Profile* maybe_incognito_profile =
          profile->GetPrimaryOTRProfile(/*create_if_needed=*/false);
      if (maybe_incognito_profile) {
        allowed_tokens.insert(maybe_incognito_profile->UniqueToken());
      }
    }

    contents = resource_coordinator::DiscardLeastImportantTab(
        ::mojom::LifecycleUnitDiscardReason::EXTERNAL,
        /*ignore_recent_visibility=*/false,
        /*allowed_browser_context_ids=*/std::move(allowed_tokens));
  }

  if (!contents) {
    // Return appropriate error message otherwise.
    return RespondNow(Error(params->tab_id
                                ? ErrorUtils::FormatErrorMessage(
                                      tabs_constants::kCannotDiscardTab,
                                      base::NumberToString(*params->tab_id))
                                : kCannotFindTabToDiscard));
  }

  return RespondNow(
      ArgumentList(tabs::Discard::Results::Create(CreateTabObjectHelper(
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
      GetTabsAPIDefaultWebContents(this, tab_id, &error);
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
      GetTabsAPIDefaultWebContents(this, tab_id, &error);
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
