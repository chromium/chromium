// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/print_management/backend/print_management_handler.h"

#include <memory>
#include <utility>

#include "ash/webui/print_management/backend/print_management_delegate.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/components/print_management/mojom/printing_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash::printing::printing_manager {

namespace {

constexpr char kRecordRequestDurationMetric[] =
    "Printing.PrintManagement.GetPrintJobsRequestDuration";

constexpr char kRecordUserActionMetric[] =
    "ChromeOS.PrintManagement.PrinterSettingsLaunchSource";

}  // namespace

PrintManagementHandler::PrintManagementHandler(
    std::unique_ptr<PrintManagementDelegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK(delegate_);
}

PrintManagementHandler::~PrintManagementHandler() = default;

void PrintManagementHandler::LaunchPrinterSettings(
    chromeos::printing::printing_manager::mojom::LaunchSource source) {
  CHECK(delegate_);
  delegate_->LaunchPrinterSettings();

  // Record launch triggered by pressing button in header or empty state.
  base::UmaHistogramEnumeration(kRecordUserActionMetric, source);
}

void PrintManagementHandler::RecordGetPrintJobsRequestDuration(
    uint32_t duration) {
  base::UmaHistogramCounts10000(kRecordRequestDurationMetric, duration);
}

void PrintManagementHandler::BindInterface(
    mojo::PendingReceiver<
        chromeos::printing::printing_manager::mojom::PrintManagementHandler>
        receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

}  // namespace ash::printing::printing_manager
