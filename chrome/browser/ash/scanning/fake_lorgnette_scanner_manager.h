// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNING_FAKE_LORGNETTE_SCANNER_MANAGER_H_
#define CHROME_BROWSER_ASH_SCANNING_FAKE_LORGNETTE_SCANNER_MANAGER_H_

#include <string>
#include <vector>

#include "chrome/browser/ash/scanning/lorgnette_scanner_manager.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/ash/components/dbus/lorgnette_manager/lorgnette_manager_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// Fake implementation of LorgnetteScannerManager for tests.
class FakeLorgnetteScannerManager final : public LorgnetteScannerManager {
 public:
  FakeLorgnetteScannerManager();
  FakeLorgnetteScannerManager(const FakeLorgnetteScannerManager&) = delete;
  FakeLorgnetteScannerManager& operator=(const FakeLorgnetteScannerManager&) =
      delete;
  ~FakeLorgnetteScannerManager() override;

  // LorgnetteScannerManager:
  void GetScannerNames(GetScannerNamesCallback callback) override;
  void GetScannerCapabilities(const std::string& scanner_name,
                              GetScannerCapabilitiesCallback callback) override;
  bool IsRotateAlternate(const std::string& scanner_name,
                         const std::string& source_name) override;
  void Scan(const std::string& scanner_name,
            const lorgnette::ScanSettings& settings,
            ProgressCallback progress_callback,
            PageCallback page_callback,
            CompletionCallback completion_callback) override;
  void CancelScan(CancelCallback cancel_callback) override;

  // Sets the response returned by GetScannerNames().
  void SetGetScannerNamesResponse(
      const std::vector<std::string>& scanner_names);

  // Sets the response returned by GetScannerCapabilities().
  void SetGetScannerCapabilitiesResponse(
      const absl::optional<lorgnette::ScannerCapabilities>&
          scanner_capabilities);

  // Sets the response returned by Scan().
  void SetScanResponse(
      const absl::optional<std::vector<std::string>>& scan_data);

 private:
  std::vector<std::string> scanner_names_;
  absl::optional<lorgnette::ScannerCapabilities> scanner_capabilities_;
  absl::optional<std::vector<std::string>> scan_data_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCANNING_FAKE_LORGNETTE_SCANNER_MANAGER_H_
