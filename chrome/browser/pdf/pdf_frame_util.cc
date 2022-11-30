// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_frame_util.h"

#include <functional>

#include "base/bind.h"
#include "base/check.h"
#include "chrome/common/pdf_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

namespace pdf_frame_util {

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
