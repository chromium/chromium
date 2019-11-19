// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/mime_handler_view/chrome_mime_handler_view_guest_delegate.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/common/pdf_util.h"
#include "components/renderer_context_menu/context_menu_delegate.h"

namespace extensions {

ChromeMimeHandlerViewGuestDelegate::ChromeMimeHandlerViewGuestDelegate() {
}

ChromeMimeHandlerViewGuestDelegate::~ChromeMimeHandlerViewGuestDelegate() {
}

bool ChromeMimeHandlerViewGuestDelegate::HandleContextMenu(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params) {
  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(web_contents);
  DCHECK(menu_delegate);

  std::unique_ptr<RenderViewContextMenuBase> menu =
      menu_delegate->BuildMenu(web_contents, params);
  menu_delegate->ShowMenu(std::move(menu));
  return true;
}

void ChromeMimeHandlerViewGuestDelegate::RecordLoadMetric(
    bool in_main_frame,
    const std::string& mime_type) {
  if (mime_type != kPDFMimeType)
    return;
  base::UmaHistogramEnumeration(
      "PDF.LoadStatus",
      in_main_frame ? PDFLoadStatus::kLoadedFullPagePdfWithPdfium
                    : PDFLoadStatus::kLoadedEmbeddedPdfWithPdfium,
      PDFLoadStatus::kPdfLoadStatusCount);
}

}  // namespace extensions
