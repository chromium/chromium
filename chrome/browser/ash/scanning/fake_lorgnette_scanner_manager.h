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
//
// It keeps track of cancelled scan jobs, which affects the behavior of
// ReadScanData (fails with OPERATION_RESULT_CANCELLED for cancelled jobs) and
// CancelScan (fails with OPERATION_RESULT_UNKNOWN for already cancelled jobs).
// Other than that, tests are free to configure the various operations's
// responses.
//
// TODO(crbug.com/479031241): Revisit the design (setters vs fake behavior) once
// the document service has been fully migrated away from crosapi.
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

  // Flips a flag to simulate D-Bus failure for OpenScanner and SetOptions, i.e.
  // make them pass std::nullopt to their response callbacks.
  // TODO(crbug.com/479031241): Make this affect all relevant operations.
  void SimulateDBusFailure(bool simulate);

  // Registers a scanner with a template configuration. A subsequent OpenScanner
  // request with the scanner id from `scanner_info` will succeed and return a
  // copy of `config_template` with a unique session handle populated in the
  // scanner token, unless overridden via SimulateDBusFailure.
  // TODO(crbug.com/479031241): Update this and other comments once the class
  // rewrite is complete.
  void AddScanner(lorgnette::ScannerInfo scanner_info,
                  lorgnette::ScannerConfig config_template,
                  std::optional<lorgnette::ScannerCapabilities> capabilities =
                      std::nullopt);

  // Configures the response returned by GetCurrentConfig().
  // If `result` has no value, the response will be nullopt (that's the
  // default). Otherwise, the response will consist of the given values and the
  // scanner from the request.
  void ConfigureGetCurrentConfigResponse(
      std::optional<lorgnette::OperationResult> result,
      std::optional<lorgnette::ScannerConfig> config);

  // Sets the result field of the response returned by StartPreparedScan().
  // - If `result` has no value, the response will be nullopt (that's the
  //   default).
  // - Otherwise, the response will consist of the given result, the scanner
  //   from the request, and, in the case of OPERATION_RESULT_SUCCESS, a fresh
  //   job handle. Exception: if the requested max read size is below the
  //   minimum of 32k, the result will be OPERATION_RESULT_INVALID.
  void SetStartPreparedScanResult(
      std::optional<lorgnette::OperationResult> result);

  // Configures the response returned by ReadScanData().
  // - If `result` has no value, the response will be nullopt (that's the
  //   default).
  // - Otherwise, each response will contain a chunk of `chunks` with
  //   OPERATION_RESULT_SUCCESS, and the given `result` is used for the final
  //   response after all chunks have been returned.
  //   Note: After cancelling a job, ReadScanData will always respond with
  //   OPERATION_RESULT_CANCELLED for that job.
  void ConfigureReadScanDataResponse(
      std::optional<lorgnette::OperationResult> result,
      std::vector<std::string> data_chunks = {});

  // Sets the response returned by Scan().
  void SetScanResponse(
      const std::optional<std::vector<std::string>>& scan_data);

  // Sets the result field of the response returned by the two-parameter version
  // of CancelScan(). If this is std::nullopt, the callback is passed
  // std::nullopt. The default is OPERATION_RESULT_ADF_JAMMED.
  // Note: This does not apply to cancelled jobs, see the class documentation.
  void SetCancelScanResult(std::optional<lorgnette::OperationResult> result);

  // Optionally sets `scan_data` if a matching set of scan settings is found.
  void MaybeSetScanDataBasedOnSettings(const lorgnette::ScanSettings& settings);

 private:
  struct ScannerSession {
    ScannerSession();
    ScannerSession(ScannerSession&& other) noexcept;
    ScannerSession& operator=(ScannerSession&& other) noexcept;
    ~ScannerSession();

    std::string client_id;
    lorgnette::ScannerConfig config;
  };

  struct ScannerState {
    ScannerState(lorgnette::ScannerInfo info,
                 lorgnette::ScannerConfig template_config,
                 lorgnette::ScannerCapabilities capabilities);
    ScannerState(ScannerState&& other) noexcept;
    ScannerState& operator=(ScannerState&& other) noexcept;
    ~ScannerState();

    lorgnette::ScannerInfo info;
    lorgnette::ScannerConfig template_config;
    lorgnette::ScannerCapabilities capabilities;
    std::optional<ScannerSession> active_session;
  };

  std::string CreateFreshHandle();

  bool simulate_dbus_failure_ = false;
  std::vector<ScannerState> scanners_;
  std::optional<lorgnette::OperationResult> get_current_config_result_;
  std::optional<lorgnette::ScannerConfig> get_current_config_config_;
  std::optional<lorgnette::OperationResult> start_prepared_scan_result_;
  std::optional<lorgnette::OperationResult> read_scan_data_result_;
  std::vector<std::string> read_scan_data_chunks_;
  std::optional<lorgnette::OperationResult> cancel_scan_result_ =
      lorgnette::OPERATION_RESULT_ADF_JAMMED;
  size_t handle_count_ = 0;
  std::optional<std::vector<std::string>> scan_data_;
  std::vector<std::string> cancelled_jobs_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCANNING_FAKE_LORGNETTE_SCANNER_MANAGER_H_
