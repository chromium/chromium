// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/scan_service.h"

#include <cstdint>
#include <utility>

#include "ash/content/scanning/scanning_uma.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager.h"
#include "chrome/browser/ash/scanning/scanning_type_converters.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "third_party/skia/include/docs/SkPDFDocument.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"

namespace ash {

namespace {

namespace mojo_ipc = scanning::mojom;

// The conversion quality when converting from PNG to JPG.
constexpr int kJpgQuality = 100;

// The number of degrees to rotate a PDF image.
constexpr int kRotationDegrees = 180;

// The max progress percent that can be reported for a scanned page.
constexpr uint32_t kMaxProgressPercent = 100;

// Creates a filename for a scanned image using |start_time|, |page_number|, and
// |file_ext|.
std::string CreateFilename(const base::Time::Exploded& start_time,
                           uint32_t page_number,
                           const mojo_ipc::FileType file_type) {
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
      return base::StringPrintf("scan_%02d%02d%02d-%02d%02d%02d.pdf",
                                start_time.year, start_time.month,
                                start_time.day_of_month, start_time.hour,
                                start_time.minute, start_time.second);
  }

  return base::StringPrintf(
      "scan_%02d%02d%02d-%02d%02d%02d_%d.%s", start_time.year, start_time.month,
      start_time.day_of_month, start_time.hour, start_time.minute,
      start_time.second, page_number, file_ext.c_str());
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

// Converts |png_img| to JPG.
std::string PngToJpg(const std::string& png_img) {
  std::vector<uint8_t> jpg_img;
  const gfx::Image img = gfx::Image::CreateFrom1xPNGBytes(
      reinterpret_cast<const unsigned char*>(png_img.c_str()), png_img.size());
  if (!gfx::JPEG1xEncodedDataFromImage(img, kJpgQuality, &jpg_img)) {
    LOG(ERROR) << "Failed to convert image from PNG to JPG.";
    return "";
  }

  return std::string(jpg_img.begin(), jpg_img.end());
}

// Creates a new page for the PDF document and adds |image_data| to the page.
// |rotate| indicates whether the page should be rotated 180 degrees.
// Returns whether the page was successfully created.
bool AddPdfPage(sk_sp<SkDocument> pdf_doc,
                const sk_sp<SkData>& image_data,
                bool rotate) {
  const sk_sp<SkImage> image = SkImage::MakeFromEncoded(image_data);
  if (!image) {
    LOG(ERROR) << "Unable to generate image from encoded image data.";
    return false;
  }

  SkCanvas* page_canvas = pdf_doc->beginPage(image->width(), image->height());
  if (!page_canvas) {
    LOG(ERROR) << "Unable to access PDF page canvas.";
    return false;
  }

  // Rotate pages that were flipped by an ADF scanner.
  if (rotate) {
    page_canvas->rotate(kRotationDegrees);
    page_canvas->translate(-image->width(), -image->height());
  }

  page_canvas->drawImage(image, /*left=*/0, /*top=*/0);
  pdf_doc->endPage();
  return true;
}

// Converts |png_images| into JPGs, adds them to a single PDF, and writes the
// PDF to |file_path|. If |rotate_alternate_pages| is true, every other page
// is rotated 180 degrees. Returns whether the PDF was successfully saved.
bool SaveAsPdf(const std::vector<std::string>& png_images,
               const base::FilePath& file_path,
               bool rotate_alternate_pages) {
  DCHECK(!file_path.empty());

  SkFILEWStream pdf_outfile(file_path.value().c_str());
  if (!pdf_outfile.isValid()) {
    LOG(ERROR) << "Unable to open output file.";
    return false;
  }

  sk_sp<SkDocument> pdf_doc = SkPDF::MakeDocument(&pdf_outfile);
  SkASSERT(pdf_doc);

  // Never rotate first page of PDF.
  bool rotate_current_page = false;
  for (const auto& png_img : png_images) {
    const std::string jpg_img = PngToJpg(png_img);
    if (jpg_img.empty()) {
      LOG(ERROR) << "Unable to convert PNG image to JPG.";
      return false;
    }

    SkDynamicMemoryWStream img_stream;
    if (!img_stream.write(jpg_img.c_str(), jpg_img.size())) {
      LOG(ERROR) << "Unable to write image to dynamic memory stream.";
      return false;
    }

    const sk_sp<SkData> img_data = img_stream.detachAsData();
    if (img_data->isEmpty()) {
      LOG(ERROR) << "Stream data is empty.";
      return false;
    }

    if (!AddPdfPage(pdf_doc, img_data, rotate_current_page)) {
      LOG(ERROR) << "Unable to add new PDF page.";
      return false;
    }

    if (rotate_alternate_pages) {
      rotate_current_page = !rotate_current_page;
    }
  }

  pdf_doc->close();
  return true;
}

// Saves |scanned_image| to a file after converting it if necessary. Returns the
// file path to the saved file if the save succeeds.
base::FilePath SavePage(const base::FilePath& scan_to_path,
                        const mojo_ipc::FileType file_type,
                        std::string scanned_image,
                        uint32_t page_number,
                        const base::Time::Exploded& start_time) {
  std::string filename = CreateFilename(start_time, page_number, file_type);
  if (file_type == mojo_ipc::FileType::kPng) {
    if (!WriteImage(scan_to_path.Append(filename), scanned_image))
      return base::FilePath();
  } else if (file_type == mojo_ipc::FileType::kJpg) {
    scanned_image = PngToJpg(scanned_image);
    if (scanned_image.empty() ||
        !WriteImage(scan_to_path.Append(filename), scanned_image)) {
      return base::FilePath();
    }
  }

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
      NOTREACHED();
      return scanning::ScanJobFailureReason::kUnknownScannerError;
  }
}

// Records the histograms based on the scan job result.
void RecordScanJobResult(
    bool success,
    const base::Optional<scanning::ScanJobFailureReason>& failure_reason,
    int num_pages_scanned) {
  base::UmaHistogramBoolean("Scanning.ScanJobSuccessful", success);
  if (success) {
    base::UmaHistogramCounts100("Scanning.NumPagesScanned", num_pages_scanned);
    return;
  }

  if (failure_reason.has_value()) {
    base::UmaHistogramEnumeration("Scanning.ScanJobFailureReason",
                                  failure_reason.value());
  }
}

}  // namespace

ScanService::ScanService(LorgnetteScannerManager* lorgnette_scanner_manager,
                         base::FilePath my_files_path,
                         base::FilePath google_drive_path)
    : lorgnette_scanner_manager_(lorgnette_scanner_manager),
      my_files_path_(std::move(my_files_path)),
      google_drive_path_(std::move(google_drive_path)),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  DCHECK(lorgnette_scanner_manager_);
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
  const std::string scanner_name = GetScannerName(scanner_id);
  if (scanner_name.empty()) {
    std::move(callback).Run(false);
    RecordScanJobResult(false, scanning::ScanJobFailureReason::kScannerNotFound,
                        /*not used*/ 0);
    return;
  }

  // Determine if an ADF scanner that flips alternate pages was selected.
  rotate_alternate_pages_ =
      RE2::PartialMatch(scanner_name, RE2("([Ee][Pp][Ss][Oo][Nn])(.*)")) &&
      RE2::PartialMatch(settings->source_name,
                        RE2("([Aa][Dd][Ff] [Dd][Uu][Pp][Ll][Ee][Xx])"));

  if (!FilePathSupported(settings->scan_to_path)) {
    std::move(callback).Run(false);
    RecordScanJobResult(false,
                        scanning::ScanJobFailureReason::kUnsupportedScanToPath,
                        /*not used*/ 0);
    return;
  }

  scan_job_observer_.reset();
  scan_job_observer_.Bind(std::move(observer));
  // Unretained is safe here, because `this` owns `scan_job_observer_`, and no
  // reply callbacks will be invoked once the mojo::Remote is destroyed.
  scan_job_observer_.set_disconnect_handler(
      base::BindOnce(&ScanService::CancelScan, base::Unretained(this)));

  base::Time::Now().LocalExplode(&start_time_);
  ClearScanState();
  lorgnette_scanner_manager_->Scan(
      scanner_name, mojo::ConvertTo<lorgnette::ScanSettings>(settings),
      base::BindRepeating(&ScanService::OnProgressPercentReceived,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ScanService::OnPageReceived,
                          weak_ptr_factory_.GetWeakPtr(),
                          settings->scan_to_path, settings->file_type),
      base::BindOnce(&ScanService::OnScanCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
  std::move(callback).Run(true);
}

void ScanService::CancelScan() {
  lorgnette_scanner_manager_->CancelScan(base::BindOnce(
      &ScanService::OnCancelCompleted, weak_ptr_factory_.GetWeakPtr()));
}

void ScanService::BindInterface(
    mojo::PendingReceiver<mojo_ipc::ScanService> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void ScanService::SetGoogleDrivePathForTesting(
    const base::FilePath& google_drive_path) {
  google_drive_path_ = google_drive_path;
}

void ScanService::SetMyFilesPathForTesting(
    const base::FilePath& my_files_path) {
  my_files_path_ = my_files_path;
}

void ScanService::Shutdown() {
  lorgnette_scanner_manager_ = nullptr;
  receiver_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
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
    const base::Optional<lorgnette::ScannerCapabilities>& capabilities) {
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
      mojo::ConvertTo<mojo_ipc::ScannerCapabilitiesPtr>(capabilities.value()));
}

void ScanService::OnProgressPercentReceived(uint32_t progress_percent,
                                            uint32_t page_number) {
  DCHECK_LE(progress_percent, kMaxProgressPercent);
  scan_job_observer_->OnPageProgress(page_number, progress_percent);
}

void ScanService::OnPageReceived(const base::FilePath& scan_to_path,
                                 const mojo_ipc::FileType file_type,
                                 std::string scanned_image,
                                 uint32_t page_number) {
  // TODO(b/172670649): Update LorgnetteManagerClient to pass scan data as a
  // vector.
  // In case the last reported progress percent was less than 100, send one
  // final progress event before the page complete event.
  scan_job_observer_->OnPageProgress(page_number, kMaxProgressPercent);
  scan_job_observer_->OnPageComplete(
      std::vector<uint8_t>(scanned_image.begin(), scanned_image.end()));
  ++num_pages_scanned_;

  // If the selected file type is PDF, the PDF will be created after all the
  // scanned images are received.
  if (file_type == mojo_ipc::FileType::kPdf) {
    scanned_images_.push_back(std::move(scanned_image));

    // The output of multi-page PDF scans is a single file so only create and
    // append a single file path.
    if (scanned_file_paths_.empty()) {
      DCHECK_EQ(1, page_number);
      scanned_file_paths_.push_back(scan_to_path.Append(CreateFilename(
          start_time_, /*not used*/ 0, mojo_ipc::FileType::kPdf)));
    }
    return;
  }

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&SavePage, scan_to_path, file_type,
                     std::move(scanned_image), page_number, start_time_),
      base::BindOnce(&ScanService::OnPageSaved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScanService::OnScanCompleted(bool success,
                                  lorgnette::ScanFailureMode failure_mode) {
  DCHECK_EQ(success, failure_mode == lorgnette::SCAN_FAILURE_MODE_NO_FAILURE);

  // |scanned_images_| only has data for PDF scans.
  if (success && !scanned_images_.empty()) {
    DCHECK(!scanned_file_paths_.empty());
    base::PostTaskAndReplyWithResult(
        task_runner_.get(), FROM_HERE,
        base::BindOnce(&SaveAsPdf, scanned_images_, scanned_file_paths_.back(),
                       rotate_alternate_pages_),
        base::BindOnce(&ScanService::OnPdfSaved,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Post a task to the task runner to ensure all the pages have been saved
  // before reporting the scan job as complete.
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(
          [](lorgnette::ScanFailureMode failure_mode) { return failure_mode; },
          failure_mode),
      base::BindOnce(&ScanService::OnAllPagesSaved,
                     weak_ptr_factory_.GetWeakPtr(), success));
}

void ScanService::OnCancelCompleted(bool success) {
  if (success)
    ClearScanState();
  scan_job_observer_->OnCancelComplete(success);
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

void ScanService::OnAllPagesSaved(bool success,
                                  lorgnette::ScanFailureMode failure_mode) {
  DCHECK_EQ(success, failure_mode == lorgnette::SCAN_FAILURE_MODE_NO_FAILURE);

  base::Optional<scanning::ScanJobFailureReason> failure_reason = base::nullopt;
  if (!success) {
    failure_reason = GetScanJobFailureReason(failure_mode);
    scanned_file_paths_.clear();
  } else if (page_save_failed_) {
    failure_mode = lorgnette::SCAN_FAILURE_MODE_UNKNOWN;
    failure_reason = scanning::ScanJobFailureReason::kSaveToDiskFailed;
    scanned_file_paths_.clear();
  }

  scan_job_observer_->OnScanComplete(
      mojo::ConvertTo<mojo_ipc::ScanResult>(
          static_cast<lorgnette::ScanFailureMode>(failure_mode)),
      scanned_file_paths_);
  RecordScanJobResult(success && !page_save_failed_, failure_reason,
                      num_pages_scanned_);
}

void ScanService::ClearScanState() {
  page_save_failed_ = false;
  scanned_file_paths_.clear();
  scanned_images_.clear();
  num_pages_scanned_ = 0;
}

bool ScanService::FilePathSupported(const base::FilePath& file_path) {
  if (file_path == my_files_path_ ||
      (!file_path.ReferencesParent() &&
       (my_files_path_.IsParent(file_path) ||
        google_drive_path_.IsParent(file_path)))) {
    return true;
  }

  return false;
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

}  // namespace ash
