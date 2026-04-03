// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_FAKE_LOCAL_PRINTER_H_
#define CHROME_BROWSER_ASH_PRINTING_FAKE_LOCAL_PRINTER_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ash/printing/local_printer.h"

namespace ash {

class FakeLocalPrinter : public LocalPrinter {
 public:
  FakeLocalPrinter();
  FakeLocalPrinter(const FakeLocalPrinter&) = delete;
  FakeLocalPrinter& operator=(const FakeLocalPrinter&) = delete;
  ~FakeLocalPrinter() override;

  // LocalPrinter overrides:
  // FakeLocalPrinter overrides all LocalPrinter methods as NOTREACHED(), which
  // will just crash by default if called. Each test can inherit this
  // FakeLocalPrinter and override the minimal methods it wants to mock.
  // TODO(crbug.com/479070409): provide fake implementations for this class
  // across multiple unittests.
  void GetPrinters(const AccountId& accountId,
                   GetPrintersCallback callback) override;
  std::optional<chromeos::Printer> GetPrinter(
      const AccountId& accountId,
      const std::string& printer_id) override;
  void GetCapability(const AccountId& accountId,
                     const std::string& printer_id,
                     GetCapabilityCallback callback) override;
  void GetStatus(const AccountId& accountId,
                 const std::string& printer_id,
                 GetStatusCallback callback) override;
  void GetEulaUrl(const AccountId& accountId,
                  const std::string& printer_id,
                  GetEulaUrlCallback callback) override;
  void GetOAuthAccessToken(const AccountId& accountId,
                           const std::string& printer_id,
                           GetOAuthAccessTokenCallback callback) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_FAKE_LOCAL_PRINTER_H_
