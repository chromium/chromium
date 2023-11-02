// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_FILE_MANAGER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_FILE_MANAGER_ASH_H_

#include "chromeos/crosapi/mojom/file_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the crosapi file manager interface. Lives in ash-chrome on the UI
// thread. Allows lacros-chrome to make UI requests to the Chrome OS file
// manager, for example to open a folder or highlight a file.
class FileManagerAsh : public mojom::FileManager {
 public:
  FileManagerAsh();
  FileManagerAsh(const FileManagerAsh&) = delete;
  FileManagerAsh& operator=(const FileManagerAsh&) = delete;
  ~FileManagerAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::FileManager> receiver);

  // crosapi::mojom::FileManager:
  void DeprecatedShowItemInFolder(const base::FilePath& path) override;
  void ShowItemInFolder(const base::FilePath& path,
                        ShowItemInFolderCallback callback) override;
  void OpenFolder(const base::FilePath& path,
                  OpenFolderCallback callback) override;
  void OpenFile(const base::FilePath& path, OpenFileCallback callback) override;

 private:
  mojo::ReceiverSet<mojom::FileManager> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_FILE_MANAGER_ASH_H_
