// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/camera_app_ui/document_scanner_installer.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"

namespace ash {

namespace {

constexpr char kDocumentScannerDlcId[] = "cros-camera-document-scanner-dlc";

}  // namespace

// static
DocumentScannerInstaller* DocumentScannerInstaller::GetInstance() {
  return base::Singleton<DocumentScannerInstaller>::get();
}

DocumentScannerInstaller::~DocumentScannerInstaller() = default;

void DocumentScannerInstaller::GetLibraryPath(
    OnGetLibraryPathCallback callback) {
  base::AutoLock auto_lock(library_path_lock_);
  if (library_path_.empty()) {
    get_library_path_callbacks_.push_back(std::move(callback));
    return;
  }
  std::move(callback).Run(library_path_);
}

void DocumentScannerInstaller::TriggerInstall() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  dlcservice::InstallRequest install_request;
  install_request.set_id(kDocumentScannerDlcId);
  chromeos::DlcserviceClient::Get()->Install(
      install_request,
      base::BindOnce(&DocumentScannerInstaller::OnInstalled,
                     base::Unretained(this)),
      base::DoNothing());
}

DocumentScannerInstaller::DocumentScannerInstaller()
    : ui_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

void DocumentScannerInstaller::OnInstalled(
    const chromeos::DlcserviceClient::InstallResult& install_result) {
  base::AutoLock auto_lock(library_path_lock_);
  if (install_result.error == dlcservice::kErrorNone) {
    library_path_ = install_result.root_path;
    for (auto& callback : get_library_path_callbacks_) {
      std::move(callback).Run(library_path_);
    }
    get_library_path_callbacks_.clear();
  } else {
    LOG(ERROR) << "Failed to install document scanner DLC";
  }
}

}  // namespace ash
