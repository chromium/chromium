// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/destroy_partitions_operation.h"

#include <string.h>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {
namespace image_writer {

DestroyPartitionsOperation::DestroyPartitionsOperation(
    base::WeakPtr<OperationManager> manager,
    const ExtensionId& extension_id,
    const std::string& storage_unit_id,
    const base::FilePath& download_folder)
    : Operation(manager, extension_id, storage_unit_id, download_folder) {}

DestroyPartitionsOperation::~DestroyPartitionsOperation() = default;

bool DestroyPartitionsOperation::ShouldWipeBackupGpt() const {
  return true;
}

void DestroyPartitionsOperation::StartImpl() {
  CHECK(IsRunningInCorrectSequence(), base::NotFatalUntil::M161);
  if (!base::CreateTemporaryFileInDir(temp_dir_->GetPath(), &image_path_)) {
    Error(error::kTempFileError);
    return;
  }

  std::vector<uint8_t> buffer(kPartitionTableSize, 0);
  if (!base::WriteFile(image_path_, buffer)) {
    Error(error::kTempFileError);
    return;
  }

  PostTask(base::BindOnce(
      &DestroyPartitionsOperation::Write, this,
      base::BindOnce(&DestroyPartitionsOperation::Finish, this)));
}

}  // namespace image_writer
}  // namespace extensions
