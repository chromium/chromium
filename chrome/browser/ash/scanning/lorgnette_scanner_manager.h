// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNING_LORGNETTE_SCANNER_MANAGER_H_
#define CHROME_BROWSER_ASH_SCANNING_LORGNETTE_SCANNER_MANAGER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/ash/components/dbus/lorgnette_manager/lorgnette_manager_client.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace ash {

class ZeroconfScannerDetector;

// Top-level manager of available scanners in Chrome OS. All functions in this
// class must be called from a sequenced context.
class LorgnetteScannerManager : public KeyedService {
 public:
  using GetScannerNamesCallback =
      base::OnceCallback<void(std::vector<std::string> scanner_names)>;
  using GetScannerInfoListCallback = base::OnceCallback<void(
      const std::optional<lorgnette::ListScannersResponse>& response)>;
  using GetScannerCapabilitiesCallback = base::OnceCallback<void(
      const std::optional<lorgnette::ScannerCapabilities>& capabilities)>;
  using OpenScannerCallback = base::OnceCallback<void(
      const std::optional<lorgnette::OpenScannerResponse>& response)>;
  using CloseScannerCallback = base::OnceCallback<void(
      const std::optional<lorgnette::CloseScannerResponse>& response)>;
  using SetOptionsCallback = base::OnceCallback<void(
      const std::optional<lorgnette::SetOptionsResponse>& response)>;
  using GetCurrentConfigCallback = base::OnceCallback<void(
      const std::optional<lorgnette::GetCurrentConfigResponse>& response)>;
  using StartPreparedScanCallback = base::OnceCallback<void(
      const std::optional<lorgnette::StartPreparedScanResponse>& response)>;
  using ReadScanDataCallback = base::OnceCallback<void(
      const std::optional<lorgnette::ReadScanDataResponse>& response)>;
  using ProgressCallback =
      base::RepeatingCallback<void(uint32_t progress_percent,
                                   uint32_t page_number)>;
  using PageCallback = base::RepeatingCallback<void(std::string scan_data,
                                                    uint32_t page_number)>;
  using CompletionCallback =
      base::OnceCallback<void(lorgnette::ScanFailureMode failure_mode)>;
  using CancelCallback = base::OnceCallback<void(bool success)>;
  using CancelScanCallback = base::OnceCallback<void(
      const std::optional<lorgnette::CancelScanResponse>& response)>;

  enum class LocalScannerFilter {
    kLocalScannersOnly = 0,
    kIncludeNetworkScanners
  };

  enum class SecureScannerFilter {
    kSecureScannersOnly = 0,
    kIncludeUnsecureScanners
  };

  ~LorgnetteScannerManager() override = default;

  static std::unique_ptr<LorgnetteScannerManager> Create(
      std::unique_ptr<ZeroconfScannerDetector> zeroconf_scanner_detector,
      Profile* profile);

  // Returns the names of all available, deduplicated scanners.
  virtual void GetScannerNames(GetScannerNamesCallback callback) = 0;

  // Returns ScannerInfo objects for all of the available lorgnette scanners and
  // zeroconf scanners, filtered by |local_only| and |secure_only|.
  virtual void GetScannerInfoList(const std::string& client_id,
                                  LocalScannerFilter local_only,
                                  SecureScannerFilter secure_only,
                                  GetScannerInfoListCallback callback) = 0;

  // Returns the capabilities of the scanner specified by |scanner_name|. If
  // |scanner_name| does not correspond to a known scanner, std::nullopt is
  // returned in the callback.
  virtual void GetScannerCapabilities(
      const std::string& scanner_name,
      GetScannerCapabilitiesCallback callback) = 0;

  // Opens the scanner described by |request|.  If an error occurs,
  // std::nullopt is returned in the callback.
  virtual void OpenScanner(const lorgnette::OpenScannerRequest& request,
                           OpenScannerCallback callback) = 0;

  // Closes the scanner described by |request|.  If an error occurs,
  // std::nullopt is returned in the callback.
  virtual void CloseScanner(const lorgnette::CloseScannerRequest& request,
                            CloseScannerCallback callback) = 0;

  // Sets the options described by |request|.  If an error occurs, std::nullopt
  // is returned in the callback.
  virtual void SetOptions(const lorgnette::SetOptionsRequest& request,
                          SetOptionsCallback callback) = 0;

  // Gets the config for the scanner described by |request|.  If an error
  // occurs, std::nullopt is returned in the callback.
  virtual void GetCurrentConfig(
      const lorgnette::GetCurrentConfigRequest& request,
      GetCurrentConfigCallback callback) = 0;

  // Starts a scan using information in |request| and returns the result using
  // the provided |callback|.  If an error occurs, std::nullopt is returned in
  // the callback.
  virtual void StartPreparedScan(
      const lorgnette::StartPreparedScanRequest& request,
      StartPreparedScanCallback callback) = 0;

  // Reads the scan data described by |request|.  If an error occurs,
  // std::nullopt is returned in the callback.
  virtual void ReadScanData(const lorgnette::ReadScanDataRequest& request,
                            ReadScanDataCallback callback) = 0;

  // Returns whether or not an ADF scanner that flips alternate pages was
  // selected based on |scanner_name| and |source_name|.
  virtual bool IsRotateAlternate(const std::string& scanner_name,
                                 const std::string& source_name) = 0;

  // Performs a scan with the scanner specified by |scanner_name| using the
  // given |scan_properties|. As each page is scanned, |progress_callback| is
  // called with the current progress percent from 0 to 100. As each scanned
  // page is completed, |page_callback| is called with the image data for that
  // page. If |scanner_name| does not correspond to a known scanner, false is
  // returned in |completion_callback|. After the scan has completed,
  // |completion_callback| will be called with argument success=true.
  virtual void Scan(const std::string& scanner_name,
                    const lorgnette::ScanSettings& settings,
                    ProgressCallback progress_callback,
                    PageCallback page_callback,
                    CompletionCallback completion_callback) = 0;

  // Request to cancel the currently running scan job. This function makes the
  // assumption that LorgnetteManagerClient only has one scan running at a time.
  virtual void CancelScan(CancelCallback cancel_callback) = 0;

  // Request to cancel the scan specified by the JobHandle in |request| and
  // return the result using the provided |callback|.  If an error occurs,
  // std::nullopt is returned in the callback.
  virtual void CancelScan(const lorgnette::CancelScanRequest& request,
                          CancelScanCallback callback) = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCANNING_LORGNETTE_SCANNER_MANAGER_H_
