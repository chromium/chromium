// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_WEBCONTENTS_ADAPTER_ASH_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_WEBCONTENTS_ADAPTER_ASH_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/printing/print_preview/print_preview_dialog_controller_cros.h"
#include "chrome/browser/chromeos/printing/print_preview/print_preview_cros_client.h"
#include "chrome/browser/chromeos/printing/print_preview/print_preview_cros_delegate.h"
#include "components/printing/common/print.mojom-forward.h"

namespace ash::printing {

// This class is the adapter to facilitate calls from browser to chromeos
// print system.
class PrintPreviewWebcontentsAdapterAsh
    : public chromeos::PrintPreviewCrosDelegate,
      public PrintPreviewDialogControllerCros::DialogControllerObserver {
 public:
  PrintPreviewWebcontentsAdapterAsh();
  PrintPreviewWebcontentsAdapterAsh(const PrintPreviewWebcontentsAdapterAsh&) =
      delete;
  PrintPreviewWebcontentsAdapterAsh& operator=(
      const PrintPreviewWebcontentsAdapterAsh&) = delete;
  ~PrintPreviewWebcontentsAdapterAsh() override;

  // chromeos::PrintPreviewCrosDelegate
  void RequestPrintPreview(
      const base::UnguessableToken& token,
      ::printing::mojom::RequestPrintPreviewParamsPtr params,
      RequestPrintPreviewCallback callback) override;
  // The initiator source is no longer available, close the print dialog.
  void PrintPreviewDone(const base::UnguessableToken& token,
                        PrintPreviewDoneCallback callback) override;

  // PrintPreviewDialogControllerCros::DialogControllerObserver:
  void OnDialogClosed(const base::UnguessableToken& token) override;

  void OnDialogClosedCallback(bool success);

  // Register client to receive print preview events.
  void RegisterAshClient(chromeos::PrintPreviewCrosClient* client);

 private:
  std::unique_ptr<PrintPreviewDialogControllerCros> dialog_controller_;
  raw_ptr<chromeos::PrintPreviewCrosClient> ash_client_{nullptr};
  base::WeakPtrFactory<PrintPreviewWebcontentsAdapterAsh> weak_ptr_factory_{
      this};
};

}  // namespace ash::printing

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_WEBCONTENTS_ADAPTER_ASH_H_
