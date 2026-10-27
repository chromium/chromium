// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_DESTROY_PARTITIONS_OPERATION_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_DESTROY_PARTITIONS_OPERATION_H_

#include <stddef.h>

#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/extensions/api/image_writer_private/operation.h"

namespace extensions {
namespace image_writer {

// Number of bytes for the maximum partition table size. GUID partition tables
// reside in the second sector of the disk. Disks can have up to 4k sectors.
// See http://crbug.com/41079977 for more information.
inline constexpr size_t kPartitionTableSize = 2 * 4096;

// Encapsulates an operation for destroying partitions.  This is achieved by
// creating a dummy blank image which is then burned to the disk.
class DestroyPartitionsOperation : public Operation {
 public:
  DestroyPartitionsOperation(base::WeakPtr<OperationManager> manager,
                             const ExtensionId& extension_id,
                             const std::string& storage_unit_id,
                             const base::FilePath& download_folder);
  void StartImpl() override;

 protected:
  bool ShouldWipeBackupGpt() const override;

 private:
  ~DestroyPartitionsOperation() override;
};

}  // namespace image_writer
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_DESTROY_PARTITIONS_OPERATION_H_
