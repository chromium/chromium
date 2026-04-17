// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_FAKE_LOCAL_PRINTER_H_
#define CHROME_BROWSER_ASH_PRINTING_FAKE_LOCAL_PRINTER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/ash/printing/local_printer.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "url/gurl.h"

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

  // Setup methods for testing.

  // Adds a printer to the fake's internal list. This printer will be returned
  // by GetPrinters and GetPrinter.
  void AddPrinter(chromeos::Printer printer);

  // Clears all added printers and their properties.
  void ClearPrinters();

  // Sets the capabilities for a specific printer. These will be returned by
  // GetCapability. Requires the printer to already exist.
  void SetCapability(
      const std::string& printer_id,
      std::optional<::printing::PrinterSemanticCapsAndDefaults> caps);

  // Sets the status for a specific printer. This will be returned by GetStatus.
  // Requires the printer to already exist.
  void SetStatus(const std::string& printer_id,
                 chromeos::CupsPrinterStatus status);

  // Sets the EULA URL for a specific printer. This will be returned by
  // GetEulaUrl. Requires the printer to already exist.
  void SetEulaUrl(const std::string& printer_id, GURL eula_url);

  // Sets the OAuth access token for a specific printer. This will be returned
  // by GetOAuthAccessToken. Requires the printer to already exist.
  void SetOAuthAccessToken(const std::string& printer_id,
                           std::optional<std::string> token);

  // Call counters for asserting behavior in tests.
  int get_printers_call_count() const { return get_printers_call_count_; }
  int get_printer_call_count() const { return get_printer_call_count_; }
  int get_capability_call_count() const { return get_capability_call_count_; }
  int get_status_call_count() const { return get_status_call_count_; }
  int get_eula_url_call_count() const { return get_eula_url_call_count_; }
  int get_oauth_token_call_count() const { return get_oauth_token_call_count_; }

 private:
  std::optional<chromeos::Printer> GetPrinterInternal(
      const std::string& printer_id) const;

  int get_printers_call_count_ = 0;
  int get_printer_call_count_ = 0;
  int get_capability_call_count_ = 0;
  int get_status_call_count_ = 0;
  int get_eula_url_call_count_ = 0;
  int get_oauth_token_call_count_ = 0;
  std::vector<chromeos::Printer> printers_;
  absl::flat_hash_map<std::string,
                      std::optional<::printing::PrinterSemanticCapsAndDefaults>>
      capabilities_;
  absl::flat_hash_map<std::string, chromeos::CupsPrinterStatus> statuses_;
  absl::flat_hash_map<std::string, GURL> eula_urls_;
  absl::flat_hash_map<std::string, std::optional<std::string>> oauth_tokens_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_FAKE_LOCAL_PRINTER_H_
