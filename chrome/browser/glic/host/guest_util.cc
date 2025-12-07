// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/guest_util.h"

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/language/core/common/language_util.h"
#include "content/public/browser/navigation_handle.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

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
}

}  // namespace glic
