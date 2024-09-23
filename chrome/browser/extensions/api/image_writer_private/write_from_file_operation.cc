// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/write_from_file_operation.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {
namespace image_writer {

using content::BrowserThread;

WriteFromFileOperation::WriteFromFileOperation(
    base::WeakPtr<OperationManager> manager,
    const ExtensionId& extension_id,
    const base::FilePath& user_file_path,
    const std::string& device_path,
    const base::FilePath& download_folder)
    : Operation(manager, extension_id, device_path, download_folder) {
  image_path_ = user_file_path;
}

WriteFromFileOperation::~WriteFromFileOperation() {}

void WriteFromFileOperation::StartImpl() {
  DCHECK(IsRunningInCorrectSequence());
  if (!base::PathExists(image_path_) || base::DirectoryExists(image_path_)) {
    DLOG(ERROR) << "Source must exist and not be a directory.";
    Error(error::kImageInvalid);
    return;
  }

  PostTask(base::BindOnce(
      &WriteFromFileOperation::Extract, this,
      base::BindOnce(
          &WriteFromFileOperation::Write, this,
          base::BindOnce(
              &WriteFromFileOperation::VerifyWrite, this,
              base::BindOnce(&WriteFromFileOperation::Finish, this)))));
}

}  // namespace image_writer
}  // namespace extensions
