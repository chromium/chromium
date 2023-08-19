// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINT_MANAGEMENT_PRINT_MANAGEMENT_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINT_MANAGEMENT_PRINT_MANAGEMENT_DELEGATE_IMPL_H_

#include "ash/webui/print_management/backend/print_management_delegate.h"

namespace ash::print_management {

class PrintManagementDelegateImpl
    : public ash::printing::printing_manager::PrintManagementDelegate {
 public:
  PrintManagementDelegateImpl() = default;
  PrintManagementDelegateImpl(const PrintManagementDelegateImpl&) = delete;
  PrintManagementDelegateImpl& operator=(const PrintManagementDelegateImpl&) =
      delete;
  ~PrintManagementDelegateImpl() override = default;

  // PrintManagementDelegate:
  void LaunchPrinterSettings() override;
};

}  // namespace ash::print_management

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINT_MANAGEMENT_PRINT_MANAGEMENT_DELEGATE_IMPL_H_
