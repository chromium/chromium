// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_PREVIEW_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_PRINTING_PRINT_PREVIEW_MESSAGE_HANDLER_H_

#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "chrome/services/printing/public/mojom/pdf_nup_converter.mojom.h"
#include "components/printing/common/print.mojom-forward.h"
#include "components/services/print_compositor/public/mojom/print_compositor.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "printing/mojom/print.mojom-forward.h"

struct PrintHostMsg_RequestPrintPreview_Params;

namespace base {
class RefCountedMemory;
}

namespace content {
class RenderFrameHost;
class WebContents;
}

namespace gfx {
class Rect;
}

namespace printing {

class PrintPreviewUI;

// Manages the print preview handling for a WebContents.
class PrintPreviewMessageHandler
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PrintPreviewMessageHandler> {
 public:
  ~PrintPreviewMessageHandler() override;

  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;

 private:
  friend class content::WebContentsUserData<PrintPreviewMessageHandler>;

  explicit PrintPreviewMessageHandler(content::WebContents* web_contents);

  // Gets the print preview dialog associated with the WebContents being
  // observed.
  content::WebContents* GetPrintPreviewDialog();

  // Gets the PrintPreviewUI associated with the WebContents being observed and
  // verifies that its id matches |preview_ui_id|.
  PrintPreviewUI* GetPrintPreviewUI(int preview_ui_id);

  // Message handlers.
  void OnRequestPrintPreview(
      content::RenderFrameHost* render_frame_host,
      const PrintHostMsg_RequestPrintPreview_Params& params);
  void OnDidGetDefaultPageLayout(
      const mojom::PageSizeMargins& page_layout_in_points,
      const gfx::Rect& printable_area_in_points,
      bool has_custom_page_size_style,
      const mojom::PreviewIds& ids);
  void OnDidStartPreview(const mojom::DidStartPreviewParams& params,
                         const mojom::PreviewIds& ids);
  void OnDidPrepareForDocumentToPdf(content::RenderFrameHost* render_frame_host,
                                    int document_cookie,
                                    const mojom::PreviewIds& ids);
  void OnDidPreviewPage(content::RenderFrameHost* render_frame_host,
                        const mojom::DidPreviewPageParams& params,
                        const mojom::PreviewIds& ids);
  void OnMetafileReadyForPrinting(content::RenderFrameHost* render_frame_host,
                                  const mojom::DidPreviewDocumentParams& params,
                                  const mojom::PreviewIds& ids);

  void NotifyUIPreviewPageReady(
      PrintPreviewUI* print_preview_ui,
      uint32_t page_number,
      const mojom::PreviewIds& ids,
      scoped_refptr<base::RefCountedMemory> data_bytes);
  void NotifyUIPreviewDocumentReady(
      PrintPreviewUI* print_preview_ui,
      const mojom::PreviewIds& ids,
      scoped_refptr<base::RefCountedMemory> data_bytes);

  // Callbacks for print compositor client.
  void OnCompositePdfPageDone(uint32_t page_number,
                              int document_cookie,
                              const mojom::PreviewIds& ids,
                              mojom::PrintCompositor::Status status,
                              base::ReadOnlySharedMemoryRegion region);
  void OnCompositeToPdfDone(int document_cookie,
                            const mojom::PreviewIds& ids,
                            mojom::PrintCompositor::Status status,
                            base::ReadOnlySharedMemoryRegion region);
  void OnPrepareForDocumentToPdfDone(const mojom::PreviewIds& ids,
                                     mojom::PrintCompositor::Status status);

  void OnNupPdfConvertDone(uint32_t page_number,
                           const mojom::PreviewIds& ids,
                           mojom::PdfNupConverter::Status status,
                           base::ReadOnlySharedMemoryRegion region);
  void OnNupPdfDocumentConvertDone(const mojom::PreviewIds& ids,
                                   mojom::PdfNupConverter::Status status,
                                   base::ReadOnlySharedMemoryRegion region);

  base::WeakPtrFactory<PrintPreviewMessageHandler> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewMessageHandler);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_MESSAGE_HANDLER_H_
