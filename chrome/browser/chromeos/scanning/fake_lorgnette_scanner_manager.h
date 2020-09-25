// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SCANNING_FAKE_LORGNETTE_SCANNER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_SCANNING_FAKE_LORGNETTE_SCANNER_MANAGER_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "chrome/browser/chromeos/scanning/lorgnette_scanner_manager.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/dbus/lorgnette_manager_client.h"

namespace chromeos {

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
            PageCallback page_callback,
            ScanCallback callback) override;

  // Sets the response returned by GetScannerNames().
  void SetGetScannerNamesResponse(
      const std::vector<std::string>& scanner_names);

  // Sets the response returned by GetScannerCapabilities().
  void SetGetScannerCapabilitiesResponse(
      const base::Optional<lorgnette::ScannerCapabilities>&
          scanner_capabilities);

  // Sets the response returned by Scan().
  void SetScanResponse(const base::Optional<std::string>& scan_data);

 private:
  std::vector<std::string> scanner_names_;
  base::Optional<lorgnette::ScannerCapabilities> scanner_capabilities_;
  base::Optional<std::string> scan_data_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SCANNING_FAKE_LORGNETTE_SCANNER_MANAGER_H_
