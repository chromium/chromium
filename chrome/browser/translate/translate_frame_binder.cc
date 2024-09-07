// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_frame_binder.h"

#include "chrome/browser/translate/chrome_translate_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace translate {

void BindContentTranslateDriver(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<translate::mojom::ContentTranslateDriver> receiver) {
  // Only valid for the primary main frame.
  if (!render_frame_host->IsInPrimaryMainFrame())
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

void BindContentLanguageDetectionDriver(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<
        language_detection::mojom::ContentLanguageDetectionDriver> receiver) {
  // TOOD(https://crbug.com/354069716): Move binding off RenderFrameHost so that
  // the model can be loaded from JS running in any context at any time.
  // Only valid for the primary main frame. The current code is just mirroring
  // the translate driver to ensure it behaves as before.
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  content::WebContents* const web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents) {
    return;
  }

  ChromeTranslateClient* const translate_client =
      ChromeTranslateClient::FromWebContents(web_contents);
  if (!translate_client) {
    return;
  }

  translate_client->language_detection_driver()->AddReceiver(
      std::move(receiver));
}
}  // namespace translate
