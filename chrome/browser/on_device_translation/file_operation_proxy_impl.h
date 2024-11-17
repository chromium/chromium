// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_FILE_OPERATION_PROXY_IMPL_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_FILE_OPERATION_PROXY_IMPL_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace on_device_translation {

// Implementation of FileOperationProxy. It is used to provide file operations
// to the OnDeviceTranslationService. This is created on the UI thread and
// destroyed on the background thread of the passed `task_runner`.
class FileOperationProxyImpl : public mojom::FileOperationProxy {
 public:
  FileOperationProxyImpl(
      mojo::PendingReceiver<mojom::FileOperationProxy> proxy_receiver,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::vector<base::FilePath> package_paths);
  ~FileOperationProxyImpl() override;

  // FileOperationProxy implementation:
  void FileExists(uint32_t package_index,
                  const base::FilePath& relative_path,
                  FileExistsCallback callback) override;
  void Open(uint32_t package_index,
            const base::FilePath& relative_path,
            OpenCallback callback) override;

 private:
  // Get the file path for the given `package_index` and `relative_path`.
  // Returns an empty path if the `package_index` is invalid or the
  // relative_path` is invalid.
  base::FilePath GetFilePath(uint32_t package_index,
                             const base::FilePath& relative_path) const;

  mojo::Receiver<FileOperationProxy> receiver_{this};
  std::vector<base::FilePath> package_paths_;
};

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_FILE_OPERATION_PROXY_IMPL_H_
