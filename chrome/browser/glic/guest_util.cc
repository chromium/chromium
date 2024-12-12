// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/guest_util.h"

#include "base/command_line.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "content/public/browser/navigation_handle.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
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
  bool hasGlicGuestURL = command_line->HasSwitch(::switches::kGlicGuestURL);
  return GURL(hasGlicGuestURL
                  ? command_line->GetSwitchValueASCII(::switches::kGlicGuestURL)
                  : features::kGlicGuestURL.Get());
}

url::Origin GetGuestOrigin() {
  return url::Origin::Create(GetGuestURL());
}

bool OnGuestAdded(content::WebContents* guest_contents) {
  content::WebContents* top =
      guest_view::GuestViewBase::GetTopLevelWebContents(guest_contents);

  if (!top || top->GetLastCommittedURL() != chrome::kChromeUIGlicURL) {
    return false;
  }

  // TODO(crbug.com/382322927): This could instead be done by having all guest
  // WebContents inherit background color from their embedders.
  guest_contents->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);

  guest_contents->SetUserData(
      "glic::WebviewWebContentsObserver",
      std::make_unique<WebviewWebContentsObserver>(guest_contents));
  return true;
}

}  // namespace glic
