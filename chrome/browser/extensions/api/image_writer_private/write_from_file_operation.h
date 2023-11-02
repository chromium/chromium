// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_WRITE_FROM_FILE_OPERATION_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_WRITE_FROM_FILE_OPERATION_H_

#include "chrome/browser/extensions/api/image_writer_private/operation.h"

namespace extensions {
namespace image_writer {

// Encapsulates a write of an image from a local file.
class WriteFromFileOperation : public Operation {
 public:
  WriteFromFileOperation(base::WeakPtr<OperationManager> manager,
                         const ExtensionId& extension_id,
                         const base::FilePath& user_file_path,
                         const std::string& storage_unit_id,
                         const base::FilePath& download_folder);
  void StartImpl() override;

 private:
  ~WriteFromFileOperation() override;
};

}  // namespace image_writer
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_WRITE_FROM_FILE_OPERATION_H_
