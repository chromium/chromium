// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing/printer_capabilities_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chromeos/printing/printer_configuration.h"
#include "printing/backend/print_backend.h"

namespace extensions {

namespace {

constexpr int kMaxPrintersCount = 20;

absl::optional<printing::PrinterSemanticCapsAndDefaults>
FetchCapabilitiesOnBlockingTaskRunner(const std::string& printer_id) {
  scoped_refptr<printing::PrintBackend> backend(
      printing::PrintBackend::CreateInstance(
          g_browser_process->GetApplicationLocale()));
  printing::PrinterSemanticCapsAndDefaults capabilities;
  if (backend->GetPrinterSemanticCapsAndDefaults(printer_id, &capabilities) !=
      printing::mojom::ResultCode::kSuccess) {
    LOG(WARNING) << "Failed to get capabilities for " << printer_id;
    return absl::nullopt;
  }
  return capabilities;
}

}  // namespace

PrinterCapabilitiesProvider::PrinterCapabilitiesProvider(
    chromeos::CupsPrintersManager* printers_manager,
    std::unique_ptr<chromeos::PrinterConfigurer> printer_configurer)
    : printers_manager_(printers_manager),
      printer_configurer_(std::move(printer_configurer)),
      printer_capabilities_cache_(kMaxPrintersCount) {}

PrinterCapabilitiesProvider::~PrinterCapabilitiesProvider() = default;

void PrinterCapabilitiesProvider::GetPrinterCapabilities(
    const std::string& printer_id,
    GetPrinterCapabilitiesCallback callback) {
  absl::optional<chromeos::Printer> printer =
      printers_manager_->GetPrinter(printer_id);
  if (!printer) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  if (!printers_manager_->IsPrinterInstalled(*printer)) {
    printer_configurer_->SetUpPrinter(
        *printer,
        base::BindOnce(&PrinterCapabilitiesProvider::OnPrinterInstalled,
                       weak_ptr_factory_.GetWeakPtr(), *printer,
                       std::move(callback)));
    return;
  }

  auto capabilities = printer_capabilities_cache_.Get(printer->id());
  if (capabilities == printer_capabilities_cache_.end()) {
    FetchCapabilities(printer->id(), std::move(callback));
    return;
  }
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), capabilities->second));
}

void PrinterCapabilitiesProvider::OnPrinterInstalled(
    const chromeos::Printer& printer,
    GetPrinterCapabilitiesCallback callback,
    chromeos::PrinterSetupResult result) {
  if (result != chromeos::kSuccess) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }
  printers_manager_->PrinterInstalled(printer, /*is_automatic=*/true);
  FetchCapabilities(printer.id(), std::move(callback));
}

void PrinterCapabilitiesProvider::FetchCapabilities(
    const std::string& printer_id,
    GetPrinterCapabilitiesCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&FetchCapabilitiesOnBlockingTaskRunner, printer_id),
      base::BindOnce(&PrinterCapabilitiesProvider::OnCapabilitiesFetched,
                     weak_ptr_factory_.GetWeakPtr(), printer_id,
                     std::move(callback)));
}

void PrinterCapabilitiesProvider::OnCapabilitiesFetched(
    const std::string& printer_id,
    GetPrinterCapabilitiesCallback callback,
    absl::optional<printing::PrinterSemanticCapsAndDefaults> capabilities) {
  if (capabilities.has_value())
    printer_capabilities_cache_.Put(printer_id, capabilities.value());
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(capabilities)));
}

}  // namespace extensions
