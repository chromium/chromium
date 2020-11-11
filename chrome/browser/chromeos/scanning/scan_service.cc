// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scanning/scan_service.h"

#include <cstdint>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/scanning/lorgnette_scanner_manager.h"
#include "chrome/browser/chromeos/scanning/scanning_type_converters.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"

namespace chromeos {

namespace {

namespace mojo_ipc = scanning::mojom;

// Path to the active user's "My files" folder.
constexpr char kActiveUserMyFilesPath[] = "/home/chronos/user/MyFiles";

// The conversion quality when converting from PNG to JPG.
constexpr int kJpgQuality = 100;

// The max progress percent that can be reported for a scanned page.
constexpr uint32_t kMaxProgressPercent = 100;

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

}  // namespace

ScanService::ScanService(LorgnetteScannerManager* lorgnette_scanner_manager,
                         base::FilePath my_files_path,
                         base::FilePath google_drive_path)
    : lorgnette_scanner_manager_(lorgnette_scanner_manager),
      my_files_path_(std::move(my_files_path)),
      google_drive_path_(std::move(google_drive_path)) {
  DCHECK(lorgnette_scanner_manager_);
}

ScanService::~ScanService() = default;

void ScanService::GetScanners(GetScannersCallback callback) {
  lorgnette_scanner_manager_->GetScannerNames(
      base::BindOnce(&ScanService::OnScannerNamesReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ScanService::GetScannerCapabilities(
    const base::UnguessableToken& scanner_id,
    GetScannerCapabilitiesCallback callback) {
  const std::string scanner_name = GetScannerName(scanner_id);
  if (scanner_name.empty())
    std::move(callback).Run(mojo_ipc::ScannerCapabilities::New());

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
  if (scanner_name.empty() || !FilePathSupported(settings->scan_to_path)) {
    std::move(callback).Run(false);
    return;
  }

  scan_job_observer_.Bind(std::move(observer));

  base::Time::Now().LocalExplode(&start_time_);
  save_failed_ = false;
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

  std::move(callback).Run(
      mojo::ConvertTo<mojo_ipc::ScannerCapabilitiesPtr>(capabilities.value()));
}

void ScanService::OnProgressPercentReceived(uint32_t progress_percent,
                                            uint32_t page_number) {
  DCHECK_LE(progress_percent, kMaxProgressPercent);
  DCHECK(scan_job_observer_.is_connected());
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
  DCHECK(scan_job_observer_.is_connected());
  scan_job_observer_->OnPageProgress(page_number, kMaxProgressPercent);
  scan_job_observer_->OnPageComplete(
      std::vector<uint8_t>(scanned_image.begin(), scanned_image.end()));

  std::string filename;
  std::string file_ext;
  switch (file_type) {
    case mojo_ipc::FileType::kPng:
      file_ext = "png";
      break;
    case mojo_ipc::FileType::kJpg:
      file_ext = "jpg";
      scanned_image = PngToJpg(scanned_image);
      if (scanned_image == "") {
        save_failed_ = true;
        return;
      }
      break;
    default:
      LOG(ERROR) << "Selected file type not supported.";
      save_failed_ = true;
      return;
  }

  filename = base::StringPrintf(
      "scan_%02d%02d%02d-%02d%02d%02d_%d.%s", start_time_.year,
      start_time_.month, start_time_.day_of_month, start_time_.hour,
      start_time_.minute, start_time_.second, page_number, file_ext.c_str());
  const auto file_path = scan_to_path.Append(filename);
  if (!base::WriteFile(file_path, scanned_image)) {
    LOG(ERROR) << "Failed to save scanned image: " << file_path.value().c_str();
    save_failed_ = true;
  }
}

void ScanService::OnScanCompleted(bool success) {
  DCHECK(scan_job_observer_.is_connected());
  scan_job_observer_->OnScanComplete(success && !save_failed_);
  scan_job_observer_.reset();
}

bool ScanService::FilePathSupported(const base::FilePath& file_path) {
  if (file_path == base::FilePath(kActiveUserMyFilesPath) ||
      file_path == my_files_path_ ||
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

}  // namespace chromeos
