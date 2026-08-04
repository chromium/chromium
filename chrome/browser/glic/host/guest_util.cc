// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/guest_util.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/supports_user_data.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/glic_hotkey.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/host/glic_guest_observer.h"
#include "chrome/browser/glic/host/glic_page_handler.h"
#include "chrome/browser/glic/host/glic_ui.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/service/glic_tab_contents_swapper.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/google/core/common/google_util.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/buildflags/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/skills/features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/content_features.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/base/url_util.h"
#include "pdf/buildflags.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#else
#include "components/guest_view/browser/slim_web_view/slim_web_view_guest.h"  // nogncheck
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/clipboard_types.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_metadata.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#endif

namespace glic {

#if !BUILDFLAG(IS_ANDROID)
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
#endif

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

  raw_ptr<content::WebContents> webui_contents_;
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(GlicWebUiData);

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

// Attached to Guest WebContents to identify it directly.
class GlicGuestMarker : public content::WebContentsUserData<GlicGuestMarker> {
 public:
  ~GlicGuestMarker() override = default;

 private:
  explicit GlicGuestMarker(content::WebContents* web_contents)
      : content::WebContentsUserData<GlicGuestMarker>(*web_contents) {}
  friend class content::WebContentsUserData<GlicGuestMarker>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(GlicGuestMarker);

// LINT.IfChange(WebViewAutoPlayProgress)
enum class WebViewAutoPlayProgress {
  kWebContentsObserverRegistered = 0,
  kAutoPlayGrantedForPrimaryRFH = 1,
  kAutoPlayGrantedForOtherRFH = 2,
  kMaxValue = kAutoPlayGrantedForOtherRFH,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:WebViewAutoPlayProgress)

// Observes the glic webview's `WebContents`.
class WebviewWebContentsObserver : public content::WebContentsObserver,
                                   public base::SupportsUserData::Data {
 public:
  explicit WebviewWebContentsObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  void ReadyToCommitNavigation(content::NavigationHandle* handle) override {
    // Enable autoplay for the webview.
    content::RenderFrameHost* frame = handle->GetRenderFrameHost();
    mojo::AssociatedRemote<blink::mojom::AutoplayConfigurationClient> client;
    frame->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    client->AddAutoplayFlags(GetGuestOrigin(),
                             blink::mojom::kAutoplayFlagForceAllow);
    VLOG(1) << "Granted Glic AutoPlay for origin=\"" << GetGuestOrigin()
            << "\" at " << (handle->IsInPrimaryMainFrame() ? "main " : "")
            << "RFH with url=\"" << handle->GetURL() << "\"";
    base::UmaHistogramEnumeration(
        "Glic.Host.WebView.AutoPlay",
        handle->IsInPrimaryMainFrame()
            ? WebViewAutoPlayProgress::kAutoPlayGrantedForPrimaryRFH
            : WebViewAutoPlayProgress::kAutoPlayGrantedForOtherRFH);
  }
};

}  // namespace

bool IsGlicGuest(content::WebContents* web_contents) {
  if (!web_contents ||
      GlicGuestMarker::FromWebContents(web_contents) == nullptr) {
    return false;
  }
  auto* guest_view = guest_view::GuestViewBase::FromWebContents(web_contents);
  return guest_view && guest_view->attached();
}

void MarkProcessAsGlic(content::RenderProcessHost* rph) {
  GlicProcessUserData::MarkProcess(rph);
}

void CreateGlicWebUiData(content::WebContents* webui_contents) {
  GlicWebUiData::CreateForWebContents(webui_contents);
}

GURL GetGuestURL() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  bool has_glic_guest_url = command_line->HasSwitch(::switches::kGlicGuestURL);
  GURL url =
      GURL(has_glic_guest_url
               ? command_line->GetSwitchValueASCII(::switches::kGlicGuestURL)
               : features::kGlicGuestURL.Get());

  // If a preset url is enabled, use it instead.
  url = MaybeApplyPresetGuestUrl(std::move(url));

  if (url.is_empty()) {
    LOG(ERROR) << "No glic guest url";
    return GURL();
  }

  return GetLocalizedGuestURL(url);
}

url::Origin GetGuestOrigin() {
  return url::Origin::Create(GetGuestURL());
}

std::string GetGlicAllowedOrigins(bool is_internal_google_account) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  std::string allowed_origins =
      command_line->GetSwitchValueASCII(::switches::kGlicAllowedOrigins);
  if (allowed_origins.empty()) {
    allowed_origins = features::kGlicAllowedOriginsOverride.Get();
  }

  // Allow corp origins for @google accounts.
  if (is_internal_google_account) {
    allowed_origins += " https://*.corp.google.com";
  }
  return allowed_origins;
}

bool IsOriginAllowedGlicApi(const url::Origin& origin) {
  if (origin.opaque()) {
    return false;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(::switches::kGlicDev)) {
    return true;
  }
  if (GetGuestOrigin().IsSameOriginWith(origin)) {
    return true;
  }
  std::string api_allowed_origins = features::kGlicApiAllowedOrigins.Get();
  if (!api_allowed_origins.empty()) {
    for (const std::string& allowed :
         base::SplitString(api_allowed_origins, " ", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY)) {
      url::Origin allowed_origin = url::Origin::Create(GURL(allowed));
      if (!allowed_origin.opaque() && allowed_origin.IsSameOriginWith(origin)) {
        return true;
      }
    }
  }
  return false;
}

bool IsFrameAllowedGlicApi(content::RenderFrameHost& frame_host) {
  if (!base::FeatureList::IsEnabled(features::kGlicEnableMojoJs)) {
    return false;
  }
  content::WebContents* guest_contents =
      content::WebContents::FromRenderFrameHost(&frame_host);
  if (!guest_contents || !IsGlicGuest(guest_contents)) {
    return false;
  }
  return IsOriginAllowedGlicApi(frame_host.GetLastCommittedOrigin());
}

void BindGlicWebClientHandler(
    content::RenderFrameHost* rfh,
    mojo::PendingReceiver<glic::mojom::WebClientHandler> receiver) {
  if (!base::FeatureList::IsEnabled(features::kGlicEnableMojoJs)) {
    return;
  }
  if (!IsFrameAllowedGlicApi(*rfh)) {
    return;
  }
  content::WebContents* guest_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!guest_contents) {
    return;
  }
  content::WebContents* top =
      guest_view::GuestViewBase::GetTopLevelWebContents(guest_contents);
  if (!top) {
    return;
  }
  auto* glic_ui = GlicUI::From(top);
  if (!glic_ui || !glic_ui->host()) {
    return;
  }
  glic_ui->host()->CreateWebClient(std::move(receiver));
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

bool IsGlicOwnedTab(tabs::TabInterface* tab) {
  if (!tab || !tab->GetContents()) {
    return false;
  }
  return tab->GetContents()->GetUserData(GlicPlaceholderUserData::kKey) ||
         IsGlicWebUI(tab->GetContents());
}

bool IsProcessHostForGlic(content::RenderProcessHost* process_host) {
  return process_host &&
         GlicProcessUserData::FromProcessHost(process_host) != nullptr;
}

content::WebContents* GetGlicGuestWebContents(
    content::WebContents* webui_contents) {
  if (!webui_contents) {
    return nullptr;
  }
  auto* data = GlicWebUiData::FromWebContents(webui_contents);
  return data ? data->guest_contents() : nullptr;
}

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

#if !BUILDFLAG(IS_ANDROID)
  guest_contents->SetSupportsDraggableRegions(true);
#endif  // !BUILDFLAG(IS_ANDROID)

  if (auto* data = GlicWebUiData::FromWebContents(top)) {
    data->SetGuestContents(guest_contents);
    GlicGuestMarker::CreateForWebContents(guest_contents);
    GlicProcessUserData::MarkProcess(
        guest_contents->GetPrimaryMainFrame()->GetProcess());

    PrefsTabHelper::CreateForWebContents(guest_contents);

#if !BUILDFLAG(IS_ANDROID)
    // TODO(harringtond): This looks wrong, either fix or document this.
    blink::web_pref::WebPreferences prefs(top->GetOrCreateWebPreferences());
    prefs.default_font_size =
        top->GetOrCreateWebPreferences().default_font_size;
    top->SetWebPreferences(prefs);
#else
    // Apply the persisted zoom level to the guest WebContents.
    if (Profile* profile =
            Profile::FromBrowserContext(top->GetBrowserContext())) {
      // LINT.IfChange(GlicZoomFactors)
      int zoom_percent = std::clamp(
          profile->GetPrefs()->GetInteger(prefs::kGlicZoomLevel), 100, 200);
      double zoom_factor = zoom_percent / 100.0;
      // LINT.ThenChange(//chrome/browser/resources/glic/webview.ts:GlicZoomFactors,
      // //chrome/browser/glic/host/glic_page_handler.cc:GlicZoomFactors)
      double zoom_level = blink::ZoomFactorToZoomLevel(zoom_factor);
      content::HostZoomMap::SetZoomLevel(guest_contents, zoom_level);
    }
#endif
  }

  guest_contents->SetUserData(
      "glic::WebviewWebContentsObserver",
      std::make_unique<WebviewWebContentsObserver>(guest_contents));
  if (base::FeatureList::IsEnabled(features::kGlicEnableMojoJs)) {
    glic::GlicGuestObserver::CreateForWebContents(guest_contents);
  }
  VLOG(1) << "Registered glic::WebviewWebContentsObserver for guest "
             "WebContents with url=\""
          << guest_contents->GetVisibleURL() << "\"";
  base::UmaHistogramEnumeration(
      "Glic.Host.WebView.AutoPlay",
      WebViewAutoPlayProgress::kWebContentsObserverRegistered);
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
#endif
  state->host_capabilities.push_back(mojom::HostCapability::kMultiInstance);

  if (base::FeatureList::IsEnabled(features::kGlicNoWebUiLoader)) {
    state->host_capabilities.push_back(mojom::HostCapability::kNoWebUiLoader);
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

  if (!GlicEnabling::HasConsentedForProfile(profile)) {
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
  state->enable_trust_first_onboarding =
      !GlicEnabling::HasConsentedForProfile(profile);
  state->onboarding_completed = GlicEnabling::HasConsentedForProfile(profile);
  state->enable_skills = base::FeatureList::IsEnabled(features::kSkillsEnabled);
  state->enable_get_tab_favicon_by_id =
      base::FeatureList::IsEnabled(features::kGlicGetTabFaviconById);
  state->enable_process_counter_abuse_verdict =
      base::FeatureList::IsEnabled(features::kGlicProcessCounterAbuseVerdict);

  std::optional<glic::mojom::GeminiEnterpriseSettings>
      gemini_enterprise_settings =
          GlicEnabling::GetGeminiEnterpriseSettings(profile);
  if (gemini_enterprise_settings.has_value()) {
    state->gemini_enterprise_settings =
        glic::mojom::GeminiEnterpriseSettings::New(
            gemini_enterprise_settings->project_id,
            gemini_enterprise_settings->app_id,
            gemini_enterprise_settings->location);
  }

  state->enable_gmail_otp_opt_in =
      base::FeatureList::IsEnabled(features::kGlicActorAutofillOneTimePassword);
  state->enable_gmail_otp_confirmation =
      base::FeatureList::IsEnabled(features::kGlicActorAutofillOneTimePassword);
  state->file_upload_policy_state =
      glic::prefs::GetFileUploadAllowedCapability(profile->GetPrefs());
}

#if !BUILDFLAG(IS_ANDROID)

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
// clipboard changes to safely grab the newly generated sequence number after
// a copy completes, and compares against this sequence number at paste time.
class GlicClipboardEligibilityMonitor : public ui::ClipboardObserver {
 public:
  static GlicClipboardEligibilityMonitor* GetInstance() {
    static base::NoDestructor<GlicClipboardEligibilityMonitor> instance;
    return instance.get();
  }

  GlicClipboardEligibilityMonitor() {
    ui::ClipboardMonitor::GetInstance()->AddObserver(this);
  }

  ~GlicClipboardEligibilityMonitor() override {
    ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
  }

  void OnCopyAttempted(bool is_eligible) { pending_eligibility_ = is_eligible; }

  void OnClipboardDataChanged() override {
    if (pending_eligibility_.has_value()) {
      last_seqno_ = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste);
      last_eligibility_ = *pending_eligibility_;
      pending_eligibility_.reset();
    }
  }

  std::optional<bool> GetEligibility(
      const ui::ClipboardSequenceNumberToken& seqno) const {
    if (last_seqno_ && *last_seqno_ == seqno) {
      return last_eligibility_;
    }
    return std::nullopt;
  }

  void SetSeqnoForTesting(ui::ClipboardSequenceNumberToken seqno) {
    if (pending_eligibility_.has_value()) {
      last_seqno_ = seqno;
      last_eligibility_ = *pending_eligibility_;
      pending_eligibility_.reset();
    }
  }

 private:
  std::optional<bool> pending_eligibility_;
  std::optional<ui::ClipboardSequenceNumberToken> last_seqno_;
  std::optional<bool> last_eligibility_;
};

void OnBeforeClipboardCopy(const content::ClipboardEndpoint& source) {
  if (!base::FeatureList::IsEnabled(features::kGlicPasteEligibilityCheck) ||
      !base::FeatureList::IsEnabled(features::kGlicWebPasteEligibilityCheck)) {
    return;
  }

  // We only track copy eligibility if it originated from a web page frame.
  content::RenderFrameHost* rfh = source.render_frame_host();
  if (!rfh) {
    return;
  }
  content::WebContents* web_contents = source.web_contents();
  if (!web_contents) {
    return;
  }

  // Page context eligibility is tied to tabs. Copies from non-tab contexts
  // (like side panels or webui) do not have this helper.
  tabs::TabInterface* source_tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!source_tab) {
    return;
  }
  auto* helper = tabs::PageContextEligibilityHelper::From(source_tab);
  if (!helper) {
    return;
  }

  // Evaluate the page's eligibility at the moment of the copy.
  optimization_guide::PageContextEligibilityStatus status =
      helper->IsPageContextEligible();
  bool is_eligible =
      (status == optimization_guide::PageContextEligibilityStatus::kEligible);

  // Stash the eligibility status in our global monitor. When the OS clipboard
  // actually finishes updating, the monitor will tie this status to the new
  // clipboard sequence number.
  GlicClipboardEligibilityMonitor::GetInstance()->OnCopyAttempted(is_eligible);
}

void SetClipboardEligibilitySeqnoForTesting(
    ui::ClipboardSequenceNumberToken seqno) {
  GlicClipboardEligibilityMonitor::GetInstance()->SetSeqnoForTesting(seqno);
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
#endif

}  // namespace glic
