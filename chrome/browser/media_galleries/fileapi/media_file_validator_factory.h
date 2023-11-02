// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_FILE_VALIDATOR_FACTORY_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_FILE_VALIDATOR_FACTORY_H_

#include "components/download/public/common/quarantine_connection.h"
#include "storage/browser/file_system/copy_or_move_file_validator.h"

namespace base {
class FilePath;
}

namespace storage {
class FileSystemURL;
}

// A factory for media file validators. A media file validator will use various
// strategies (depending on the file type) to attempt to verify that the file
// is a valid media file.
class MediaFileValidatorFactory
    : public storage::CopyOrMoveFileValidatorFactory {
 public:
  explicit MediaFileValidatorFactory(
      download::QuarantineConnectionCallback quarantine_connection_callback);

  MediaFileValidatorFactory(const MediaFileValidatorFactory&) = delete;
  MediaFileValidatorFactory& operator=(const MediaFileValidatorFactory&) =
      delete;

  ~MediaFileValidatorFactory() override;

  // CopyOrMoveFileValidatorFactory implementation.
  storage::CopyOrMoveFileValidator* CreateCopyOrMoveFileValidator(
      const storage::FileSystemURL& src,
      const base::FilePath& platform_path) override;

 private:
  download::QuarantineConnectionCallback quarantine_connection_callback_;
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_FILE_VALIDATOR_FACTORY_H_
