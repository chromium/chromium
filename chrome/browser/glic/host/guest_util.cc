// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/guest_util.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/supports_user_data.h"
#include "base/task/single_thread_task_runner.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/common/glic_navigation.h"
#include "chrome/browser/glic/glic_hotkey.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_client_data_storage.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/host/glic_guest_observer.h"
#include "chrome/browser/glic/host/glic_ui.h"
#include "chrome/browser/glic/host/glic_web_client_manager.h"
#include "chrome/browser/glic/host/glic_web_contents_manager.h"
#include "chrome/browser/glic/host/guest_util_internal.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/google/core/common/google_util.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/buildflags/buildflags.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/origin_matcher/origin_matcher.h"
#include "components/prefs/pref_service.h"
#include "components/skills/features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/content_features.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/base/url_util.h"
#include "pdf/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_metadata.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/printing_init.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#else
#include "components/guest_view/browser/slim_web_view/slim_web_view_guest.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "components/page_content_annotations/content/page_context_fetcher.h"
#endif

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
#include "chrome/browser/enterprise/data_controls/data_controls_dialog_factory.h"
#include "components/enterprise/data_controls/content/browser/last_replaced_clipboard_data.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/enterprise/data_controls/android_data_controls_dialog_factory.h"
#else
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog_factory.h"
#endif  // BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

namespace glic {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GlicPasteFormat {
  kPlainText = 0,
  kHtml = 1,
  kBitmap = 2,
  kFilenames = 3,
  kOther = 4,
  kMaxValue = kOther,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GlicPasteFailedEligibilityReason {
  kPageContextIneligible = 0,
  kPageContextInvalidated = 1,
  kCrossProfile = 2,
  kMaxValue = kCrossProfile,
};

namespace {

// Attached to the WebUI WebContents using WebContentsUserData.
// Acts as both a marker for Glic WebUI and a link to its guest WebContents.
class GlicWebUiData : public content::WebContentsUserData<GlicWebUiData>,
                      public content::WebContentsObserver {
 public:
  ~GlicWebUiData() override = default;

  // Call this when the guest is attached to establish the link.
  void SetGuestContents(content::WebContents* guest_contents) {
    Observe(guest_contents);
  }

  void SetContentsManager(GlicWebContentsManager* contents_manager) {
    contents_manager_ = contents_manager;
  }
  GlicWebContentsManager* contents_manager() const { return contents_manager_; }

  // Returns the guest WebContents if it is attached and valid, nullptr
  // otherwise.
  content::WebContents* guest_contents() const {
    content::WebContents* guest = web_contents();
    if (!guest) {
      return nullptr;
    }
    auto* guest_view = guest_view::GuestViewBase::FromWebContents(guest);
    if (guest_view && !guest_view->attached()) {
      return nullptr;
    }
    return guest;
  }

 private:
  explicit GlicWebUiData(content::WebContents* webui_contents)
      : content::WebContentsUserData<GlicWebUiData>(*webui_contents),
        content::WebContentsObserver(nullptr),
        webui_contents_(webui_contents) {}
  friend class content::WebContentsUserData<GlicWebUiData>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  using WebContentsObserver::web_contents;

  raw_ptr<GlicWebContentsManager> contents_manager_ = nullptr;
  raw_ptr<content::WebContents> webui_contents_;
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(GlicWebUiData);

// Attached to the overlay WebContents using WebContentsUserData.
class GlicOverlayData : public content::WebContentsUserData<GlicOverlayData> {
 public:
  ~GlicOverlayData() override = default;

 private:
  explicit GlicOverlayData(content::WebContents* overlay_contents)
      : content::WebContentsUserData<GlicOverlayData>(*overlay_contents) {}
  friend class content::WebContentsUserData<GlicOverlayData>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(GlicOverlayData);

// Attached to RenderProcessHost to identify Glic processes.
class GlicProcessUserData : public base::SupportsUserData::Data {
 public:
  static constexpr char kKey[] = "glic::GlicProcessUserData";

  ~GlicProcessUserData() override = default;

  static GlicProcessUserData* FromProcessHost(content::RenderProcessHost* rph) {
    return rph ? static_cast<GlicProcessUserData*>(rph->GetUserData(kKey))
               : nullptr;
  }

  static void MarkProcess(content::RenderProcessHost* rph) {
    if (rph && !FromProcessHost(rph)) {
      rph->SetUserData(kKey, base::WrapUnique(new GlicProcessUserData()));
    }
  }

 private:
  GlicProcessUserData() = default;
};

// Caches an OriginMatcher parsed from a space-separated origin list string.
// In production, feature parameters are fixed at startup, but unit tests
// frequently reconfigure parameters across test cases (e.g., via
// base::test::ScopedFeatureList). Updating the matcher whenever the parameter
// string changes prevents stale matcher state and test contamination.
class CachedOriginMatcher {
 public:
  bool Matches(const std::string& allowed_origins_param,
               const url::Origin& origin) {
    if (cached_param_ != allowed_origins_param) {
      cached_param_ = allowed_origins_param;
      matcher_ = origin_matcher::OriginMatcher();
      for (const std::string& allowed :
           base::SplitString(allowed_origins_param, " ", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY)) {
        matcher_.AddRuleFromString(allowed);
      }
    }
    return matcher_.Matches(origin);
  }

 private:
  std::string cached_param_;
  origin_matcher::OriginMatcher matcher_;
};

void ConfigureGuestZoom(content::WebContents& guest_contents) {
#if BUILDFLAG(IS_ANDROID)
  // Apply the persisted zoom level to the guest WebContents.
  if (Profile* profile =
          Profile::FromBrowserContext(guest_contents.GetBrowserContext())) {
    double zoom_factor = GetZoomFactor(profile->GetPrefs());
    double zoom_level = blink::ZoomFactorToZoomLevel(zoom_factor);
    content::HostZoomMap::SetZoomLevel(&guest_contents, zoom_level);
  }
#endif
}

}  // namespace

void PrepareGlicGuestWebContents(content::WebContents& guest_contents,
                                 GlicWebContentsManager& contents_manager) {
  GlicGuestObserver::CreateForWebContents(guest_contents, contents_manager);

  if (guest_contents.GetPrimaryMainFrame()) {
    GlicProcessUserData::MarkProcess(
        guest_contents.GetPrimaryMainFrame()->GetProcess());
  }

  PrefsTabHelper::CreateForWebContents(&guest_contents);
  ConfigureGuestZoom(guest_contents);

#if BUILDFLAG(ENABLE_PRINTING)
  // In <webview> mode, WebViewGuest attaches printing helpers via
  // ChromeExtensionsAPIClient::AttachWebContentsHelpers. In NoWebview mode,
  // initialize printing explicitly on the guest WebContents.
  if (GlicKeyedService::IsNoWebviewEnabled(
          guest_contents.GetBrowserContext())) {
    printing::InitializePrintingForWebContents(&guest_contents);
  }
#endif

#if !BUILDFLAG(IS_ANDROID)
  guest_contents.SetSupportsDraggableRegions(true);
#endif  // !BUILDFLAG(IS_ANDROID)
}

void SetHostForGuest(content::WebContents& guest_contents, Host* host) {
  if (auto* observer = GlicGuestObserver::FromWebContents(&guest_contents)) {
    observer->set_host(host);
  }
}

bool IsGlicGuest(const content::WebContents* web_contents) {
  return web_contents &&
         GlicGuestObserver::FromWebContents(web_contents) != nullptr;
}

bool MaybeHandleGlicWindowCreation(
    content::RenderFrameHost* opener,
    const GURL& target_url,
    ui::mojom::WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features) {
  if (!opener) {
    return false;
  }
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(opener);
  if (!web_contents || !IsGlicGuest(web_contents)) {
    return false;
  }
  if (!target_url.is_valid() || !target_url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  bool is_popup =
      (disposition == ui::mojom::WindowOpenDisposition::kNewPopup) ||
      window_features.is_popup;
  if (base::FeatureList::IsEnabled(features::kGlicPopupWindowsEnabled) &&
      (is_popup || window_features.has_x || window_features.has_y ||
       window_features.has_width || window_features.has_height)) {
    int32_t popup_width =
        window_features.has_width && window_features.bounds.width() > 0
            ? window_features.bounds.width()
            : 500;
    int32_t popup_height =
        window_features.has_height && window_features.bounds.height() > 0
            ? window_features.bounds.height()
            : 500;

    int x = 0;
    int y = 0;
    gfx::NativeView native_view = web_contents->GetContentNativeView();
    if (display::Screen::Get() && native_view) {
      const display::Display& display =
          display::Screen::Get()->GetDisplayNearestView(native_view);
      const gfx::Rect work_area = display.work_area();
      x = work_area.x() + (work_area.width() - popup_width) / 2;
      y = work_area.y() + (work_area.height() - popup_height) / 2;
    }

    std::unique_ptr<NavigateParams> params = std::make_unique<NavigateParams>(
        profile, target_url, ui::PAGE_TRANSITION_LINK);
    params->disposition = ui::mojom::WindowOpenDisposition::kNewPopup;
    params->opened_by_another_window = true;
    params->window_features.bounds = gfx::Rect(x, y, popup_width, popup_height);
    glic::NavigateAsync(std::move(params), base::DoNothing());
    return true;
  }

  // TODO(b/568778823): Make non-popups use createTab.
  std::unique_ptr<NavigateParams> params = std::make_unique<NavigateParams>(
      profile, target_url, ui::PAGE_TRANSITION_LINK);
  params->disposition =
      (disposition == ui::mojom::WindowOpenDisposition::kNewBackgroundTab)
          ? ui::mojom::WindowOpenDisposition::kNewBackgroundTab
          : ui::mojom::WindowOpenDisposition::kNewForegroundTab;
  glic::NavigateAsync(std::move(params), base::DoNothing());
  return true;
}

void MarkProcessAsGlic(content::RenderProcessHost* rph) {
  GlicProcessUserData::MarkProcess(rph);
}

void CreateGlicWebUiData(content::WebContents* webui_contents) {
  GlicWebUiData::CreateForWebContents(webui_contents);
}

void CreateGlicOverlayData(content::WebContents* overlay_contents) {
  GlicOverlayData::CreateForWebContents(overlay_contents);
}

void SetContentsManagerForWebContents(
    content::WebContents* web_contents,
    GlicWebContentsManager* contents_manager) {
  if (!web_contents) {
    return;
  }
  if (auto* data = GlicWebUiData::FromWebContents(web_contents)) {
    data->SetContentsManager(contents_manager);
  }
}

GlicWebContentsManager* GetContentsManagerForWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  if (auto* observer = GlicGuestObserver::FromWebContents(web_contents)) {
    return &observer->contents_manager();
  }
  // Only needed for the legacy nested <webview> mode where the outer WebUI
  // WebContents holds the GlicWebUiData.
  if (auto* data = GlicWebUiData::FromWebContents(web_contents)) {
    if (data->contents_manager()) {
      return data->contents_manager();
    }
  }
  return nullptr;
}

GlicWebClientManager* GetWebClientManagerForWebContents(
    content::WebContents* web_contents) {
  if (auto* contents_manager = GetContentsManagerForWebContents(web_contents)) {
    return &contents_manager->web_client_manager();
  }
  return nullptr;
}

GURL GetGuestURL(content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return GetGlicProviderSpec(profile).GetGuestUrl(profile);
}

base::TimeDelta GetMaxLoadingTime() {
  int max_loading_time_ms = features::kGlicMaxLoadingTimeMs.Get();
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(::switches::kGlicDev)) {
    // Bump up timeout value, as dev server may be slow.
    max_loading_time_ms *= 100;
  }
  return base::Milliseconds(std::max(0, max_loading_time_ms));
}

url::Origin GetGuestOrigin(content::BrowserContext* browser_context) {
  return url::Origin::Create(GetGuestURL(browser_context));
}

std::string GetGlicAllowedOrigins() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  std::string allowed_origins =
      command_line->GetSwitchValueASCII(::switches::kGlicAllowedOrigins);
  if (allowed_origins.empty()) {
    allowed_origins = features::kGlicAllowedOriginsOverride.Get();
  }

  // Allow corp origins, needed for @google accounts.
  allowed_origins += " https://*.corp.google.com";
  return allowed_origins;
}

bool IsOriginAllowedGlicApi(const url::Origin& origin,
                            content::BrowserContext* browser_context) {
  if (origin.opaque()) {
    return false;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(::switches::kGlicDev)) {
    return true;
  }
  if (GetGuestOrigin(browser_context).IsSameOriginWith(origin)) {
    return true;
  }
  std::string api_allowed_origins = features::kGlicApiAllowedOrigins.Get();
  if (api_allowed_origins.empty()) {
    return false;
  }

  static base::NoDestructor<CachedOriginMatcher> cached_matcher;
  return cached_matcher->Matches(api_allowed_origins, origin);
}

bool IsGuestOriginAllowed(const url::Origin& origin,
                          content::BrowserContext* browser_context) {
  if (origin.opaque()) {
    return false;
  }
  if (IsOriginAllowedGlicApi(origin, browser_context)) {
    return true;
  }

  // Allow login/auth origins.
  if (origin.scheme() == url::kHttpsScheme &&
      (origin.DomainIs("login.corp.google.com") ||
       origin.DomainIs("accounts.google.com") ||
       origin.DomainIs("accounts.googlers.com") ||
       origin.DomainIs("gaiastaging.corp.google.com"))) {
    return true;
  }

  std::string allowed_origins = GetGlicAllowedOrigins();
  if (allowed_origins.empty()) {
    return false;
  }

  static base::NoDestructor<CachedOriginMatcher> cached_matcher;
  return cached_matcher->Matches(allowed_origins, origin);
}

bool IsAdminBlockedUrl(const GURL& url) {
  if (!base::FeatureList::IsEnabled(features::kGlicCaaGuestError)) {
    return false;
  }
  auto* command_line = base::CommandLine::ForCurrentProcess();
  std::string admin_blocked_redirect_patterns =
      command_line->GetSwitchValueASCII(::switches::kGlicAdminRedirectPatterns);
  if (admin_blocked_redirect_patterns.empty()) {
    admin_blocked_redirect_patterns =
        features::kGlicCaaGuestRedirectPatterns.Get();
  }
  if (admin_blocked_redirect_patterns.empty()) {
    return false;
  }
  std::string url_spec = url.spec();
  for (const std::string& pattern :
       base::SplitString(admin_blocked_redirect_patterns, " ",
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (base::MatchPattern(url_spec, pattern)) {
      return true;
    }
    GURL pattern_gurl(pattern);
    if (pattern_gurl.is_valid()) {
      if (pattern_gurl.scheme() == url.scheme() &&
          (pattern_gurl.host() == url.host() ||
           base::MatchPattern(url.host(), pattern_gurl.host())) &&
          (!pattern_gurl.has_port() || pattern_gurl.port() == url.port()) &&
          (pattern_gurl.path().empty() || pattern_gurl.path() == "/" ||
           base::MatchPattern(url.path(),
                              base::StrCat({pattern_gurl.path(), "*"})) ||
           base::MatchPattern(url.path(), pattern_gurl.path()))) {
        return true;
      }
    }
  }
  return false;
}

bool IsFrameAllowedGlicApi(content::RenderFrameHost& frame_host) {
  if (!frame_host.IsInPrimaryMainFrame()) {
    return false;
  }
  content::WebContents* guest_contents =
      content::WebContents::FromRenderFrameHost(&frame_host);
  if (!guest_contents || !IsGlicGuest(guest_contents)) {
    return false;
  }
  return IsOriginAllowedGlicApi(frame_host.GetLastCommittedOrigin(),
                                frame_host.GetBrowserContext());
}

void BindGlicWebClientHandler(
    content::RenderFrameHost* rfh,
    mojo::PendingReceiver<glic::mojom::WebClientHandler> receiver) {
  if (!IsFrameAllowedGlicApi(*rfh)) {
    return;
  }
  content::WebContents* guest_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!guest_contents) {
    return;
  }
  auto* observer = GlicGuestObserver::FromWebContents(guest_contents);
  if (!observer) {
    return;
  }
  if (observer->host()) {
    observer->host()->CreateWebClient(std::move(receiver));
    return;
  }
  if (auto* manager = GetWebClientManagerForWebContents(guest_contents)) {
    manager->SetPendingWebClientReceiver(std::move(receiver));
    return;
  }
}
content::StoragePartitionConfig GetGlicStoragePartitionConfig(
    content::BrowserContext* browser_context) {
  return content::StoragePartitionConfig::Create(browser_context,
                                                 chrome::kChromeUIGlicHost,
                                                 /*partition_name=*/"glicpart",
                                                 /*in_memory=*/false);
}

GURL MaybeApplyPresetGuestUrl(GURL guest_url) {
  if (!base::FeatureList::IsEnabled(features::kGlicGuestUrlPresets)) {
    return guest_url;
  }

  GURL preset_url;
  switch (features::kGlicGuestUrlPresetType.Get()) {
    case 0:
      preset_url = GURL(g_browser_process->local_state()->GetString(
          prefs::kGlicGuestUrlPresetAutopush));
      break;
    case 1:
      preset_url = GURL(g_browser_process->local_state()->GetString(
          prefs::kGlicGuestUrlPresetStaging));
      break;
    case 2:
      preset_url = GURL(g_browser_process->local_state()->GetString(
          prefs::kGlicGuestUrlPresetPreprod));
      break;
    case 3:
      preset_url = GURL(g_browser_process->local_state()->GetString(
          prefs::kGlicGuestUrlPresetProd));
      break;
    default:
      return guest_url;
  }

  if (preset_url.is_valid()) {
    return preset_url;
  } else {
    LOG(ERROR) << "Invalid preset glic guest url, ignoring.";
    return guest_url;
  }
}

// The GEiC FRE pref key strips `hl` (see geic_fre.cc).
GURL GetLocalizedGuestURL(const GURL& guest_url) {
  std::string unused_output;
  if (net::GetValueForKeyInQuery(guest_url, "hl", &unused_output)) {
    return guest_url;
  }
  std::string application_locale = g_browser_process->GetApplicationLocale();
  std::string google_locale = google_util::GetGoogleLocale(application_locale);
  return net::AppendQueryParameter(guest_url, "hl", google_locale);
}

bool IsGlicWebUI(const content::WebContents* web_contents) {
  return web_contents &&
         GlicWebUiData::FromWebContents(web_contents) != nullptr;
}

bool IsGlicOverlay(const content::WebContents* web_contents) {
  return web_contents &&
         GlicOverlayData::FromWebContents(web_contents) != nullptr;
}

bool IsAnyGlicWebContents(const content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }
  return IsGlicGuest(web_contents) || IsGlicWebUI(web_contents) ||
         IsGlicOverlay(web_contents);
}

bool IsProcessHostForGlic(content::RenderProcessHost* process_host) {
  return process_host &&
         GlicProcessUserData::FromProcessHost(process_host) != nullptr;
}

content::WebContents* GetGlicGuestWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  if (auto* data = GlicWebUiData::FromWebContents(web_contents)) {
    return data->guest_contents();
  }
  if (IsGlicGuest(web_contents)) {
    return web_contents;
  }
  return nullptr;
}

GlicUI* GetGlicUiForGuest(content::WebContents* guest_contents) {
  if (!IsGlicGuest(guest_contents)) {
    return nullptr;
  }
  content::WebContents* top =
      guest_view::GuestViewBase::GetTopLevelWebContents(guest_contents);
  if (!top) {
    return nullptr;
  }
  return GlicUI::From(top);
}

Host* GetGlicHostForGuest(content::WebContents* guest_contents) {
  auto* glic_ui = GetGlicUiForGuest(guest_contents);
  return glic_ui ? glic_ui->host() : nullptr;
}

// Called by Chrome's GuestView subsystem when a `<webview>` guest is created.
// Not used in NoWebview mode, where PrivilegedWebContents is used instead.
bool OnGuestAdded(content::WebContents* guest_contents) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (!extensions::WebViewGuest::FromWebContents(guest_contents)) {
    return false;
  }
#else
  if (!guest_view::SlimWebViewGuest::FromWebContents(guest_contents)) {
    return false;
  }
#endif
  if (guest_contents->HasLiveOriginalOpenerChain()) {
    return false;
  }

  content::WebContents* top =
      guest_view::GuestViewBase::GetTopLevelWebContents(guest_contents);
  CHECK(top);
  if (!IsGlicWebUI(top)) {
    return false;
  }
  GlicKeyedService* service = GlicKeyedServiceFactory::GetGlicKeyedService(
      top->GetBrowserContext(), /*create=*/false);
  if (!service) {
    return false;
  }

  // The outer WebUI WebContents (top) is always created and managed by a
  // GlicWebUIContentsManager in <webview> mode.
  GlicWebContentsManager* contents_manager =
      GetContentsManagerForWebContents(top);
  CHECK(contents_manager);
  if (auto* data = GlicWebUiData::FromWebContents(top)) {
    data->SetGuestContents(guest_contents);
    PrepareGlicGuestWebContents(*guest_contents, *contents_manager);
  }

  if (contents_manager) {
    contents_manager->web_client_manager().AttachGuestContents(guest_contents);
  }
  return true;
}

bool IsMediaRequestFromGlic(content::BrowserContext* browser_context,
                            const std::string& request_id) {
  content::WebContents* web_contents =
      content::MediaSession::GetWebContentsFromRequestId(request_id);
  return web_contents && web_contents->GetBrowserContext() == browser_context &&
         IsGlicGuest(web_contents);
}

mojom::FormFactor GetGlicFormFactor(ui::DeviceFormFactor form_factor) {
#if BUILDFLAG(IS_ANDROID)
  // TODO(b/512144892): Foldable is currently grouped with phone. We need
  // transition between bottom sheet and side panel, to match tablet UI.
  if (base::android::device_info::is_foldable()) {
    return mojom::FormFactor::kPhone;
  }
#endif
  switch (form_factor) {
    case ui::DEVICE_FORM_FACTOR_DESKTOP:
      return mojom::FormFactor::kDesktop;
    // TODO(b/512144892): Foldable is currently grouped with phone. We need
    // transition between bottom sheet and side panel, to match tablet UI.
    case ui::DEVICE_FORM_FACTOR_FOLDABLE:
    case ui::DEVICE_FORM_FACTOR_PHONE:
      return mojom::FormFactor::kPhone;
    case ui::DEVICE_FORM_FACTOR_TABLET:
      return mojom::FormFactor::kTablet;
    default:
      return mojom::FormFactor::kUnknown;
  }
}

mojom::Platform GetGlicPlatform() {
#if BUILDFLAG(IS_MAC)
  return mojom::Platform::kMacOS;
#elif BUILDFLAG(IS_WIN)
  return mojom::Platform::kWindows;
#elif BUILDFLAG(IS_LINUX)
  return mojom::Platform::kLinux;
#elif BUILDFLAG(IS_CHROMEOS)
  return mojom::Platform::kChromeOS;
#elif BUILDFLAG(IS_ANDROID)
  return mojom::Platform::kAndroid;
#else
  return mojom::Platform::kUnknown;
#endif
}

std::optional<std::string> GetOnboardingArm(Profile* profile) {
  if (!profile || GlicEnabling::HasConsentedForProfile(profile)) {
    return std::nullopt;
  }
  return "2";
}

void PopulateGlobalClientInitialState(mojom::WebClientInitialState* state,
                                      Profile* profile) {
  state->chrome_version = version_info::GetVersion();
  state->platform = GetGlicPlatform();
  state->form_factor = GetGlicFormFactor(ui::GetDeviceFormFactor());

  PrefService* pref_service = profile->GetPrefs();
  state->microphone_permission_enabled =
      pref_service->GetBoolean(prefs::kGlicMicrophoneEnabled);
  state->location_permission_enabled =
      pref_service->GetBoolean(prefs::kGlicGeolocationEnabled);
  state->tab_context_permission_enabled =
      pref_service->GetBoolean(prefs::kGlicTabContextEnabled);
  state->os_location_permission_enabled =
      system_permission_settings::IsAllowed(ContentSettingsType::GEOLOCATION);

  state->zoom_factor = GetZoomFactor(pref_service);

#if !BUILDFLAG(IS_ANDROID)
  state->hotkey = GetHotkeyString();
#endif

  state->enable_zero_state_suggestions = IsZeroStateSuggestionsEnabled();
  state->enable_cached_get_user_profile_info = base::FeatureList::IsEnabled(
      features::kGlicEnableCachedGetUserProfileInfo);
  state->enable_act_in_focused_tab =
      base::FeatureList::IsEnabled(features::kGlicActor);
  state->enable_scroll_to =
      base::FeatureList::IsEnabled(features::kGlicScrollTo);
  state->enable_default_tab_context_setting_feature =
      base::FeatureList::IsEnabled(features::kGlicDefaultTabContextSetting);
  state->default_tab_context_setting_enabled =
      pref_service->GetBoolean(prefs::kGlicDefaultTabContextEnabled);
  state->closed_captioning_setting_enabled =
      pref_service->GetBoolean(prefs::kGlicClosedCaptioningEnabled);
  state->enable_maybe_refresh_user_status =
      base::FeatureList::IsEnabled(features::kGlicUserStatusCheck) &&
      features::kGlicUserStatusRefreshApi.Get();
  state->enable_get_context_actor =
      base::FeatureList::IsEnabled(glic::mojom::features::kGlicActorTabContext);
  state->enable_web_actuation_setting_feature =
      base::FeatureList::IsEnabled(features::kGlicWebActuationSetting);

  auto* glic_service = GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  state->actuation_on_web_setting_enabled =
      glic_service ? glic_service->enabling().GetUserEnabledActuationOnWeb()
                   : false;

#if BUILDFLAG(ENABLE_PDF)
  if (features::kGlicScrollToPDF.Get()) {
    state->host_capabilities.push_back(mojom::HostCapability::kScrollToPdf);
  }

  if (base::FeatureList::IsEnabled(
          page_content_annotations::kGlicEmbeddedPdfBytesExtraction)) {
    state->host_capabilities.push_back(
        mojom::HostCapability::kEmbeddedPdfBytesExtraction);
  }
#endif
  state->host_capabilities.push_back(mojom::HostCapability::kMultiInstance);

  if (base::FeatureList::IsEnabled(features::kGlicNoWebUiLoader)) {
    state->host_capabilities.push_back(mojom::HostCapability::kNoWebUiLoader);
  }

  if (GlicKeyedService::IsNoWebviewEnabled(profile)) {
    state->host_capabilities.push_back(mojom::HostCapability::kNoWebview);
  }

  if (base::FeatureList::IsEnabled(features::kGlicPasteEligibilityCheck)) {
    state->host_capabilities.push_back(
        mojom::HostCapability::kEnforcesPasteEligibility);
  }

  if (base::FeatureList::IsEnabled(features::kGlicWebDragAndDropFileUpload)) {
    state->host_capabilities.push_back(mojom::HostCapability::kImgWebDragDrop);
  }

  if (GlicEnabling::IsAutoOpenForPdfEnabled(profile)) {
    state->host_capabilities.push_back(mojom::HostCapability::kPdfZeroState);
  }

  if (base::FeatureList::IsEnabled(features::kGlicInvoke)) {
    state->host_capabilities.push_back(mojom::HostCapability::kInvoke);
  }

  if (GetOnboardingArm(profile) == "2") {
    state->host_capabilities.push_back(
        mojom::HostCapability::kTrustFirstOnboardingArm2);
  }
  if (GlicEnabling::IsShareImageEnabledForProfile(profile)) {
    state->host_capabilities.push_back(
        mojom::HostCapability::kShareAdditionalImageContext);
  }
  if (!GlicEnabling::IsLiveAndFloatyEnabledByFlags()) {
    state->host_capabilities.push_back(mojom::HostCapability::kNoLiveMode);
  }
  if (base::FeatureList::IsEnabled(features::kFedCmEmbedderInitiatedLogin)) {
    state->host_capabilities.push_back(
        mojom::HostCapability::kAutoLoginSignInWithGoogle);
  }
  if (base::FeatureList::IsEnabled(
          features::kGlicActorAutofillOneTimePassword)) {
    state->host_capabilities.push_back(
        mojom::HostCapability::kAttemptOtpFilling);
  }
  if (base::FeatureList::IsEnabled(features::kGlicDynamicChromeTools)) {
    state->host_capabilities.push_back(mojom::HostCapability::kChromeTools);
  }
  if (base::FeatureList::IsEnabled(features::kSkillsWebViewV2Enabled)) {
    state->host_capabilities.push_back(mojom::HostCapability::kSkillsV2);
  }
  if (base::FeatureList::IsEnabled(mojom::features::kGlicSendTabToSelf)) {
    state->host_capabilities.push_back(mojom::HostCapability::kSendTabToSelf);
  }
  state->enable_get_page_metadata =
      base::FeatureList::IsEnabled(blink::features::kFrameMetadataObserver);
  if (base::FeatureList::IsEnabled(
          glic::mojom::features::kGlicAppendModelQualityClientId)) {
    state->host_capabilities.push_back(
        mojom::HostCapability::kGetModelQualityClientId);
  }
  state->enable_capture_region =
      base::FeatureList::IsEnabled(features::kGlicCaptureRegion);
  state->can_act_on_web = false;
  if (base::FeatureList::IsEnabled(features::kGlicActor)) {
    state->can_act_on_web =
        glic_service ? glic_service->actor_policy_checker().CanActOnWeb()
                     : false;
  }
  state->enable_activate_tab =
      base::FeatureList::IsEnabled(glic::mojom::features::kGlicActivateTabApi);
  state->enable_get_tab_by_id =
      base::FeatureList::IsEnabled(features::kGlicGetTabByIdApi);
  state->enable_open_password_manager_settings_page =
      base::FeatureList::IsEnabled(
          features::kGlicOpenPasswordManagerSettingsPageApi);
  state->enable_open_contact_info_settings_page = base::FeatureList::IsEnabled(
      features::kGlicOpenContactInfoSettingsPageApi);
  state->enable_trust_first_onboarding =
      !GlicEnabling::HasConsentedForProfile(profile);
  state->onboarding_completed = GlicEnabling::HasConsentedForProfile(profile);
  state->enable_skills =
      skills::SkillsServiceFactory::IsSkillsEnabledForProfile(profile);
  state->enable_get_tab_favicon_by_id =
      base::FeatureList::IsEnabled(features::kGlicGetTabFaviconById);
  state->enable_process_counter_abuse_verdict =
      base::FeatureList::IsEnabled(features::kGlicProcessCounterAbuseVerdict);

  state->enable_gmail_otp_opt_in =
      base::FeatureList::IsEnabled(features::kGlicActorAutofillOneTimePassword);
  state->enable_gmail_otp_confirmation =
      base::FeatureList::IsEnabled(features::kGlicActorAutofillOneTimePassword);
  state->file_upload_policy_state =
      glic::prefs::GetFileUploadAllowedCapability(profile->GetPrefs());

  state->enable_client_storage =
      glic_service && glic_service->IsNoWebviewEnabled() &&
      base::FeatureList::IsEnabled(features::kGlicClientDataStorage);
  if (state->enable_client_storage && glic_service) {
    state->client_storage = glic_service->client_data_storage().GetAll();
  }
}

double GetZoomFactor(PrefService* pref_service) {
  // LINT.IfChange(GlicZoomFactors)
  int zoom_percent =
      std::clamp(pref_service->GetInteger(prefs::kGlicZoomLevel), 100, 200);
  return zoom_percent / 100.0;
  // LINT.ThenChange(//chrome/browser/resources/glic/webview.ts:GlicZoomFactors,
  // //chrome/browser/glic/host/glic_page_handler.cc:GlicZoomFactors)
}

void LogPasteAttempt(const content::ClipboardEndpoint& source,
                     const ui::ClipboardMetadata& metadata) {
  std::string_view source_suffix = source.web_contents() ? "Web" : "OS";

  GlicPasteFormat format = GlicPasteFormat::kOther;
  if (metadata.format_type == ui::ClipboardFormatType::PlainTextType()) {
    format = GlicPasteFormat::kPlainText;
  } else if (metadata.format_type == ui::ClipboardFormatType::HtmlType()) {
    format = GlicPasteFormat::kHtml;
  } else if (metadata.format_type == ui::ClipboardFormatType::BitmapType()) {
    format = GlicPasteFormat::kBitmap;
  } else if (metadata.format_type == ui::ClipboardFormatType::FilenamesType()) {
    format = GlicPasteFormat::kFilenames;
  }

  base::UmaHistogramEnumeration(
      base::StrCat({"Glic.Paste.AttemptedFormat.", source_suffix}), format);
}

// Tracks the copy eligibility of the last clipboard write. It listens to
// clipboard changes while a copy is pending to grab the newly generated
// sequence number after a copy completes, and compares against this sequence
// number at paste time.
class GlicClipboardEligibilityMonitor : public ui::ClipboardObserver {
 public:
  static constexpr base::TimeDelta kPendingCopyExpiration = base::Seconds(10);

  static GlicClipboardEligibilityMonitor* GetInstance() {
    static base::NoDestructor<GlicClipboardEligibilityMonitor> instance;
    return instance.get();
  }

  GlicClipboardEligibilityMonitor() = default;

  ~GlicClipboardEligibilityMonitor() override { StopObserving(); }

  void StageCopyEligibility(std::optional<bool> is_eligible,
                            bool clear_at_end_of_task) {
    if (!is_eligible.has_value()) {
      ClearPending();
      return;
    }
    ++pending_generation_;
    finalize_task_posted_ = false;
    pending_eligibility_ = *is_eligible;
    if (auto* clipboard = ui::Clipboard::GetForCurrentThread()) {
      pending_baseline_seqno_ =
          clipboard->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);
    } else {
      pending_baseline_seqno_.reset();
    }
    StartObserving();
    if (clear_at_end_of_task) {
      ScheduleFinalizePendingCopy();
    } else if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
      const uint64_t generation = pending_generation_;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&GlicClipboardEligibilityMonitor::FinalizePendingCopy,
                         base::Unretained(this), generation),
          kPendingCopyExpiration);
    }
  }

  void SetEligibilityForSeqno(ui::ClipboardSequenceNumberToken seqno,
                              bool is_eligible) {
    last_seqno_ = seqno;
    last_eligibility_ = is_eligible;
    ClearPending();
  }

  void OnClipboardDataChanged() override {
    if (!pending_eligibility_.has_value()) {
      return;
    }
    auto* clipboard = ui::Clipboard::GetForCurrentThread();
    if (!clipboard) {
      return;
    }
    last_seqno_ = clipboard->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);
    last_eligibility_ = *pending_eligibility_;
    ScheduleFinalizePendingCopy();
  }

  std::optional<bool> GetEligibility(
      const ui::ClipboardSequenceNumberToken& seqno) {
    // Handle platforms or clipboard backends where the sequence number advances
    // synchronously inside ScopedClipboardWriter destruction before
    // OnClipboardDataChanged() or FinalizePendingCopy() runs (e.g., macOS
    // pasteboard polling timer, Windows WM_CLIPBOARDUPDATE, or trailing
    // WebSmartPaste writes on TestClipboard).
    if (pending_eligibility_.has_value()) {
      if (auto* clipboard = ui::Clipboard::GetForCurrentThread()) {
        const ui::ClipboardSequenceNumberToken current_seqno =
            clipboard->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);
        if (current_seqno == seqno &&
            (!pending_baseline_seqno_.has_value() ||
             current_seqno != *pending_baseline_seqno_)) {
          SetEligibilityForSeqno(current_seqno, *pending_eligibility_);
        }
      }
    }
    if (last_seqno_ && *last_seqno_ == seqno) {
      return last_eligibility_;
    }
    return std::nullopt;
  }

  bool HasPendingForTesting() const { return pending_eligibility_.has_value(); }

  void SetSeqnoForTesting(ui::ClipboardSequenceNumberToken seqno) {
    if (pending_eligibility_.has_value()) {
      SetEligibilityForSeqno(seqno, *pending_eligibility_);
    }
  }

  void ResetForTesting() {
    ClearPending();
    last_seqno_.reset();
    last_eligibility_.reset();
  }

 private:
  void ScheduleFinalizePendingCopy() {
    if (base::SingleThreadTaskRunner::HasCurrentDefault() &&
        !finalize_task_posted_) {
      finalize_task_posted_ = true;
      const uint64_t generation = pending_generation_;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&GlicClipboardEligibilityMonitor::FinalizePendingCopy,
                         base::Unretained(this), generation));
    }
  }

  void FinalizePendingCopy(uint64_t generation) {
    if (generation != pending_generation_ ||
        !pending_eligibility_.has_value()) {
      return;
    }
    if (auto* clipboard = ui::Clipboard::GetForCurrentThread()) {
      const ui::ClipboardSequenceNumberToken current_seqno =
          clipboard->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);
      if (!pending_baseline_seqno_.has_value() ||
          current_seqno != *pending_baseline_seqno_) {
        last_seqno_ = current_seqno;
        last_eligibility_ = *pending_eligibility_;
      }
    }
    ClearPending();
  }

  void StartObserving() {
    if (!observing_) {
      ui::ClipboardMonitor::GetInstance()->AddObserver(this);
      observing_ = true;
    }
  }

  void StopObserving() {
    if (observing_) {
      ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
      observing_ = false;
    }
  }

  void ClearPending() {
    ++pending_generation_;
    pending_eligibility_.reset();
    pending_baseline_seqno_.reset();
    finalize_task_posted_ = false;
    StopObserving();
  }

  uint64_t pending_generation_ = 0;
  bool observing_ = false;
  bool finalize_task_posted_ = false;
  std::optional<bool> pending_eligibility_;
  std::optional<ui::ClipboardSequenceNumberToken> pending_baseline_seqno_;
  std::optional<ui::ClipboardSequenceNumberToken> last_seqno_;
  std::optional<bool> last_eligibility_;
};

std::optional<bool> GetClipboardCopyEligibility(
    const content::ClipboardEndpoint& source) {
  if (!base::FeatureList::IsEnabled(features::kGlicPasteEligibilityCheck) ||
      !base::FeatureList::IsEnabled(features::kGlicWebPasteEligibilityCheck)) {
    return std::nullopt;
  }

  // We only track copy eligibility if it originated from a web page frame.
  content::RenderFrameHost* rfh = source.render_frame_host();
  if (!rfh) {
    return std::nullopt;
  }
  content::WebContents* web_contents = source.web_contents();
  if (!web_contents) {
    return std::nullopt;
  }

  // Page context eligibility is tied to tabs. Copies from non-tab contexts
  // (like side panels or webui) do not have this helper.
  tabs::TabInterface* source_tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!source_tab) {
    return std::nullopt;
  }
  auto* helper = tabs::PageContextEligibilityHelper::From(source_tab);
  if (!helper) {
    return std::nullopt;
  }

  // Evaluate the page's eligibility at the moment of the copy.
  optimization_guide::PageContextEligibilityStatus status =
      helper->IsPageContextEligible();
  return status == optimization_guide::PageContextEligibilityStatus::kEligible;
}

void StageClipboardCopyEligibility(std::optional<bool> is_eligible,
                                   bool clear_at_end_of_task) {
  GlicClipboardEligibilityMonitor::GetInstance()->StageCopyEligibility(
      is_eligible, clear_at_end_of_task);
}

bool HasPendingClipboardCopyEligibilityForTesting() {
  return GlicClipboardEligibilityMonitor::GetInstance()
      ->HasPendingForTesting();  // IN-TEST
}

void SetClipboardEligibilityForSeqno(ui::ClipboardSequenceNumberToken seqno,
                                     bool is_eligible) {
  GlicClipboardEligibilityMonitor::GetInstance()->SetEligibilityForSeqno(
      seqno, is_eligible);
}

void OnBeforeClipboardCopy(const content::ClipboardEndpoint& source) {
  if (!base::FeatureList::IsEnabled(features::kGlicPasteEligibilityCheck) ||
      !base::FeatureList::IsEnabled(features::kGlicWebPasteEligibilityCheck)) {
    return;
  }

  // Stash (or clear, for non-tab/OS sources) the copy-time eligibility status
  // in the global monitor so it binds to the resulting clipboard sequence
  // number without leaking stale pending state across unrelated copies.
  StageClipboardCopyEligibility(GetClipboardCopyEligibility(source));
}

namespace {

class PendingClipboardCopyCleanup {
 public:
  explicit PendingClipboardCopyCleanup(bool armed) : armed_(armed) {}
  PendingClipboardCopyCleanup(PendingClipboardCopyCleanup&& other)
      : armed_(std::exchange(other.armed_, false)) {}
  PendingClipboardCopyCleanup& operator=(PendingClipboardCopyCleanup&& other) {
    if (this != &other) {
      DisarmAndClearIfNeeded();
      armed_ = std::exchange(other.armed_, false);
    }
    return *this;
  }
  ~PendingClipboardCopyCleanup() { DisarmAndClearIfNeeded(); }

  void Disarm() { armed_ = false; }

 private:
  void DisarmAndClearIfNeeded() {
    if (std::exchange(armed_, false)) {
      StageClipboardCopyEligibility(std::nullopt);
    }
  }

  bool armed_ = false;
};

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
void ShowPasteBlockDialogIfNeeded(content::WebContents* web_contents) {
  data_controls::DataControlsDialogFactory* factory =
#if BUILDFLAG(IS_ANDROID)
      data_controls::AndroidDataControlsDialogFactory::GetInstance();
#else
      data_controls::DesktopDataControlsDialogFactory::GetInstance();
#endif
  if (factory) {
    factory->ShowDialogIfNeeded(
        web_contents,
        data_controls::DataControlsDialog::Type::kClipboardPasteBlock);
  }
}

bool ShouldBlockPasteForRestrictedCopyImpl(
    bool has_source_browser_context,
    content::WebContents* destination_web_contents,
    const ui::ClipboardSequenceNumberToken& seqno) {
  if (!destination_web_contents || !IsGlicGuest(destination_web_contents) ||
      seqno != data_controls::GetLastReplacedClipboardData().seqno) {
    return false;
  }

  switch (data_controls::GetLastReplacedClipboardData().restriction_level) {
    case data_controls::CopyRestrictionLevel::kBlocked:
      ShowPasteBlockDialogIfNeeded(destination_web_contents);
      return true;
    case data_controls::CopyRestrictionLevel::kOngoingScan:
      return true;
    case data_controls::CopyRestrictionLevel::kKeptInManagedChrome:
      if (!has_source_browser_context) {
        ShowPasteBlockDialogIfNeeded(destination_web_contents);
        return true;
      }
      return false;
    case data_controls::CopyRestrictionLevel::kNone:
      return false;
  }
}
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

}  // namespace

content::ContentBrowserClient::IsClipboardCopyAllowedCallback
WrapClipboardCopyAllowedCallback(
    const content::ClipboardEndpoint& source,
    const content::ClipboardPasteData& data,
    content::ContentBrowserClient::IsClipboardCopyAllowedCallback callback) {
  if (!base::FeatureList::IsEnabled(features::kGlicPasteEligibilityCheck) ||
      !base::FeatureList::IsEnabled(features::kGlicWebPasteEligibilityCheck)) {
    return callback;
  }

  std::optional<bool> copy_eligibility = GetClipboardCopyEligibility(source);
  // Clear any previously staged pending eligibility immediately so non-tab
  // copies, blocked copies, and in-flight copy warning/scanning dialogs never
  // leave stale pending state armed.
  StageClipboardCopyEligibility(std::nullopt);
  if (!copy_eligibility.has_value()) {
    return callback;
  }

  return base::BindOnce(
      [](bool is_eligible, bool input_was_empty,
         PendingClipboardCopyCleanup cleanup,
         content::ContentBrowserClient::IsClipboardCopyAllowedCallback
             inner_callback,
         const ui::ClipboardFormatType& format_type,
         const content::ClipboardPasteData& allowed_data,
         std::optional<std::u16string> replacement_data) {
        cleanup.Disarm();
        if (inner_callback.IsCancelled()) {
          StageClipboardCopyEligibility(std::nullopt);
          return;
        }
        if (format_type != ui::ClipboardFormatType::WebCustomFormatMap() &&
            !input_was_empty && allowed_data.empty() &&
            !replacement_data.has_value()) {
          StageClipboardCopyEligibility(std::nullopt);
          std::move(inner_callback)
              .Run(format_type, allowed_data, std::move(replacement_data));
          return;
        }

        std::optional<ui::ClipboardSequenceNumberToken> seqno_before;
        if (auto* clipboard = ui::Clipboard::GetForCurrentThread()) {
          seqno_before =
              clipboard->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);
        }
        StageClipboardCopyEligibility(is_eligible,
                                      /*clear_at_end_of_task=*/false);
        std::move(inner_callback)
            .Run(format_type, allowed_data, std::move(replacement_data));
        if (auto* clipboard = ui::Clipboard::GetForCurrentThread()) {
          const ui::ClipboardSequenceNumberToken seqno_after =
              clipboard->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);
          if (!seqno_before.has_value() || seqno_after != *seqno_before) {
            SetClipboardEligibilityForSeqno(seqno_after, is_eligible);
          }
        }
      },
      *copy_eligibility, data.empty(),
      PendingClipboardCopyCleanup(/*armed=*/true), std::move(callback));
}

void SetClipboardEligibilitySeqnoForTesting(
    ui::ClipboardSequenceNumberToken seqno) {
  auto* monitor = GlicClipboardEligibilityMonitor::GetInstance();
  monitor->SetSeqnoForTesting(seqno);  // IN-TEST
}

void ResetClipboardEligibilityMonitorForTesting() {
  GlicClipboardEligibilityMonitor::GetInstance()->ResetForTesting();  // IN-TEST
}

bool IsClipboardPasteAllowed(const content::ClipboardEndpoint& source,
                             const content::ClipboardEndpoint& destination,
                             const ui::ClipboardMetadata& metadata) {
  if (!base::FeatureList::IsEnabled(features::kGlicPasteEligibilityCheck)) {
    return false;
  }

  std::string_view source_suffix = source.web_contents() ? "Web" : "OS";

  if (!source.web_contents()) {
    // Allow pastes from external (OS) sources. We do not have sufficient
    // provenance metadata to reliably block these without breaking the
    // clipboard.
    return true;
  }

  if (!base::FeatureList::IsEnabled(features::kGlicWebPasteEligibilityCheck)) {
    return false;
  }

  // Pasting from Glic to Glic is always allowed.
  if (IsGlicGuest(source.web_contents())) {
    return true;
  }

  if (source.browser_context() != destination.browser_context()) {
    base::UmaHistogramEnumeration(
        base::StrCat({"Glic.Paste.FailedEligibilityReason.", source_suffix}),
        GlicPasteFailedEligibilityReason::kCrossProfile);
    return false;
  }

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
  if (metadata.seqno == data_controls::GetLastReplacedClipboardData().seqno &&
      data_controls::GetLastReplacedClipboardData().restriction_level ==
          data_controls::CopyRestrictionLevel::kOngoingScan) {
    return false;
  }
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

  std::optional<bool> was_eligible =
      GlicClipboardEligibilityMonitor::GetInstance()->GetEligibility(
          metadata.seqno);

  if (was_eligible.has_value()) {
    if (!*was_eligible) {
      base::UmaHistogramEnumeration(
          base::StrCat({"Glic.Paste.FailedEligibilityReason.", source_suffix}),
          GlicPasteFailedEligibilityReason::kPageContextIneligible);
      return false;
    }
    return true;
  }

  // If there's no matching sequence number, it means the copy was from
  // an unknown source (or the monitor missed it), so we fail the paste.
  base::UmaHistogramEnumeration(
      base::StrCat({"Glic.Paste.FailedEligibilityReason.", source_suffix}),
      GlicPasteFailedEligibilityReason::kPageContextInvalidated);
  return false;
}

bool MaybeBlockPasteForRestrictedCopy(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const ui::ClipboardMetadata& metadata) {
#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
  return ShouldBlockPasteForRestrictedCopyImpl(
      /*has_source_browser_context=*/source.browser_context() != nullptr,
      destination.web_contents(), metadata.seqno);
#else
  return false;
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
}

content::ContentBrowserClient::IsClipboardPasteAllowedCallback
WrapClipboardPasteAllowedCallback(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const ui::ClipboardMetadata& metadata,
    content::ContentBrowserClient::IsClipboardPasteAllowedCallback callback) {
#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
  if (!destination.web_contents() || !IsGlicGuest(destination.web_contents())) {
    return callback;
  }

  base::WeakPtr<content::BrowserContext> source_browser_context =
      source.browser_context() ? source.browser_context()->GetWeakPtr()
                               : nullptr;
  base::WeakPtr<content::WebContents> destination_web_contents =
      destination.web_contents()->GetWeakPtr();
  return base::BindOnce(
      [](base::WeakPtr<content::BrowserContext> source_browser_context,
         base::WeakPtr<content::WebContents> destination_web_contents,
         ui::ClipboardSequenceNumberToken seqno,
         content::ContentBrowserClient::IsClipboardPasteAllowedCallback
             inner_callback,
         std::optional<content::ClipboardPasteData> allowed_data) {
        if (allowed_data.has_value() &&
            ShouldBlockPasteForRestrictedCopyImpl(
                /*has_source_browser_context=*/static_cast<bool>(
                    source_browser_context),
                destination_web_contents.get(), seqno)) {
          std::move(inner_callback).Run(std::nullopt);
          return;
        }
        std::move(inner_callback).Run(std::move(allowed_data));
      },
      std::move(source_browser_context), std::move(destination_web_contents),
      metadata.seqno, std::move(callback));
#else
  return callback;
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
}

}  // namespace glic
