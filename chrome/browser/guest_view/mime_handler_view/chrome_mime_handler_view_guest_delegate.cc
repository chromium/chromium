// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/mime_handler_view/chrome_mime_handler_view_guest_delegate.h"

#include <utility>

#include "chrome/browser/accessibility/pdf_ocr_metrics.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_menu_helper.h"
#include "components/pdf/common/constants.h"
#include "components/pdf/common/pdf_util.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "content/public/browser/web_contents.h"

namespace extensions {

ChromeMimeHandlerViewGuestDelegate::ChromeMimeHandlerViewGuestDelegate() {
}

ChromeMimeHandlerViewGuestDelegate::~ChromeMimeHandlerViewGuestDelegate() {
}

bool ChromeMimeHandlerViewGuestDelegate::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host);
  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(web_contents);
  DCHECK(menu_delegate);

  std::unique_ptr<RenderViewContextMenuBase> menu = menu_delegate->BuildMenu(
      render_frame_host,
      AddContextMenuParamsPropertiesFromPreferences(web_contents, params));
  menu_delegate->ShowMenu(std::move(menu));
  return true;
}

void ChromeMimeHandlerViewGuestDelegate::RecordLoadMetric(
    bool is_full_page,
    const std::string& mime_type,
    content::BrowserContext* browser_context) {
  if (mime_type != pdf::kPDFMimeType) {
    return;
  }

  ReportPDFLoadStatus(is_full_page
                          ? PDFLoadStatus::kLoadedFullPagePdfWithPdfium
                          : PDFLoadStatus::kLoadedEmbeddedPdfWithPdfium);

  accessibility::RecordPDFOpenedWithA11yFeatureWithPdfOcr();
}

}  // namespace extensions
