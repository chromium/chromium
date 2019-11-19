// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language/translate_frame_binder.h"

#include "chrome/browser/translate/chrome_translate_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace language {

void BindContentTranslateDriver(
    mojo::PendingReceiver<translate::mojom::ContentTranslateDriver> receiver,
    content::RenderFrameHost* render_frame_host) {
  // Only valid for the main frame.
  if (render_frame_host->GetParent())
    return;

  content::WebContents* const web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;

  ChromeTranslateClient* const translate_client =
      ChromeTranslateClient::FromWebContents(web_contents);
  if (!translate_client)
    return;

  translate_client->translate_driver()->AddReceiver(std::move(receiver));
}

}  // namespace language
