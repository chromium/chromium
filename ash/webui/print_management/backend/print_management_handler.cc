// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/print_management/backend/print_management_handler.h"

#include <memory>
#include <utility>

#include "ash/webui/print_management/backend/print_management_delegate.h"
#include "base/check.h"

namespace ash::printing::printing_manager {

PrintManagementHandler::PrintManagementHandler(
    std::unique_ptr<PrintManagementDelegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK(delegate_);
}

PrintManagementHandler::~PrintManagementHandler() = default;

void PrintManagementHandler::LaunchPrinterSettings() {
  CHECK(delegate_);
  delegate_->LaunchPrinterSettings();
}

}  // namespace ash::printing::printing_manager
