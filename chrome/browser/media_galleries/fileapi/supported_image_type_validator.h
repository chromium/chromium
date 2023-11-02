// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SUPPORTED_IMAGE_TYPE_VALIDATOR_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SUPPORTED_IMAGE_TYPE_VALIDATOR_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media_galleries/fileapi/av_scanning_file_validator.h"
#include "components/download/public/common/quarantine_connection.h"

class ImageDecoder;

class MediaFileValidatorFactory;

// Use ImageDecoder to determine if the file decodes without error. Handles
// image files supported by Chrome.
class SupportedImageTypeValidator : public AVScanningFileValidator {
 public:
  SupportedImageTypeValidator(const SupportedImageTypeValidator&) = delete;
  SupportedImageTypeValidator& operator=(const SupportedImageTypeValidator&) =
      delete;

  ~SupportedImageTypeValidator() override;

  static bool SupportsFileType(const base::FilePath& path);

  void StartPreWriteValidation(ResultCallback result_callback) override;

 private:
  friend class MediaFileValidatorFactory;

  SupportedImageTypeValidator(
      const base::FilePath& file,
      download::QuarantineConnectionCallback quarantine_connection_callback);

  void OnFileOpen(std::unique_ptr<std::string> data);

  base::FilePath path_;
  storage::CopyOrMoveFileValidator::ResultCallback callback_;
  base::WeakPtrFactory<SupportedImageTypeValidator> weak_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SUPPORTED_IMAGE_TYPE_VALIDATOR_H_
