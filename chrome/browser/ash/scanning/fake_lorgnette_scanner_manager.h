// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNING_FAKE_LORGNETTE_SCANNER_MANAGER_H_
#define CHROME_BROWSER_ASH_SCANNING_FAKE_LORGNETTE_SCANNER_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/ash/components/dbus/lorgnette_manager/lorgnette_manager_client.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

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
  // TODO(crbug.com/479031241): Fix edge cases. 
  void CancelScan(const lorgnette::CancelScanRequest& request,
                  CancelScanCallback callback) override;

  // Flips a flag to simulate D-Bus failure for OpenScanner and SetOptions, i.e.
  // make them pass std::nullopt to their response callbacks.
  // TODO(crbug.com/479031241): Make this affect all relevant operations.
  void SimulateDBusFailure(bool simulate);
  void SimulateScannerFailure(bool simulate);

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

  // Sets the response returned by Scan().
  void SetScanResponse(
      const std::optional<std::vector<std::string>>& scan_data);

  // Optionally sets `scan_data` if a matching set of scan settings is found.
  void MaybeSetScanDataBasedOnSettings(const lorgnette::ScanSettings& settings);

  // Feeds data to be produced by all future scan jobs.
  // In the case of StartPreparedScan, associated ReadScanData invocations will
  // produce the given chunks in order (with result OPERATION_RESULT_SUCCESS),
  // followed by OPERATTION_RESULT_EOF.
  // In the case of Scan, the chunks represent pages and are returned in order
  // via repeated invocations of Scan's page callback, followed by a completion
  // callback invocation.
  // Note: Behavior can be overridden by Simulate{DBus,Scanner}Failure.
  void SetDataForFutureScanJobs(std::vector<std::string> data_chunks);

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

  struct JobState {
    JobState();
    JobState(const JobState&);
    JobState(JobState&&) noexcept;
    JobState& operator=(const JobState&);
    JobState& operator=(JobState&&) noexcept;
    ~JobState();

    bool cancelled = false;
    base::queue<std::string> remaining_chunks;
  };

  std::string CreateFreshHandle();

  bool simulate_dbus_failure_ = false;
  bool simulate_scanner_failure_ = false;
  size_t handle_count_ = 0;
  std::vector<ScannerState> scanners_;
  absl::flat_hash_map<std::string, JobState> scan_jobs_;
  std::optional<std::vector<std::string>> scan_data_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCANNING_FAKE_LORGNETTE_SCANNER_MANAGER_H_
