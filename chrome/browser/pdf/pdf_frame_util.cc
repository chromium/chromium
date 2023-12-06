// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_frame_util.h"

#include <functional>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/pdf/pdf_viewer_stream_manager.h"
#include "chrome/common/pdf_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "pdf/pdf_features.h"

namespace pdf_frame_util {

content::RenderFrameHost* FindFullPagePdfExtensionHost(
    content::WebContents* contents) {
  CHECK(base::FeatureList::IsEnabled(chrome_pdf::features::kPdfOopif));

  auto* pdf_viewer_stream_manager =
      pdf::PdfViewerStreamManager::FromWebContents(contents);
  if (!pdf_viewer_stream_manager) {
    return nullptr;
  }

  // If `primary_main_frame` has a stream container, it must be a full-page PDF
  // embedder host.
  content::RenderFrameHost* primary_main_frame =
      contents->GetPrimaryMainFrame();
  if (!pdf_viewer_stream_manager->GetStreamContainer(primary_main_frame)) {
    return nullptr;
  }

  // A full-page PDF embedder host should have a child PDF extension host.
  content::RenderFrameHost* extension_host = nullptr;
  primary_main_frame->ForEachRenderFrameHost(
      [&extension_host](content::RenderFrameHost* child_host) {
        if (!IsPdfExtensionOrigin(child_host->GetLastCommittedOrigin())) {
          return;
        }

        CHECK(!extension_host);
        extension_host = child_host;
      });

  return extension_host;
}

content::RenderFrameHost* FindPdfChildFrame(content::RenderFrameHost* rfh) {
  if (!IsPdfInternalPluginAllowedOrigin(rfh->GetLastCommittedOrigin()))
    return nullptr;

  content::RenderFrameHost* pdf_rfh = nullptr;
  rfh->ForEachRenderFrameHost(
      [&pdf_rfh](content::RenderFrameHost* rfh) {
        if (!rfh->GetProcess()->IsPdf())
          return;

        DCHECK(IsPdfInternalPluginAllowedOrigin(
            rfh->GetParent()->GetLastCommittedOrigin()));
        DCHECK(!pdf_rfh);
        pdf_rfh = rfh;
      });

  return pdf_rfh;
}

}  // namespace pdf_frame_util
