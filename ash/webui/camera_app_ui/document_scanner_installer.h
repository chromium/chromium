// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_CAMERA_APP_UI_DOCUMENT_SCANNER_INSTALLER_H_
#define ASH_WEBUI_CAMERA_APP_UI_DOCUMENT_SCANNER_INSTALLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"

namespace ash {

// Singleton installer for document scanner library via DLC service.
class DocumentScannerInstaller {
 public:
  using LibraryPathCallback =
      base::OnceCallback<void(const std::string& library_path)>;

  static DocumentScannerInstaller* GetInstance();

  DocumentScannerInstaller(const DocumentScannerInstaller&) = delete;
  DocumentScannerInstaller& operator=(const DocumentScannerInstaller&) = delete;
  ~DocumentScannerInstaller();

  void RegisterLibraryPathCallback(LibraryPathCallback callback);

  // It should only be called on the UI thread.
  void TriggerInstall();

 private:
  friend struct base::DefaultSingletonTraits<DocumentScannerInstaller>;

  DocumentScannerInstaller();

  void OnInstalled(const DlcserviceClient::InstallResult& install_result);

  std::string library_path_ GUARDED_BY(library_path_lock_);

  std::vector<LibraryPathCallback> library_path_callbacks_
      GUARDED_BY(library_path_lock_);

  bool installing_ GUARDED_BY(library_path_lock_) = false;

  scoped_refptr<base::SingleThreadTaskRunner> original_task_runner_ = nullptr;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_ = nullptr;

  base::Lock library_path_lock_;

  base::WeakPtrFactory<DocumentScannerInstaller> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_CAMERA_APP_UI_DOCUMENT_SCANNER_INSTALLER_H_
