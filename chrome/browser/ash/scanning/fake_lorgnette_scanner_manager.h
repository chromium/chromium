// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNING_FAKE_LORGNETTE_SCANNER_MANAGER_H_
#define CHROME_BROWSER_ASH_SCANNING_FAKE_LORGNETTE_SCANNER_MANAGER_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/dbus/lorgnette_manager/lorgnette_manager_client.h"

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
      const base::Optional<lorgnette::ScannerCapabilities>&
          scanner_capabilities);

  // Sets the response returned by Scan().
  void SetScanResponse(
      const base::Optional<std::vector<std::string>>& scan_data);

 private:
  std::vector<std::string> scanner_names_;
  base::Optional<lorgnette::ScannerCapabilities> scanner_capabilities_;
  base::Optional<std::vector<std::string>> scan_data_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCANNING_FAKE_LORGNETTE_SCANNER_MANAGER_H_
