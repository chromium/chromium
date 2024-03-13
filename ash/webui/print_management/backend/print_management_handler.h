// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PRINT_MANAGEMENT_BACKEND_PRINT_MANAGEMENT_HANDLER_H_
#define ASH_WEBUI_PRINT_MANAGEMENT_BACKEND_PRINT_MANAGEMENT_HANDLER_H_

#include <memory>

#include "chromeos/components/print_management/mojom/printing_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::printing::printing_manager {

class PrintManagementDelegate;

class PrintManagementHandler : public chromeos::printing::printing_manager::
                                   mojom::PrintManagementHandler {
 public:
  explicit PrintManagementHandler(
      std::unique_ptr<PrintManagementDelegate> delegate);
  PrintManagementHandler(const PrintManagementHandler&) = delete;
  PrintManagementHandler& operator=(const PrintManagementHandler&) = delete;
  ~PrintManagementHandler() override;

  // PrintManagementHandler:
  void LaunchPrinterSettings(
      chromeos::printing::printing_manager::mojom::LaunchSource source)
      override;
  void RecordGetPrintJobsRequestDuration(uint32_t duration) override;

  void BindInterface(
      mojo::PendingReceiver<
          chromeos::printing::printing_manager::mojom::PrintManagementHandler>
          receiver);

 private:
  mojo::Receiver<
      chromeos::printing::printing_manager::mojom::PrintManagementHandler>
      receiver_{this};
  // Used to call browser functions from ash.
  std::unique_ptr<PrintManagementDelegate> delegate_;
};

}  // namespace ash::printing::printing_manager

#endif  // ASH_WEBUI_PRINT_MANAGEMENT_BACKEND_PRINT_MANAGEMENT_HANDLER_H_
