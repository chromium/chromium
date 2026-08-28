// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_frame_binder.h"

#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller.h"
#endif
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace translate {

#if !BUILDFLAG(IS_ANDROID)
content::WebContents* GetTargetContents(
    content::WebContents* web_contents) {
  ReadAnythingControllerGlue* glue =
      ReadAnythingControllerGlue::FromWebContents(web_contents);
  if (glue && glue->controller() && glue->controller()->tab()) {
    return glue->controller()->tab()->GetContents();
  }

  ReadAnythingSidePanelControllerGlue* side_glue =
      ReadAnythingSidePanelControllerGlue::FromWebContents(web_contents);
  if (side_glue && side_glue->controller() && side_glue->controller()->tab()) {
    return side_glue->controller()->tab()->GetContents();
  }

  return web_contents;
}
#endif

void BindContentTranslateDriver(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<translate::mojom::ContentTranslateDriver> receiver) {
  // Only valid for the primary main frame.
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  content::WebContents* const web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents) {
    return;
  }

  content::WebContents* target_contents = web_contents;
#if !BUILDFLAG(IS_ANDROID)
  target_contents = GetTargetContents(web_contents);
#endif

  ChromeTranslateClient* const translate_client =
      ChromeTranslateClient::FromWebContents(target_contents);
  if (!translate_client) {
    return;
  }

  translate_client->translate_driver()->AddReceiver(std::move(receiver));
}
}  // namespace translate
