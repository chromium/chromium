// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/file_operation_proxy_impl.h"

#include "base/files/file_util.h"

namespace on_device_translation {

FileOperationProxyImpl::FileOperationProxyImpl(
    mojo::PendingReceiver<FileOperationProxy> proxy_receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::vector<base::FilePath> package_paths)
    : receiver_(this, std::move(proxy_receiver), task_runner),
      package_paths_(std::move(package_paths)) {}

FileOperationProxyImpl::~FileOperationProxyImpl() = default;

void FileOperationProxyImpl::FileExists(uint32_t package_index,
                                        const base::FilePath& relative_path,
                                        FileExistsCallback callback) {
  const base::FilePath file_path = GetFilePath(package_index, relative_path);
  if (file_path.empty()) {
    // Invalid `path` was passed.
    // Calling `ReportBadMessage` to crash the service, because this is
    // likely caused by a compromised service. We check the validity of the
    // `path` before passing it from the service in translate_kit_client.cc.
    receiver_.ReportBadMessage("Invalid `path` was passed.");
    return;
  }
  if (!base::PathExists(file_path)) {
    // File doesn't exist.
    std::move(callback).Run(/*exists=*/false, /*is_directory=*/false);
    return;
  }
  std::move(callback).Run(
      /*exists=*/true,
      /*is_directory=*/base::DirectoryExists(file_path));
}

void FileOperationProxyImpl::Open(uint32_t package_index,
                                  const base::FilePath& relative_path,
                                  OpenCallback callback) {
  const base::FilePath file_path = GetFilePath(package_index, relative_path);
  if (file_path.empty()) {
    // Invalid `path` was passed.
    // Calling `ReportBadMessage` to crash the service, because this is
    // likely caused by a compromised service. We check the validity of the
    // `path` before passing it from the service in translate_kit_client.cc.
    receiver_.ReportBadMessage("Invalid `path` was passed.");
    return;
  }
  // Sends a file object only if the file exists and is not a directory.
  std::move(callback).Run(
      base::PathExists(file_path) && !base::DirectoryExists(file_path)
          ? base::File(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ)
          : base::File());
}

base::FilePath FileOperationProxyImpl::GetFilePath(
    uint32_t package_index,
    const base::FilePath& relative_path) const {
  if (package_index >= package_paths_.size()) {
    // Invalid package index.
    return base::FilePath();
  }
  if (relative_path.IsAbsolute() || relative_path.ReferencesParent()) {
    // Invalid relative path.
    return base::FilePath();
  }
  return package_paths_[package_index].Append(relative_path);
}

}  // namespace on_device_translation
