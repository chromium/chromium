// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/customtabs/client_data_header_web_contents_observer.h"

#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace customtabs {

ClientDataHeaderWebContentsObserver::~ClientDataHeaderWebContentsObserver() =
    default;

ClientDataHeaderWebContentsObserver::ClientDataHeaderWebContentsObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<ClientDataHeaderWebContentsObserver>(
          *web_contents) {}

void ClientDataHeaderWebContentsObserver::SetHeader(const std::string& header) {
  header_ = header;
  web_contents()->ForEachRenderFrameHost(
      [this](content::RenderFrameHost* render_frame_host) {
        UpdateFrameCCTHeader(render_frame_host);
      });
}

void ClientDataHeaderWebContentsObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  UpdateFrameCCTHeader(render_frame_host);
}

void ClientDataHeaderWebContentsObserver::UpdateFrameCCTHeader(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->IsRenderFrameLive())
    return;
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> client;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
  client->SetCCTClientHeader(header_);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ClientDataHeaderWebContentsObserver);

}  // namespace customtabs
