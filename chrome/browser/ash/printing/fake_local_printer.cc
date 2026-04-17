// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/fake_local_printer.h"

#include "base/check.h"
#include "base/notreached.h"

namespace ash {

FakeLocalPrinter::FakeLocalPrinter() = default;

FakeLocalPrinter::~FakeLocalPrinter() = default;

void FakeLocalPrinter::GetPrinters(const AccountId& accountId,
                                   GetPrintersCallback callback) {
  get_printers_call_count_++;
  std::move(callback).Run(printers_);
}

std::optional<chromeos::Printer> FakeLocalPrinter::GetPrinterInternal(
    const std::string& printer_id) const {
  for (const auto& printer : printers_) {
    if (printer.id() == printer_id) {
      return printer;
    }
  }
  return std::nullopt;
}

std::optional<chromeos::Printer> FakeLocalPrinter::GetPrinter(
    const AccountId& accountId,
    const std::string& printer_id) {
  get_printer_call_count_++;
  return GetPrinterInternal(printer_id);
}

void FakeLocalPrinter::GetCapability(const AccountId& accountId,
                                     const std::string& printer_id,
                                     GetCapabilityCallback callback) {
  get_capability_call_count_++;
  auto it = capabilities_.find(printer_id);
  if (it == capabilities_.end()) {
    std::move(callback).Run(std::nullopt, std::nullopt);
    return;
  }
  std::optional<chromeos::Printer> printer = GetPrinterInternal(printer_id);
  std::move(callback).Run(
      printer ? base::optional_ref<const chromeos::Printer>(*printer)
              : std::nullopt,
      it->second);
}

void FakeLocalPrinter::GetStatus(const AccountId& accountId,
                                 const std::string& printer_id,
                                 GetStatusCallback callback) {
  get_status_call_count_++;
  auto it = statuses_.find(printer_id);
  if (it == statuses_.end()) {
    std::move(callback).Run(chromeos::CupsPrinterStatus());
    return;
  }
  std::move(callback).Run(it->second);
}

void FakeLocalPrinter::GetEulaUrl(const AccountId& accountId,
                                  const std::string& printer_id,
                                  GetEulaUrlCallback callback) {
  get_eula_url_call_count_++;
  auto it = eula_urls_.find(printer_id);
  if (it == eula_urls_.end()) {
    std::move(callback).Run(GURL());
    return;
  }
  std::move(callback).Run(it->second);
}

void FakeLocalPrinter::GetOAuthAccessToken(
    const AccountId& accountId,
    const std::string& printer_id,
    GetOAuthAccessTokenCallback callback) {
  get_oauth_token_call_count_++;
  auto it = oauth_tokens_.find(printer_id);
  if (it == oauth_tokens_.end()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(it->second);
}

void FakeLocalPrinter::AddPrinter(chromeos::Printer printer) {
  CHECK(!GetPrinterInternal(printer.id()).has_value());
  printers_.push_back(std::move(printer));
}

void FakeLocalPrinter::ClearPrinters() {
  printers_.clear();
  capabilities_.clear();
  statuses_.clear();
  eula_urls_.clear();
  oauth_tokens_.clear();
}

void FakeLocalPrinter::SetCapability(
    const std::string& printer_id,
    std::optional<::printing::PrinterSemanticCapsAndDefaults> caps) {
  CHECK(GetPrinterInternal(printer_id).has_value());
  capabilities_[printer_id] = std::move(caps);
}

void FakeLocalPrinter::SetStatus(const std::string& printer_id,
                                 chromeos::CupsPrinterStatus status) {
  CHECK(GetPrinterInternal(printer_id).has_value());
  statuses_[printer_id] = std::move(status);
}

void FakeLocalPrinter::SetEulaUrl(const std::string& printer_id,
                                  GURL eula_url) {
  CHECK(GetPrinterInternal(printer_id).has_value());
  eula_urls_[printer_id] = std::move(eula_url);
}

void FakeLocalPrinter::SetOAuthAccessToken(const std::string& printer_id,
                                           std::optional<std::string> token) {
  CHECK(GetPrinterInternal(printer_id).has_value());
  oauth_tokens_[printer_id] = std::move(token);
}

}  // namespace ash
