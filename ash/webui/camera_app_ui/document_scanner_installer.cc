// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/camera_app_ui/document_scanner_installer.h"

#include "ash/webui/camera_app_ui/document_scanner_service_client.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

namespace {

constexpr char kDocumentScannerDlcId[] = "cros-camera-document-scanner-dlc";

}  // namespace

// static
DocumentScannerInstaller* DocumentScannerInstaller::GetInstance() {
  return base::Singleton<DocumentScannerInstaller>::get();
}

DocumentScannerInstaller::~DocumentScannerInstaller() = default;

void DocumentScannerInstaller::RegisterLibraryPathCallback(
    LibraryPathCallback callback) {
  base::AutoLock auto_lock(library_path_lock_);
  if (library_path_.empty()) {
    if (!installing_) {
      ui_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&DocumentScannerInstaller::TriggerInstall,
                                    base::Unretained(this)));
    }
    library_path_callbacks_.push_back(std::move(callback));
  } else {
    std::move(callback).Run(library_path_);
  }
}

void DocumentScannerInstaller::TriggerInstall() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  if (!DocumentScannerServiceClient::IsSupportedByDlc()) {
    return;
  }

  base::AutoLock auto_lock(library_path_lock_);
  if (installing_) {
    return;
  }
  installing_ = true;

  dlcservice::InstallRequest install_request;
  install_request.set_id(kDocumentScannerDlcId);
  DlcserviceClient::Get()->Install(
      install_request,
      base::BindOnce(&DocumentScannerInstaller::OnInstalled,
                     base::Unretained(this)),
      base::DoNothing());
}

DocumentScannerInstaller::DocumentScannerInstaller()
    : ui_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

void DocumentScannerInstaller::OnInstalled(
    const DlcserviceClient::InstallResult& install_result) {
  base::AutoLock auto_lock(library_path_lock_);
  if (install_result.error == dlcservice::kErrorNone) {
    library_path_ = install_result.root_path;
  } else {
    LOG(ERROR) << "Failed to install document scanner DLC: "
               << install_result.error;
  }
  for (auto& callback : library_path_callbacks_) {
    std::move(callback).Run(library_path_);
  }
  library_path_callbacks_.clear();
  installing_ = false;
}

}  // namespace ash
