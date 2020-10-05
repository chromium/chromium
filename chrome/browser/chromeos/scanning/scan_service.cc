// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scanning/scan_service.h"

#include <utility>

#include "base/bind.h"
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

// Path to the user's "My files" folder, relative to the root directory.
constexpr char kMyFilesPath[] = "home/chronos/user/MyFiles";

}  // namespace

ScanService::ScanService(LorgnetteScannerManager* lorgnette_scanner_manager)
    : lorgnette_scanner_manager_(lorgnette_scanner_manager) {
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
  if (scanner_name.empty())
    std::move(callback).Run(false);

  base::Time::Now().UTCExplode(&start_time_);
  save_failed_ = false;

  // TODO(jschettler): Create a TypeConverter to convert from
  // mojo_ipc::ScanSettingsPtr to lorgnette::ScanSettings once the settings are
  // finalized.
  lorgnette::ScanSettings settings_proto;
  settings_proto.set_source_name(settings->source_name);
  lorgnette_scanner_manager_->Scan(
      scanner_name, settings_proto,
      base::BindRepeating(&ScanService::OnPageReceived,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ScanService::OnScanCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ScanService::BindInterface(
    mojo::PendingReceiver<mojo_ipc::ScanService> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void ScanService::SetRootDirForTesting(const base::FilePath& root_dir) {
  root_dir_ = root_dir;
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

void ScanService::OnPageReceived(std::string scanned_image,
                                 uint32_t page_number) {
  // The |page_number| is 0-indexed.
  const std::string filename = base::StringPrintf(
      "scan_%02d%02d%02d-%02d%02d%02d_page_%d.png", start_time_.year,
      start_time_.month, start_time_.day_of_month, start_time_.hour,
      start_time_.minute, start_time_.second, page_number + 1);
  const auto file_path = root_dir_.Append(kMyFilesPath).Append(filename);
  if (!base::WriteFile(file_path, scanned_image)) {
    LOG(ERROR) << "Failed to save scanned image: " << file_path.value().c_str();
    save_failed_ = true;
  }
}

void ScanService::OnScanCompleted(ScanCallback callback, bool success) {
  std::move(callback).Run(success && !save_failed_);
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
