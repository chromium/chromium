// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNING_FAKE_LORGNETTE_SCANNER_MANAGER_H_
#define CHROME_BROWSER_ASH_SCANNING_FAKE_LORGNETTE_SCANNER_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/ash/scanning/lorgnette_scanner_manager.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/ash/components/dbus/lorgnette_manager/lorgnette_manager_client.h"

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
  void GetScannerInfoList(const std::string& client_id,
                          LocalScannerFilter local_only,
                          SecureScannerFilter secure_only,
                          GetScannerInfoListCallback callback) override;
  void GetScannerCapabilities(const std::string& scanner_name,
                              GetScannerCapabilitiesCallback callback) override;
  void OpenScanner(const lorgnette::OpenScannerRequest& request,
                   OpenScannerCallback callback) override;
  void CloseScanner(const lorgnette::CloseScannerRequest& request,
                    CloseScannerCallback callback) override;
  void SetOptions(const lorgnette::SetOptionsRequest& request,
                  SetOptionsCallback callback) override;
  void GetCurrentConfig(const lorgnette::GetCurrentConfigRequest& request,
                        GetCurrentConfigCallback callback) override;
  void StartPreparedScan(const lorgnette::StartPreparedScanRequest& request,
                         StartPreparedScanCallback callback) override;
  void ReadScanData(const lorgnette::ReadScanDataRequest& request,
                    ReadScanDataCallback callback) override;
  bool IsRotateAlternate(const std::string& scanner_name,
                         const std::string& source_name) override;
  void Scan(const std::string& scanner_name,
            const lorgnette::ScanSettings& settings,
            ProgressCallback progress_callback,
            PageCallback page_callback,
            CompletionCallback completion_callback) override;
  void CancelScan(CancelCallback cancel_callback) override;
  void CancelScan(const lorgnette::CancelScanRequest& request,
                  CancelScanCallback callback) override;

  // Sets the response returned by GetScannerNames().
  void SetGetScannerNamesResponse(
      const std::vector<std::string>& scanner_names);

  // Sets the response returned by GetScannerInfoList().
  void SetGetScannerInfoListResponse(
      const std::optional<lorgnette::ListScannersResponse>& response);

  // Sets the response returned by GetScannerCapabilities().
  void SetGetScannerCapabilitiesResponse(
      const std::optional<lorgnette::ScannerCapabilities>&
          scanner_capabilities);

  // Sets the response returned by OpenScanner().
  void SetOpenScannerResponse(
      const std::optional<lorgnette::OpenScannerResponse>& response);

  // Sets the response returned by CloseScanner().
  void SetCloseScannerResponse(
      const std::optional<lorgnette::CloseScannerResponse>& response);

  // Sets the response returned by SetOptions().
  void SetSetOptionsResponse(
      const std::optional<lorgnette::SetOptionsResponse>& response);

  // Sets the response returned by GetCurrentConfig().
  void SetGetCurrentConfigResponse(
      const std::optional<lorgnette::GetCurrentConfigResponse>& response);

  // Sets the response returned by StartPreparedScan().
  void SetStartPreparedScanResponse(
      const std::optional<lorgnette::StartPreparedScanResponse>& response);

  // Sets the response returned by ReadScanData().
  void SetReadScanDataResponse(
      const std::optional<lorgnette::ReadScanDataResponse>& response);

  // Sets the response returned by Scan().
  void SetScanResponse(
      const std::optional<std::vector<std::string>>& scan_data);

  // Sets the response returned by CancelScan().
  void SetCancelScanResponse(
      const std::optional<lorgnette::CancelScanResponse>& response);

  // Optionally sets `scan_data` if a matching set of scan settings is found.
  void MaybeSetScanDataBasedOnSettings(const lorgnette::ScanSettings& settings);

 private:
  std::vector<std::string> scanner_names_;
  std::optional<lorgnette::ListScannersResponse> list_scanners_response_;
  std::optional<lorgnette::ScannerCapabilities> scanner_capabilities_;
  std::optional<lorgnette::OpenScannerResponse> open_scanner_response_;
  std::optional<lorgnette::CloseScannerResponse> close_scanner_response_;
  std::optional<lorgnette::SetOptionsResponse> set_options_response_;
  std::optional<lorgnette::GetCurrentConfigResponse>
      get_current_config_response_;
  std::optional<lorgnette::StartPreparedScanResponse>
      start_prepared_scan_response_;
  std::optional<lorgnette::ReadScanDataResponse> read_scan_data_response_;
  std::optional<lorgnette::CancelScanResponse> cancel_scan_response_;
  std::optional<std::vector<std::string>> scan_data_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCANNING_FAKE_LORGNETTE_SCANNER_MANAGER_H_
