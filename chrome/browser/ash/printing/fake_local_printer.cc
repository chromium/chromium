// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/fake_local_printer.h"

#include "base/notreached.h"

namespace ash {

FakeLocalPrinter::FakeLocalPrinter() = default;

FakeLocalPrinter::~FakeLocalPrinter() = default;

void FakeLocalPrinter::GetPrinters(const AccountId& accountId,
                                   GetPrintersCallback callback) {
  NOTREACHED();
}

std::optional<chromeos::Printer> FakeLocalPrinter::GetPrinter(
    const AccountId& accountId,
    const std::string& printer_id) {
  NOTREACHED();
}

void FakeLocalPrinter::GetCapability(const AccountId& accountId,
                                     const std::string& printer_id,
                                     GetCapabilityCallback callback) {
  NOTREACHED();
}

void FakeLocalPrinter::GetStatus(const AccountId& accountId,
                                 const std::string& printer_id,
                                 GetStatusCallback callback) {
  NOTREACHED();
}

void FakeLocalPrinter::GetEulaUrl(const AccountId& accountId,
                                  const std::string& printer_id,
                                  GetEulaUrlCallback callback) {
  NOTREACHED();
}

void FakeLocalPrinter::GetOAuthAccessToken(
    const AccountId& accountId,
    const std::string& printer_id,
    GetOAuthAccessTokenCallback callback) {
  NOTREACHED();
}

}  // namespace ash
