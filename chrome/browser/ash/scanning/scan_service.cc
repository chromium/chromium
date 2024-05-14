// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/scan_service.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/webui/scanning/mojom/scanning_type_converters.h"
#include "ash/webui/scanning/scanning_uma.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager.h"
#include "chrome/browser/ash/scanning/scanning_file_path_helper.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chromeos/utils/pdf_conversion.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace ash {

namespace {

namespace mojo_ipc = scanning::mojom;

// The max progress percent that can be reported for a scanned page.
constexpr uint32_t kMaxProgressPercent = 100;

// The number of minutes to wait before the scan times out and is canceled.
constexpr base::TimeDelta kTimeout = base::Minutes(15);

// Creates a filename for a scanned image using |start_time|, |page_number|, and
// |file_ext|.
std::string CreateFilename(const base::Time& start_time,
                           uint32_t page_number,
                           const mojo_ipc::FileType file_type) {
  const std::string timestamp =
      base::UnlocalizedTimeFormatWithPattern(start_time, "yyMMdd-HHmmss");

  std::string file_ext;
  switch (file_type) {
    case mojo_ipc::FileType::kPng:
      file_ext = "png";
      break;
    case mojo_ipc::FileType::kJpg:
      file_ext = "jpg";
      break;
    case mojo_ipc::FileType::kPdf:
      // The filename of a PDF doesn't include the page number.
      return base::StringPrintf("scan_%s.pdf", timestamp.c_str());
  }

  return base::StringPrintf("scan_%s_%d.%s", timestamp.c_str(), page_number,
                            file_ext.c_str());
}

// Helper function that writes |scanned_image| to |file_path|.
// Returns whether the image was successfully written.
bool WriteImage(const base::FilePath& file_path,
                const std::string& scanned_image) {
  if (!base::WriteFile(file_path, scanned_image)) {
    LOG(ERROR) << "Failed to save scanned image: " << file_path.value().c_str();
    return false;
  }

  return true;
}

// Adds |jpg_images| to a single PDF, and writes the PDF to |file_path|. If
// |rotate_alternate_pages| is true, every other page is rotated 180 degrees.
// Returns whether the PDF was successfully saved.
bool SaveAsPdf(const std::vector<std::string>& jpg_images,
               const base::FilePath& file_path,
               bool rotate_alternate_pages,
               bool is_multi_page_scan,
               std::optional<int> dpi) {
  size_t total_image_size = 0;
  for (const std::string& image : jpg_images) {
    total_image_size += image.size();
  }
  base::UmaHistogramCounts1M(
      is_multi_page_scan
          ? "Scanning.MultiPageScan.CombinedImageSizeInKbBeforePdf"
          : "Scanning.CombinedImageSizeInKbBeforePdf",
      total_image_size / 1024);

  const base::TimeTicks pdf_start_time = base::TimeTicks::Now();
  const bool pdf_saved = chromeos::ConvertJpgImagesToPdf(
      jpg_images, file_path, rotate_alternate_pages, dpi);
  base::UmaHistogramTimes(is_multi_page_scan
                              ? "Scanning.MultiPageScan.PDFGenerationTime"
                              : "Scanning.PDFGenerationTime",
                          base::TimeTicks::Now() - pdf_start_time);
  return pdf_saved;
}

// Saves |scanned_image| to a file after converting it if necessary. Returns the
// file path to the saved file if the save succeeds.
base::FilePath SavePage(const base::FilePath& scan_to_path,
                        const mojo_ipc::FileType file_type,
                        std::string scanned_image,
                        uint32_t page_number,
                        const base::Time& start_time) {
  std::string filename = CreateFilename(start_time, page_number, file_type);
  if (!WriteImage(scan_to_path.Append(filename), scanned_image))
    return base::FilePath();

  return scan_to_path.Append(filename);
}

// Returns a ScanJobFailureReason corresponding to the given |failure_mode|.
scanning::ScanJobFailureReason GetScanJobFailureReason(
    const lorgnette::ScanFailureMode failure_mode) {
  switch (failure_mode) {
    case lorgnette::SCAN_FAILURE_MODE_UNKNOWN:
      return scanning::ScanJobFailureReason::kUnknownScannerError;
    case lorgnette::SCAN_FAILURE_MODE_DEVICE_BUSY:
      return scanning::ScanJobFailureReason::kDeviceBusy;
    case lorgnette::SCAN_FAILURE_MODE_ADF_JAMMED:
      return scanning::ScanJobFailureReason::kAdfJammed;
    case lorgnette::SCAN_FAILURE_MODE_ADF_EMPTY:
      return scanning::ScanJobFailureReason::kAdfEmpty;
    case lorgnette::SCAN_FAILURE_MODE_FLATBED_OPEN:
      return scanning::ScanJobFailureReason::kFlatbedOpen;
    case lorgnette::SCAN_FAILURE_MODE_IO_ERROR:
      return scanning::ScanJobFailureReason::kIoError;
    case lorgnette::SCAN_FAILURE_MODE_NO_FAILURE:
    case lorgnette::ScanFailureMode_INT_MIN_SENTINEL_DO_NOT_USE_:
    case lorgnette::ScanFailureMode_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED_IN_MIGRATION();
      return scanning::ScanJobFailureReason::kUnknownScannerError;
  }
}

// Records the histograms based on the scan job result.
void RecordScanJobResult(
    bool success,
    const std::optional<scanning::ScanJobFailureReason>& failure_reason,
    int num_files_created,
    int num_pages_scanned) {
  base::UmaHistogramBoolean("Scanning.ScanJobSuccessful", success);
  if (success) {
    base::UmaHistogramCounts100("Scanning.NumFilesCreated", num_files_created);
    base::UmaHistogramCounts100("Scanning.NumPagesScanned", num_pages_scanned);
    return;
  }

  if (failure_reason.has_value()) {
    base::UmaHistogramEnumeration("Scanning.ScanJobFailureReason",
                                  failure_reason.value());
  }
}

std::unique_ptr<device::PowerSaveBlocker> RequestWakeLock(
    const std::string& description) {
  return std::make_unique<device::PowerSaveBlocker>(
      /*type=*/device::mojom::WakeLockType::kPreventDisplaySleepAllowDimming,
      /*reason=*/device::mojom::WakeLockReason::kOther,
      /*description=*/description,
      /*ui_task_runner=*/base::SequencedTaskRunner::GetCurrentDefault(),
      /*blocking_task_runner=*/nullptr);
}

}  // namespace

ScanService::ScanService(LorgnetteScannerManager* lorgnette_scanner_manager,
                         base::FilePath my_files_path,
                         base::FilePath google_drive_path,
                         content::BrowserContext* context)
    : lorgnette_scanner_manager_(lorgnette_scanner_manager),
      context_(context),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      file_path_helper_(std::move(google_drive_path),
                        std::move(my_files_path)) {
  DCHECK(lorgnette_scanner_manager_);
  DCHECK(context_);
}

ScanService::~ScanService() = default;

void ScanService::GetScanners(GetScannersCallback callback) {
  get_scanners_time_ = base::TimeTicks::Now();
  lorgnette_scanner_manager_->GetScannerNames(
      base::BindOnce(&ScanService::OnScannerNamesReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ScanService::GetScannerCapabilities(
    const base::UnguessableToken& scanner_id,
    GetScannerCapabilitiesCallback callback) {
  const std::string scanner_name = GetScannerName(scanner_id);
  if (scanner_name.empty()) {
    std::move(callback).Run(mojo_ipc::ScannerCapabilities::New());
    return;
  }

  lorgnette_scanner_manager_->GetScannerCapabilities(
      scanner_name,
      base::BindOnce(&ScanService::OnScannerCapabilitiesReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ScanService::StartScan(
    const base::UnguessableToken& scanner_id,
    mojo_ipc::ScanSettingsPtr settings,
    mojo::PendingRemote<mojo_ipc::ScanJobObserver> observer,
    StartScanCallback callback) {
  ClearScanState();
  SetScanJobObserver(std::move(observer));

  wake_lock_ = RequestWakeLock("Scan Job in Progress");

  std::move(callback).Run(SendScanRequest(
      scanner_id, std::move(settings), /*page_index_to_replace=*/std::nullopt,
      base::BindOnce(&ScanService::OnScanCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     /*is_multi_page_scan=*/false)));
}

void ScanService::StartMultiPageScan(
    const base::UnguessableToken& scanner_id,
    scanning::mojom::ScanSettingsPtr settings,
    mojo::PendingRemote<scanning::mojom::ScanJobObserver> observer,
    StartMultiPageScanCallback callback) {
  if (multi_page_controller_receiver_.is_bound()) {
    LOG(ERROR) << "Unable to start multi-page scan, controller already bound.";
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  ClearScanState();
  SetScanJobObserver(std::move(observer));

  wake_lock_ = RequestWakeLock("Multi Page Scan in Progress");

  const bool success = SendScanRequest(
      scanner_id, std::move(settings), /*page_index_to_replace=*/std::nullopt,
      base::BindOnce(&ScanService::OnMultiPageScanPageCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
  if (!success) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  mojo::PendingRemote<scanning::mojom::MultiPageScanController> pending_remote =
      multi_page_controller_receiver_.BindNewPipeAndPassRemote();
  // This allows a multi-page scan session to be cancelled by resetting the
  // message pipe.
  multi_page_controller_receiver_.set_disconnect_handler(
      base::BindOnce(&ScanService::ResetMultiPageScanController,
                     weak_ptr_factory_.GetWeakPtr()));
  std::move(callback).Run(std::move(pending_remote));

  multi_page_start_time_ = base::TimeTicks::Now();
}

bool ScanService::SendScanRequest(
    const base::UnguessableToken& scanner_id,
    mojo_ipc::ScanSettingsPtr settings,
    const std::optional<uint32_t> page_index_to_replace,
    base::OnceCallback<void(lorgnette::ScanFailureMode failure_mode)>
        completion_callback) {
  const std::string scanner_name = GetScannerName(scanner_id);
  if (scanner_name.empty()) {
    RecordScanJobResult(false, scanning::ScanJobFailureReason::kScannerNotFound,
                        /*not used*/ 0, /*not used*/ 0);
    return false;
  }

  if (!file_path_helper_.IsFilePathSupported(settings->scan_to_path)) {
    RecordScanJobResult(false,
                        scanning::ScanJobFailureReason::kUnsupportedScanToPath,
                        /*not used*/ 0, /*not used*/ 0);
    return false;
  }

  // Determine if an ADF scanner that flips alternate pages was selected.
  rotate_alternate_pages_ = lorgnette_scanner_manager_->IsRotateAlternate(
      scanner_name, settings->source_name);

  // Save the DPI information for the scan.
  scan_dpi_ = settings->resolution_dpi;

  timeout_callback_.Reset(base::BindOnce(&ScanService::OnScanCompleted));
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      // If this callback is called, `is_multi_page_scan` does not matter.
      // Always setting it to false here makes it simpler.
      FROM_HERE, base::BindOnce(timeout_callback_.callback(), this,
	                        /*is_multi_page_scan=*/false,
                                lorgnette::SCAN_FAILURE_MODE_IO_ERROR),
      kTimeout);

  start_time_ = base::Time::Now();
  lorgnette_scanner_manager_->Scan(
      scanner_name,
      mojo::StructTraits<lorgnette::ScanSettings,
                         mojo_ipc::ScanSettingsPtr>::ToMojom(settings),
      base::BindRepeating(&ScanService::OnProgressPercentReceived,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          &ScanService::OnPageReceived, weak_ptr_factory_.GetWeakPtr(),
          settings->scan_to_path, settings->file_type, page_index_to_replace),
      std::move(completion_callback));
  return true;
}

void ScanService::CancelScan() {
  // In case the user cancels, we don't want to cancel again in the future.
  timeout_callback_.Cancel();
  lorgnette_scanner_manager_->CancelScan(base::BindOnce(
      &ScanService::OnCancelCompleted, weak_ptr_factory_.GetWeakPtr()));
}

void ScanService::ScanNextPage(const base::UnguessableToken& scanner_id,
                               scanning::mojom::ScanSettingsPtr settings,
                               ScanNextPageCallback callback) {
  std::move(callback).Run(SendScanRequest(
      scanner_id, std::move(settings), /*page_index_to_replace=*/std::nullopt,
      base::BindOnce(&ScanService::OnMultiPageScanPageCompleted,
                     weak_ptr_factory_.GetWeakPtr())));
}

void ScanService::RemovePage(uint32_t page_index) {
  if (page_index >= scanned_images_.size()) {
    multi_page_controller_receiver_.ReportBadMessage(
        "Invalid page_index passed to ScanService::RemovePage()");
    return;
  }

  if (scanned_images_.size() == 0) {
    multi_page_controller_receiver_.ReportBadMessage(
        "Invalid call to ScanService::RemovePage(), no scanned images "
        "available to remove");
    return;
  }

  base::UmaHistogramEnumeration(
      "Scanning.MultiPageScan.ToolbarAction",
      scanning::ScanMultiPageToolbarAction::kRemovePage);
  if (scanned_images_.size() == 1) {
    ClearScanState();
    multi_page_controller_receiver_.reset();
    return;
  }

  scanned_images_.erase(scanned_images_.begin() + page_index);
  --num_pages_scanned_;
}

void ScanService::RescanPage(const base::UnguessableToken& scanner_id,
                             scanning::mojom::ScanSettingsPtr settings,
                             uint32_t page_index,
                             ScanNextPageCallback callback) {
  if (scanned_images_.size() == 0) {
    multi_page_controller_receiver_.ReportBadMessage(
        "Invalid call to ScanService::RescanPage(), no scanned images "
        "available to rescan");
    return;
  }

  if (page_index >= scanned_images_.size()) {
    multi_page_controller_receiver_.ReportBadMessage(
        "Invalid page_index passed to ScanService::RescanPage()");
    return;
  }

  std::move(callback).Run(
      SendScanRequest(scanner_id, std::move(settings), page_index,
                      base::BindOnce(&ScanService::OnMultiPageScanPageCompleted,
                                     weak_ptr_factory_.GetWeakPtr())));
  base::UmaHistogramEnumeration(
      "Scanning.MultiPageScan.ToolbarAction",
      scanning::ScanMultiPageToolbarAction::kRescanPage);
}

void ScanService::CompleteMultiPageScan() {
  OnScanCompleted(/*is_multi_page_scan=*/true,
                  lorgnette::SCAN_FAILURE_MODE_NO_FAILURE);
  base::UmaHistogramCounts100("Scanning.MultiPageScan.NumPagesScanned",
                              num_pages_scanned_);
  base::UmaHistogramLongTimes100(
      "Scanning.MultiPageScan.SessionDuration",
      base::TimeTicks::Now() - multi_page_start_time_);
  multi_page_start_time_ = base::TimeTicks();
  multi_page_controller_receiver_.reset();
}

void ScanService::BindInterface(
    mojo::PendingReceiver<mojo_ipc::ScanService> pending_receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(pending_receiver));
}

void ScanService::Shutdown() {
  timeout_callback_.Cancel();
  lorgnette_scanner_manager_ = nullptr;
  receiver_.reset();
  multi_page_controller_receiver_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  wake_lock_.reset();
}

void ScanService::OnScannerNamesReceived(
    GetScannersCallback callback,
    std::vector<std::string> scanner_names) {
  base::UmaHistogramCounts100("Scanning.NumDetectedScanners",
                              scanner_names.size());
  scanner_names_.clear();
  scanner_names_.reserve(scanner_names.size());
  std::vector<mojo_ipc::ScannerPtr> scanners;
  scanners.reserve(scanner_names.size());
  for (const auto& name : scanner_names) {
    base::UnguessableToken id = base::UnguessableToken::Create();
    scanner_names_[id] = name;
    scanners.push_back(mojo_ipc::Scanner::New(id, base::UTF8ToUTF16(name)));
  }

  std::move(callback).Run(std::move(scanners));
}

void ScanService::OnScannerCapabilitiesReceived(
    GetScannerCapabilitiesCallback callback,
    const std::optional<lorgnette::ScannerCapabilities>& capabilities) {
  if (!capabilities) {
    LOG(ERROR) << "Failed to get scanner capabilities.";
    std::move(callback).Run(mojo_ipc::ScannerCapabilities::New());
    return;
  }

  // If this is the first time capabilities have been received since the last
  // call to GetScanners(), record the time between the two events to capture
  // the time between the Scan app launching and the user being able to interact
  // with the app (e.g. select a scanner, change scan settings, or start a
  // scan). If the user selects a different scanner and new capabilities are
  // received, don't record the metric again.
  if (!get_scanners_time_.is_null()) {
    base::UmaHistogramMediumTimes("Scanning.ReadyTime",
                                  base::TimeTicks::Now() - get_scanners_time_);
    get_scanners_time_ = base::TimeTicks();
  }

  std::move(callback).Run(
      mojo::StructTraits<
          ash::scanning::mojom::ScannerCapabilitiesPtr,
          lorgnette::ScannerCapabilities>::ToMojom(capabilities.value()));
}

void ScanService::OnProgressPercentReceived(uint32_t progress_percent,
                                            uint32_t page_number) {
  DCHECK_LE(progress_percent, kMaxProgressPercent);
  scan_job_observer_->OnPageProgress(page_number, progress_percent);
}

void ScanService::OnPageReceived(
    const base::FilePath& scan_to_path,
    const mojo_ipc::FileType file_type,
    const std::optional<uint32_t> page_index_to_replace,
    std::string scanned_image,
    uint32_t page_number) {
  timeout_callback_.Cancel();
  // TODO(b/172670649): Update LorgnetteManagerClient to pass scan data as a
  // vector.
  // In case the last reported progress percent was less than 100, send one
  // final progress event before the page complete event.
  scan_job_observer_->OnPageProgress(page_number, kMaxProgressPercent);

  uint32_t new_page_index;
  if (file_type == mojo_ipc::FileType::kPdf) {
    new_page_index = page_index_to_replace.has_value()
                         ? page_index_to_replace.value()
                         : scanned_images_.size();
  } else {
    // Non-PDF scans do not save images in |scanned_images_| so the next index
    // is based off |page_number|.
    DCHECK(!page_index_to_replace.has_value());
    new_page_index = page_number - 1;
  }
  scan_job_observer_->OnPageComplete(
      std::vector<uint8_t>(scanned_image.begin(), scanned_image.end()),
      new_page_index);

  // Only increment the |num_pages_scanned_| tracker if appending, not
  // replacing, a page.
  if (!page_index_to_replace.has_value()) {
    ++num_pages_scanned_;
  }

  // If the selected file type is PDF, the PDF will be created after all the
  // scanned images are received.
  if (file_type == mojo_ipc::FileType::kPdf) {
    if (!page_index_to_replace.has_value()) {
      scanned_images_.push_back(std::move(scanned_image));
    } else {
      DCHECK(page_index_to_replace.value() >= 0 &&
             page_index_to_replace.value() < scanned_images_.size());
      scanned_images_[page_index_to_replace.value()] = std::move(scanned_image);
    }

    // The output of multi-page PDF scans is a single file so only create and
    // append a single file path.
    if (scanned_file_paths_.empty()) {
      DCHECK_EQ(1u, page_number);
      scanned_file_paths_.push_back(scan_to_path.Append(CreateFilename(
          start_time_, /*not used*/ 0, mojo_ipc::FileType::kPdf)));
    }
    return;
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SavePage, scan_to_path, file_type,
                     std::move(scanned_image), page_number, start_time_),
      base::BindOnce(&ScanService::OnPageSaved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScanService::OnScanCompleted(bool is_multi_page_scan,
                                  lorgnette::ScanFailureMode failure_mode) {
  // |scanned_images_| only has data for PDF scans.
  if (failure_mode == lorgnette::SCAN_FAILURE_MODE_NO_FAILURE &&
      !scanned_images_.empty()) {
    DCHECK(!scanned_file_paths_.empty());
    timeout_callback_.Cancel();
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&SaveAsPdf, scanned_images_, scanned_file_paths_.back(),
                       rotate_alternate_pages_, is_multi_page_scan, scan_dpi_),
        base::BindOnce(&ScanService::OnPdfSaved,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Post a task to the task runner to ensure all the pages have been saved
  // before reporting the scan job as complete.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](lorgnette::ScanFailureMode failure_mode) { return failure_mode; },
          failure_mode),
      base::BindOnce(&ScanService::OnAllPagesSaved,
                     weak_ptr_factory_.GetWeakPtr()));
  wake_lock_.reset();
}

void ScanService::OnMultiPageScanPageCompleted(
    lorgnette::ScanFailureMode failure_mode) {
  if (failure_mode ==
      lorgnette::ScanFailureMode::SCAN_FAILURE_MODE_NO_FAILURE) {
    base::UmaHistogramEnumeration("Scanning.MultiPageScan.PageScanResult",
                                  scanning::ScanJobFailureReason::kSuccess);
    // Reset the timeout for each page in a multi page scan so timeout isn't
    // triggered for a long scan.
    timeout_callback_.Cancel();
    return;
  }

  scan_job_observer_->OnMultiPageScanFail(failure_mode);

  base::UmaHistogramEnumeration("Scanning.MultiPageScan.PageScanResult",
                                GetScanJobFailureReason(failure_mode));
}

void ScanService::OnCancelCompleted(bool success) {
  if (success)
    ClearScanState();
  scan_job_observer_->OnCancelComplete(success);
  wake_lock_.reset();
}

void ScanService::OnPdfSaved(const bool success) {
  page_save_failed_ = !success;
}

void ScanService::OnPageSaved(const base::FilePath& saved_file_path) {
  page_save_failed_ = page_save_failed_ || saved_file_path.empty();
  if (page_save_failed_) {
    return;
  }

  scanned_file_paths_.push_back(saved_file_path);
}

void ScanService::OnAllPagesSaved(lorgnette::ScanFailureMode failure_mode) {
  std::optional<scanning::ScanJobFailureReason> failure_reason = std::nullopt;
  if (failure_mode != lorgnette::SCAN_FAILURE_MODE_NO_FAILURE) {
    failure_reason = GetScanJobFailureReason(failure_mode);
    scanned_file_paths_.clear();
  } else if (page_save_failed_) {
    failure_mode = lorgnette::SCAN_FAILURE_MODE_UNKNOWN;
    failure_reason = scanning::ScanJobFailureReason::kSaveToDiskFailed;
    scanned_file_paths_.clear();
  }

  scan_job_observer_->OnScanComplete(failure_mode, scanned_file_paths_);
  HoldingSpaceKeyedService* holding_space_keyed_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(context_);
  if (holding_space_keyed_service) {
    for (const auto& saved_scan_path : scanned_file_paths_)
      holding_space_keyed_service->AddItemOfType(HoldingSpaceItem::Type::kScan,
                                                 saved_scan_path);
  }
  RecordScanJobResult(failure_mode == lorgnette::SCAN_FAILURE_MODE_NO_FAILURE &&
                          !page_save_failed_,
                      failure_reason, scanned_file_paths_.size(),
                      num_pages_scanned_);
}

void ScanService::ClearScanState() {
  page_save_failed_ = false;
  rotate_alternate_pages_ = false;
  scan_dpi_ = std::nullopt;
  scanned_file_paths_.clear();
  scanned_images_.clear();
  num_pages_scanned_ = 0;
  wake_lock_.reset();
}

void ScanService::SetScanJobObserver(
    mojo::PendingRemote<mojo_ipc::ScanJobObserver> observer) {
  scan_job_observer_.reset();
  scan_job_observer_.Bind(std::move(observer));
  scan_job_observer_.set_disconnect_handler(
      base::BindOnce(&ScanService::CancelScan, weak_ptr_factory_.GetWeakPtr()));
}

void ScanService::ResetMultiPageScanController() {
  multi_page_controller_receiver_.reset();
  ClearScanState();
}

std::string ScanService::GetScannerName(
    const base::UnguessableToken& scanner_id) {
  const auto it = scanner_names_.find(scanner_id);
  if (it == scanner_names_.end()) {
    LOG(ERROR) << "Failed to find scanner name using the given scanner id.";
    return "";
  }

  return it->second;
}

std::vector<std::string> ScanService::GetScannedImagesForTesting() const {
  return scanned_images_;
}

}  // namespace ash
