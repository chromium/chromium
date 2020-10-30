// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scanning/scan_service.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/check.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/scanning/lorgnette_scanner_manager.h"
#include "chrome/browser/chromeos/scanning/scanning_type_converters.h"

namespace chromeos {

namespace {

namespace mojo_ipc = scanning::mojom;

// Path to the user's "My files" folder.
constexpr char kMyFilesPath[] = "/home/chronos/user/MyFiles";

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

void ScanService::Scan(const base::UnguessableToken& scanner_id,
                       mojo_ipc::ScanSettingsPtr settings,
                       ScanCallback callback) {
  const std::string scanner_name = GetScannerName(scanner_id);
  if (scanner_name.empty() || !FilePathSupported(settings->scan_to_path)) {
    std::move(callback).Run(false);
    return;
  }

  base::Time::Now().LocalExplode(&start_time_);
  save_failed_ = false;
  lorgnette_scanner_manager_->Scan(
      scanner_name, mojo::ConvertTo<lorgnette::ScanSettings>(settings),
      base::NullCallback(),
      base::BindRepeating(&ScanService::OnPageReceived,
                          weak_ptr_factory_.GetWeakPtr(),
                          settings->scan_to_path, settings->file_type),
      base::BindOnce(&ScanService::OnScanCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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

void ScanService::OnPageReceived(const base::FilePath& scan_to_path,
                                 const mojo_ipc::FileType file_type,
                                 std::string scanned_image,
                                 uint32_t page_number) {
  // TODO(jschettler): Add support for converting the scanned image to other
  // file types.
  if (file_type != mojo_ipc::FileType::kPng) {
    LOG(ERROR) << "Selected file type not supported.";
    save_failed_ = true;
    return;
  }

  // The |page_number| is 0-indexed.
  const std::string filename = base::StringPrintf(
      "scan_%02d%02d%02d-%02d%02d%02d_%d.png", start_time_.year,
      start_time_.month, start_time_.day_of_month, start_time_.hour,
      start_time_.minute, start_time_.second, page_number + 1);
  const auto file_path = scan_to_path.Append(filename);
  if (!base::WriteFile(file_path, scanned_image)) {
    LOG(ERROR) << "Failed to save scanned image: " << file_path.value().c_str();
    save_failed_ = true;
  }
}

void ScanService::OnScanCompleted(ScanCallback callback, bool success) {
  std::move(callback).Run(success && !save_failed_);
}

bool ScanService::FilePathSupported(const base::FilePath& file_path) {
  // TODO(jschettler): Remove this check once the path is selected by the user.
  if (file_path == base::FilePath(kMyFilesPath))
    return true;

  if (!file_path.ReferencesParent() &&
      (file_path == my_files_path_ || my_files_path_.IsParent(file_path) ||
       google_drive_path_.IsParent(file_path))) {
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
