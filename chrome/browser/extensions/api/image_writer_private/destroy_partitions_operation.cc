// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "chrome/browser/extensions/api/image_writer_private/destroy_partitions_operation.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {
namespace image_writer {

// Number of bytes for the maximum partition table size.  GUID partition tables
// reside in the second sector of the disk.  Disks can have up to 4k sectors.
// See http://crbug.com/328246 for more information.
const int kPartitionTableSize = 2 * 4096;

DestroyPartitionsOperation::DestroyPartitionsOperation(
    base::WeakPtr<OperationManager> manager,
    const ExtensionId& extension_id,
    const std::string& storage_unit_id,
    const base::FilePath& download_folder)
    : Operation(manager,
                extension_id,
                storage_unit_id,
                download_folder) {}

DestroyPartitionsOperation::~DestroyPartitionsOperation() {}

void DestroyPartitionsOperation::StartImpl() {
  DCHECK(IsRunningInCorrectSequence());
  if (!base::CreateTemporaryFileInDir(temp_dir_->GetPath(), &image_path_)) {
    Error(error::kTempFileError);
    return;
  }

  std::unique_ptr<char[]> buffer(new char[kPartitionTableSize]);
  memset(buffer.get(), 0, kPartitionTableSize);

  if (base::WriteFile(image_path_, buffer.get(), kPartitionTableSize) !=
      kPartitionTableSize) {
    Error(error::kTempFileError);
    return;
  }

  PostTask(
      base::BindOnce(&DestroyPartitionsOperation::Write, this,
                     base::Bind(&DestroyPartitionsOperation::Finish, this)));
}

}  // namespace image_writer
}  // namespace extensions
