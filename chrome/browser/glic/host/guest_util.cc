// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/guest_util.h"

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "components/guest_view/buildflags/buildflags.h"
#include "components/language/core/common/language_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "url/gurl.h"

// Note: guest_view isn't available on android mobile yet. Once it is, we can
// include these unconditionally.
#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "components/guest_view/browser/guest_view_base.h"
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#endif

namespace glic {

BASE_FEATURE(kGlicGuestUrlMultiInstanceParam, base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

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
  std::string locale = g_browser_process->GetApplicationLocale();
  language::ToTranslateLanguageSynonym(&locale);
  return net::AppendQueryParameter(guest_url, "hl", locale);
}

GURL MaybeAddMultiInstanceParameter(const GURL& guest_url) {
  if (GlicEnabling::IsMultiInstanceEnabled() &&
      base::FeatureList::IsEnabled(kGlicGuestUrlMultiInstanceParam)) {
    return net::AppendOrReplaceQueryParameter(guest_url, "mode", "mi");
  }
  return guest_url;
}

bool IsGlicWebUI(const content::WebContents* web_contents) {
  return web_contents &&
         web_contents->GetLastCommittedURL() == chrome::kChromeUIGlicURL;
}

bool OnGuestAdded(content::WebContents* guest_contents) {
#if !BUILDFLAG(ENABLE_GUEST_VIEW) || !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // NEEDS_MOBILE_ANDROID_IMPL: Guest view is not yet enabled on mobile android.
  // Also, we're using extensions::WebViewGuest, which will need refactored
  // when we have a guest_view that doesn't use extensions.
  return false;
#else
  // Only handle the glic webview. Explicitly check the guest type here in case
  // glic's web content happens to load a mime handler.
  if (!extensions::WebViewGuest::FromWebContents(guest_contents)) {
    return false;
  }

  content::WebContents* top =
      guest_view::GuestViewBase::GetTopLevelWebContents(guest_contents);
  CHECK(top);
  if (!IsGlicWebUI(top)) {
    return false;
  }
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(top->GetBrowserContext());
  if (!service) {
    return false;
  }

#if !BUILDFLAG(IS_ANDROID)
  guest_contents->SetSupportsDraggableRegions(true);
#endif  // !BUILDFLAG(IS_ANDROID)

  service->GuestAdded(guest_contents);

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
#endif
}

}  // namespace glic
