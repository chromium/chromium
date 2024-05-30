// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_WEBCONTENTS_ADAPTER_ASH_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_WEBCONTENTS_ADAPTER_ASH_H_

#include <memory>

#include "ash/public/cpp/print_preview_delegate.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/printing/print_preview/print_preview_dialog_controller_cros.h"
#include "chromeos/crosapi/mojom/print_preview_cros.mojom.h"
#include "components/printing/common/print.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::printing {

// Implements the PrintPreviewDelegate to handle calls from the browser.
// This class is also the adapter to facilitate calls from ash to chrome
// browser (via PrintPreviewCrosClient). It uses crosapi to handle cross process
// communication.
// There are two possible communication pipelines, lacros via mojom and
// ash-chrome via delegate.
class PrintPreviewWebcontentsAdapterAsh
    : public PrintPreviewDelegate,
      public crosapi::mojom::PrintPreviewCrosDelegate,
      public PrintPreviewDialogControllerCros::DialogControllerObserver {
 public:
  PrintPreviewWebcontentsAdapterAsh();
  PrintPreviewWebcontentsAdapterAsh(const PrintPreviewWebcontentsAdapterAsh&) =
      delete;
  PrintPreviewWebcontentsAdapterAsh& operator=(
      const PrintPreviewWebcontentsAdapterAsh&) = delete;
  ~PrintPreviewWebcontentsAdapterAsh() override;

  // Binds a pending receiver connected to a lacros mojo client to the delegate.
  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::PrintPreviewCrosDelegate> receiver);

  // crosapi::mojom::PrintPreviewCrosDelegate
  void RegisterMojoClient(
      mojo::PendingRemote<crosapi::mojom::PrintPreviewCrosClient> client,
      RegisterMojoClientCallback callback) override;
  void RequestPrintPreview(
      const base::UnguessableToken& token,
      ::printing::mojom::RequestPrintPreviewParamsPtr params,
      RequestPrintPreviewCallback callback) override;
  // The initiator source is no longer available, close the print dialog.
  void PrintPreviewDone(const base::UnguessableToken& token,
                        PrintPreviewDoneCallback callback) override;

  // PrintPreviewDelegate::
  void StartGetPreview(const base::UnguessableToken& token,
                       crosapi::mojom::PrintSettingsPtr settings,
                       base::OnceCallback<void(bool)> callback) override;

  // PrintPreviewDialogControllerCros::DialogControllerObserver:
  void OnDialogClosed(const base::UnguessableToken& token) override;

  void OnDialogClosedCallback(bool success);

  // Ash-chrome clients do not require a mojom endpoint, instead can directly
  // access the client.
  void RegisterAshClient(crosapi::mojom::PrintPreviewCrosClient* client);

 private:
  std::unique_ptr<PrintPreviewDialogControllerCros> dialog_controller_;
  mojo::Remote<crosapi::mojom::PrintPreviewCrosClient> mojo_client_;
  mojo::Receiver<crosapi::mojom::PrintPreviewCrosDelegate> receiver_{this};
  raw_ptr<crosapi::mojom::PrintPreviewCrosClient> ash_client_{nullptr};
  base::WeakPtrFactory<PrintPreviewWebcontentsAdapterAsh> weak_ptr_factory_{
      this};
};

}  // namespace ash::printing

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_WEBCONTENTS_ADAPTER_ASH_H_
