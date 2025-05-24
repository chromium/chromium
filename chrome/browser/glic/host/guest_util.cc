// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/guest_util.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
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

namespace {
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
                             blink::mojom::kAutoplayFlagHighMediaEngagement);
  }
};

}  // namespace

GURL GetGuestURL() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  bool has_glic_guest_url = command_line->HasSwitch(::switches::kGlicGuestURL);
  GURL base_url =
      GURL(has_glic_guest_url
               ? command_line->GetSwitchValueASCII(::switches::kGlicGuestURL)
               : features::kGlicGuestURL.Get());
  if (base_url.is_empty()) {
    LOG(ERROR) << "No glic guest url";
  }
  std::string locale = g_browser_process->GetApplicationLocale();
  language::ToTranslateLanguageSynonym(&locale);
  GURL localized_url =
      net::AppendOrReplaceQueryParameter(base_url, "hl", locale);
  return localized_url;
}

url::Origin GetGuestOrigin() {
  return url::Origin::Create(GetGuestURL());
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
  return true;
}

}  // namespace glic
