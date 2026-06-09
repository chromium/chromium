// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/guest_util.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/buildflags/buildflags.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
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

namespace glic {

BASE_FEATURE(kGlicGuestUrlMultiInstanceParam, base::FEATURE_ENABLED_BY_DEFAULT);

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

  url = MaybeAddMultiInstanceParameter(url);

  return GetLocalizedGuestURL(url);
}

url::Origin GetGuestOrigin() {
  return url::Origin::Create(GetGuestURL());
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

GURL MaybeAddMultiInstanceParameter(const GURL& guest_url) {
  if (base::FeatureList::IsEnabled(kGlicGuestUrlMultiInstanceParam)) {
    return net::AppendOrReplaceQueryParameter(guest_url, "mode", "mi");
  }
  return guest_url;
}

bool IsGlicWebUI(const content::WebContents* web_contents) {
  return web_contents &&
         GlicWebUiData::FromWebContents(web_contents) != nullptr;
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

#if !BUILDFLAG(IS_ANDROID)
    // TODO(harringtond): This looks wrong, either fix or document this.
    blink::web_pref::WebPreferences prefs(top->GetOrCreateWebPreferences());
    prefs.default_font_size =
        top->GetOrCreateWebPreferences().default_font_size;
    top->SetWebPreferences(prefs);
#else
    // TODO(b/470059315): What do we do for Android?
#endif
  }

  guest_contents->SetUserData(
      "glic::WebviewWebContentsObserver",
      std::make_unique<WebviewWebContentsObserver>(guest_contents));
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

}  // namespace glic
