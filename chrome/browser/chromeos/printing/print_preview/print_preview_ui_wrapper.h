// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_UI_WRAPPER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_UI_WRAPPER_H_

#include <stdint.h>

#include <optional>

#include "base/unguessable_token.h"
#include "base/values.h"
#include "components/printing/common/print.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "ui/gfx/geometry/rect_f.h"

namespace content {

class RenderFrameHost;

}  // namespace content

namespace chromeos {

// Handles webcontent-associated print preview commands. Since webcontents
// cannot be passed to ash, this class serves as a front for handling print
// preview UI-related mojo calls from the renderer process.
class PrintPreviewUiWrapper : public ::printing::mojom::PrintPreviewUI {
 public:
  PrintPreviewUiWrapper();
  PrintPreviewUiWrapper(const PrintPreviewUiWrapper&) = delete;
  PrintPreviewUiWrapper& operator=(const PrintPreviewUiWrapper&) = delete;
  ~PrintPreviewUiWrapper() override;

  // Establish the renderer bindings.
  void BindPrintPreviewUI(content::RenderFrameHost* rfh);

  // Start the process for generating a print preview. Preview generation is
  // done asynchronously, updates are provided via mojom::PrintPreviewUI.
  void GeneratePrintPreview(base::Value::Dict settings);

  // mojom::PrintPreviewUI::
  void SetOptionsFromDocument(
      ::printing::mojom::OptionsFromDocumentParamsPtr params,
      int32_t request_id) override;
  void DidPrepareDocumentForPreview(int32_t document_cookie,
                                    int32_t request_id) override;
  void DidPreviewPage(::printing::mojom::DidPreviewPageParamsPtr params,
                      int32_t request_id) override;
  void MetafileReadyForPrinting(
      ::printing::mojom::DidPreviewDocumentParamsPtr params,
      int32_t request_id) override;
  void PrintPreviewFailed(int32_t document_cookie, int32_t request_id) override;
  void PrintPreviewCancelled(int32_t document_cookie,
                             int32_t request_id) override;
  void PrinterSettingsInvalid(int32_t document_cookie,
                              int32_t request_id) override;
  void DidGetDefaultPageLayout(
      ::printing::mojom::PageSizeMarginsPtr page_layout_in_points,
      const gfx::RectF& printable_area_in_points,
      bool all_pages_have_custom_size,
      bool all_pages_have_custom_orientation,
      int32_t request_id) override;
  void DidStartPreview(::printing::mojom::DidStartPreviewParamsPtr params,
                       int32_t request_id) override;

  // Static function to check if a print preview instance and its request ID
  // should be cancelled.
  static bool ShouldCancelRequest(
      const std::optional<int32_t>& print_preview_ui_id,
      int request_id);

  // Reset local state and remove internal ID.
  void Reset();

 private:
  friend class PrintViewManagerCrosTest;

  // Establish instance ID.
  void SetPreviewUIId();

  // Unique ID of this instance. Note that mojom::PrintManagerHost relies on
  // a `int32_t` to be the identifier of a PrintPreviewUI instance.
  std::optional<int32_t> id_;
  raw_ptr<content::RenderFrameHost> render_frame_host_;
  mojo::AssociatedRemote<::printing::mojom::PrintRenderFrame>
      print_render_frame_;
  mojo::AssociatedReceiver<::printing::mojom::PrintPreviewUI> receiver_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_UI_WRAPPER_H_
