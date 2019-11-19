// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_PRINT_SPOOLER_PRINT_SESSION_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_PRINT_SPOOLER_PRINT_SESSION_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/ash/arc_custom_tab_modal_dialog_host.h"
#include "chrome/services/printing/public/mojom/pdf_flattener.mojom.h"
#include "components/arc/mojom/print_spooler.mojom.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace ash {
class ArcCustomTab;
}  // namespace ash

namespace content {
class WebContents;
}  // namespace content

namespace arc {

// Implementation of PrintSessionHost interface. Also used by other classes to
// send print-related messages to ARC.
class PrintSessionImpl : public mojom::PrintSessionHost,
                         public ArcCustomTabModalDialogHost,
                         public content::WebContentsUserData<PrintSessionImpl>,
                         public printing::mojom::PrintRenderer {
 public:
  static mojom::PrintSessionHostPtr Create(
      std::unique_ptr<content::WebContents> web_contents,
      std::unique_ptr<ash::ArcCustomTab> custom_tab,
      mojom::PrintSessionInstancePtr instance);

  ~PrintSessionImpl() override;

  // Called when print preview is closed.
  void OnPrintPreviewClosed();

 private:
  PrintSessionImpl(std::unique_ptr<content::WebContents> web_contents,
                   std::unique_ptr<ash::ArcCustomTab> custom_tab,
                   mojom::PrintSessionInstancePtr instance,
                   mojom::PrintSessionHostRequest request);
  friend class content::WebContentsUserData<PrintSessionImpl>;

  // printing::mojom::PrintRenderer:
  void CreatePreviewDocument(base::Value job_settings,
                             CreatePreviewDocumentCallback callback) override;

  // Called once the preview document has been created by ARC. The preview
  // document must be read and flattened before being returned by the
  // PrintRenderer.
  void OnPreviewDocumentCreated(CreatePreviewDocumentCallback callback,
                                mojo::ScopedHandle preview_document,
                                int64_t data_size);

  // Called once the preview document from ARC has been read. The preview
  // document must be flattened before being returned by the PrintRenderer.
  void OnPreviewDocumentRead(
      CreatePreviewDocumentCallback callback,
      base::ReadOnlySharedMemoryRegion preview_document_region);

  // Used to close the ARC Custom Tab used for printing. If the remote end
  // closes the connection, the ARC Custom Tab and print preview will be closed.
  // If printing has already started, this will not cancel any active print job.
  void Close();

  // Opens Chrome print preview after waiting for the PDF plugin to load.
  void StartPrintAfterDelay();

  // Used to send messages to ARC and request a new print document.
  mojom::PrintSessionInstancePtr instance_;

  // Binding for PrintRenderer.
  mojo::AssociatedBinding<printing::mojom::PrintRenderer>
      print_renderer_binding_;

  // Used to bind the PrintSessionHost interface implementation to a message
  // pipe.
  mojo::Binding<mojom::PrintSessionHost> session_binding_;

  // Remote interface used to flatten a PDF (preview document).
  mojo::Remote<printing::mojom::PdfFlattener> pdf_flattener_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<PrintSessionImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrintSessionImpl);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_PRINT_SPOOLER_PRINT_SESSION_IMPL_H_
