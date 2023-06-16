// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PRINT_MANAGEMENT_BACKEND_PRINT_MANAGEMENT_HANDLER_H_
#define ASH_WEBUI_PRINT_MANAGEMENT_BACKEND_PRINT_MANAGEMENT_HANDLER_H_

#include <memory>

namespace ash::printing::printing_manager {

class PrintManagementDelegate;

class PrintManagementHandler {
 public:
  explicit PrintManagementHandler(
      std::unique_ptr<PrintManagementDelegate> delegate);
  PrintManagementHandler(const PrintManagementHandler&) = delete;
  PrintManagementHandler& operator=(const PrintManagementHandler&) = delete;
  ~PrintManagementHandler();

  void LaunchPrinterSettings();

 private:
  // Used to call browser functions from ash.
  std::unique_ptr<PrintManagementDelegate> delegate_;
};

}  // namespace ash::printing::printing_manager

#endif  // ASH_WEBUI_PRINT_MANAGEMENT_BACKEND_PRINT_MANAGEMENT_HANDLER_H_
