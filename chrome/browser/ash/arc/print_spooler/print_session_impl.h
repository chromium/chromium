// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_PRINT_SPOOLER_PRINT_SESSION_IMPL_H_
#define CHROME_BROWSER_ASH_ARC_PRINT_SPOOLER_PRINT_SESSION_IMPL_H_

#include <memory>

#include "ash/components/arc/mojom/print_spooler.mojom.h"
#include "base/containers/flat_map.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ui/ash/arc/arc_custom_tab_modal_dialog_host.h"
#include "chrome/services/printing/public/mojom/pdf_flattener.mojom.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace arc {

// Implementation of PrintSessionHost interface. Also used by other classes to
// send print-related messages to ARC.
class PrintSessionImpl : public mojom::PrintSessionHost,
                         public ArcCustomTabModalDialogHost,
                         public content::WebContentsUserData<PrintSessionImpl>,
                         public printing::mojom::PrintRenderer,
                         public aura::WindowObserver {
 public:
  static mojo::PendingRemote<mojom::PrintSessionHost> Create(
      std::unique_ptr<content::WebContents> web_contents,
      aura::Window* arc_window,
      mojo::PendingRemote<mojom::PrintSessionInstance> instance);

  PrintSessionImpl(const PrintSessionImpl&) = delete;
  PrintSessionImpl& operator=(const PrintSessionImpl&) = delete;
  ~PrintSessionImpl() override;

  // Called when print preview is closed.
  void OnPrintPreviewClosed();

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  PrintSessionImpl(std::unique_ptr<content::WebContents> web_contents,
                   aura::Window* arc_window,
                   mojo::PendingRemote<mojom::PrintSessionInstance> instance,
                   mojo::PendingReceiver<mojom::PrintSessionHost> receiver);
  friend class content::WebContentsUserData<PrintSessionImpl>;

  // printing::mojom::PrintRenderer:
  void CreatePreviewDocument(base::Value::Dict job_settings,
                             CreatePreviewDocumentCallback callback) override;

  // Called once the preview document has been created by ARC. The preview
  // document must be read and flattened before being returned by the
  // PrintRenderer.
  void OnPreviewDocumentCreated(int request_id,
                                CreatePreviewDocumentCallback callback,
                                mojo::ScopedHandle preview_document,
                                int64_t data_size);

  // Called once the preview document from ARC has been read. The preview
  // document must be flattened before being returned by the PrintRenderer.
  void OnPreviewDocumentRead(
      int request_id,
      CreatePreviewDocumentCallback callback,
      base::ReadOnlySharedMemoryRegion preview_document_region);

  void OnPdfFlattened(int request_id,
                      printing::mojom::FlattenPdfResultPtr result);

  void OnPdfFlattenerDisconnected();

  // Used to close the ARC Custom Tab used for printing. If the remote end
  // closes the connection, the ARC Custom Tab and print preview will be closed.
  // If printing has already started, this will not cancel any active print job.
  void Close();

  // Opens Chrome print preview after waiting for the PDF plugin to load.
  void StartPrintAfterPluginIsLoaded();

  // Opens Chrome print preview without waiting.
  void StartPrintNow();

  // Used to send messages to ARC and request a new print document.
  mojo::Remote<mojom::PrintSessionInstance> instance_;

  // Receiver for PrintRenderer.
  mojo::AssociatedReceiver<printing::mojom::PrintRenderer>
      print_renderer_receiver_{this};

  // Used to bind the PrintSessionHost interface implementation to a message
  // pipe.
  mojo::Receiver<mojom::PrintSessionHost> session_receiver_;

  // Remote interface used to flatten a PDF (preview document).
  mojo::Remote<printing::mojom::PdfFlattener> pdf_flattener_;

  // In flight callbacks to |pdf_flattener_|, with their request IDs as the key.
  base::flat_map<int, CreatePreviewDocumentCallback> callbacks_;

  // Web contents for the ARC custom tab.
  std::unique_ptr<content::WebContents> web_contents_;

  // Observes the ARC window.
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      arc_window_observation_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<PrintSessionImpl> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_PRINT_SPOOLER_PRINT_SESSION_IMPL_H_
