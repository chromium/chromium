// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNING_SCAN_SERVICE_H_
#define CHROME_BROWSER_ASH_SCANNING_SCAN_SERVICE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/webui/scanning/mojom/scanning.mojom.h"
#include "base/cancelable_callback.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/scanning/scanning_file_path_helper.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/wake_lock/power_save_blocker/power_save_blocker.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

class LorgnetteScannerManager;

// Implementation of the ash::scanning::mojom::ScanService interface. Used
// by the scanning WebUI (chrome://scanning) to get connected scanners, obtain
// scanner capabilities, and perform scans.
class ScanService : public scanning::mojom::ScanService,
                    public scanning::mojom::MultiPageScanController,
                    public KeyedService {
 public:
  ScanService(LorgnetteScannerManager* lorgnette_scanner_manager,
              base::FilePath my_files_path,
              base::FilePath google_drive_path,
              content::BrowserContext* context);
  ~ScanService() override;

  ScanService(const ScanService&) = delete;
  ScanService& operator=(const ScanService&) = delete;

  // scanning::mojom::ScanService:
  void GetScanners(GetScannersCallback callback) override;
  void GetScannerCapabilities(const base::UnguessableToken& scanner_id,
                              GetScannerCapabilitiesCallback callback) override;
  void StartScan(const base::UnguessableToken& scanner_id,
                 scanning::mojom::ScanSettingsPtr settings,
                 mojo::PendingRemote<scanning::mojom::ScanJobObserver> observer,
                 StartScanCallback callback) override;
  void StartMultiPageScan(
      const base::UnguessableToken& scanner_id,
      scanning::mojom::ScanSettingsPtr settings,
      mojo::PendingRemote<scanning::mojom::ScanJobObserver> observer,
      StartMultiPageScanCallback callback) override;
  void CancelScan() override;

  // scanning::mojom::MultiPageScanController:
  void ScanNextPage(const base::UnguessableToken& scanner_id,
                    scanning::mojom::ScanSettingsPtr settings,
                    ScanNextPageCallback callback) override;
  void RemovePage(uint32_t page_index) override;
  void RescanPage(const base::UnguessableToken& scanner_id,
                  scanning::mojom::ScanSettingsPtr settings,
                  uint32_t page_index,
                  ScanNextPageCallback callback) override;
  void CompleteMultiPageScan() override;

  // Binds receiver_ by consuming |pending_receiver|.
  void BindInterface(
      mojo::PendingReceiver<scanning::mojom::ScanService> pending_receiver);

  // Returns |scanned_images_| to verify the correct images are added/removed in
  // unit tests.
  std::vector<std::string> GetScannedImagesForTesting() const;

 private:
  // KeyedService:
  void Shutdown() override;

  // Processes the result of calling LorgnetteScannerManager::GetScannerNames().
  void OnScannerNamesReceived(GetScannersCallback callback,
                              std::vector<std::string> scanner_names);

  // Processes the result of calling
  // LorgnetteScannerManager::GetScannerCapabilities().
  void OnScannerCapabilitiesReceived(
      GetScannerCapabilitiesCallback callback,
      const std::optional<lorgnette::ScannerCapabilities>& capabilities);

  // Receives progress updates after calling LorgnetteScannerManager::Scan().
  // |page_number| indicates the page the |progress_percent| corresponds to.
  void OnProgressPercentReceived(uint32_t progress_percent,
                                 uint32_t page_number);

  // Processes each |scanned_image| received after calling
  // LorgnetteScannerManager::Scan(). |scan_to_path| is where images will be
  // saved, and |file_type| specifies the file type to use when saving scanned
  // images. If |page_index_to_replace| exists then |scanned_image| will replace
  // an existing scanned image instead of being appended.
  void OnPageReceived(const base::FilePath& scan_to_path,
                      const scanning::mojom::FileType file_type,
                      const std::optional<uint32_t> page_index_to_replace,
                      std::string scanned_image,
                      uint32_t page_number);

  // Processes the final result of calling LorgnetteScannerManager::Scan().
  // |failure_mode| is set to SCAN_FAILURE_MODE_NO_FAILURE when the scan
  // succeeds; otherwise, its value indicates what caused the scan to fail.
  void OnScanCompleted(bool is_multi_page_scan,
                       lorgnette::ScanFailureMode failure_mode);

  // For a multi-page scan, when a page scan completes, report a failure if it
  // exists.
  void OnMultiPageScanPageCompleted(lorgnette::ScanFailureMode failure_mode);

  // Processes the final result of calling
  // LorgnetteScannerManager::CancelScan().
  void OnCancelCompleted(bool success);

  // Called once the task runner finishes saving a PDF file.
  void OnPdfSaved(const bool success);

  // Called once the task runner finishes saving a page of a scan.
  void OnPageSaved(const base::FilePath& saved_file_path);

  // Sends the scan request to the scanner.
  bool SendScanRequest(
      const base::UnguessableToken& scanner_id,
      scanning::mojom::ScanSettingsPtr settings,
      const std::optional<uint32_t> page_index_to_replace,
      base::OnceCallback<void(lorgnette::ScanFailureMode failure_mode)>
          completion_callback);

  // Called once the task runner finishes saving the last page of a scan.
  void OnAllPagesSaved(lorgnette::ScanFailureMode failure_mode);

  // Sets the local member variables back to their initial empty state.
  void ClearScanState();

  // Sets the ScanJobOberver for a new scan.
  void SetScanJobObserver(
      mojo::PendingRemote<scanning::mojom::ScanJobObserver> observer);

  // Resets the mojo::Receiver |multi_page_controller_receiver_|.
  void ResetMultiPageScanController();

  // Determines whether the service supports saving scanned images to
  // |file_path|.
  bool FilePathSupported(const base::FilePath& file_path);

  // Returns the scanner name corresponding to the given |scanner_id| or an
  // empty string if the name cannot be found.
  std::string GetScannerName(const base::UnguessableToken& scanner_id);

  // Map of scanner IDs to display names. Used to pass the correct display name
  // to LorgnetteScannerManager when clients provide an ID.
  base::flat_map<base::UnguessableToken, std::string> scanner_names_;

  // Receives and dispatches method calls to this implementation of the
  // ash::scanning::mojom::ScanService interface.
  mojo::Receiver<scanning::mojom::ScanService> receiver_{this};

  // Receives and dispatches method calls to this implementation of the
  // ash::scanning::mojom::MultiPageScanController interface.
  mojo::Receiver<scanning::mojom::MultiPageScanController>
      multi_page_controller_receiver_{this};

  // Used to send scan job events to an observer. The remote is bound when a
  // scan job is started and is disconnected when the scan job is complete.
  mojo::Remote<scanning::mojom::ScanJobObserver> scan_job_observer_;

  // Unowned. Used to get scanner information and perform scans.
  raw_ptr<LorgnetteScannerManager> lorgnette_scanner_manager_;

  // The browser context from which scans are initiated.
  const raw_ptr<content::BrowserContext> context_;

  // Indicates whether there was a failure to save scanned images.
  bool page_save_failed_;

  // The scanned images used to create a multipage PDF.
  std::vector<std::string> scanned_images_;

  // The time a scan was started. Used in filenames when saving scanned images.
  base::Time start_time_;

  // The file paths of the pages scanned in a scan job.
  std::vector<base::FilePath> scanned_file_paths_;

  // Task runner used to convert and save scanned images.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Tracks the number of pages scanned for histogram recording.
  int num_pages_scanned_;

  // Indicates whether alternate pages must be rotated to account for an ADF
  // scanner that flips them.
  bool rotate_alternate_pages_;

  // Stores the dots per inch (DPI) of the requested scan.
  std::optional<int> scan_dpi_;

  // The time at which GetScanners() is called. Used to record the time between
  // a user launching the Scan app and being able to interact with it.
  base::TimeTicks get_scanners_time_;

  // The time a multi-page scan session starts. Used to record the duration of a
  // multi-page scan session.
  base::TimeTicks multi_page_start_time_;

  // Helper class for for file path manipulation and verification.
  ScanningFilePathHelper file_path_helper_;

  // Wake lock to ensure system does not suspend during a scan job.
  std::unique_ptr<device::PowerSaveBlocker> wake_lock_;

  // Called if there is no response from the scanner after a timeout. Used to
  // ensure the wake lock will be released if there is an error from the
  // scanner or backend.
  base::CancelableOnceCallback<void(ScanService*, bool,
                               lorgnette::ScanFailureMode)> timeout_callback_;

  // Needs to be last member variable.
  base::WeakPtrFactory<ScanService> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCANNING_SCAN_SERVICE_H_
