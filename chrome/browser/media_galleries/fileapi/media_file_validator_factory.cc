// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/media_file_validator_factory.h"

#include "base/files/file_path.h"
#include "chrome/browser/media_galleries/fileapi/supported_audio_video_checker.h"
#include "chrome/browser/media_galleries/fileapi/supported_image_type_validator.h"
#include "components/download/public/common/quarantine_connection.h"
#include "storage/browser/file_system/copy_or_move_file_validator.h"
#include "storage/browser/file_system/file_system_url.h"

namespace {

class InvalidFileValidator : public storage::CopyOrMoveFileValidator {
 public:
  InvalidFileValidator(const InvalidFileValidator&) = delete;
  InvalidFileValidator& operator=(const InvalidFileValidator&) = delete;

  ~InvalidFileValidator() override {}
  void StartPreWriteValidation(storage::CopyOrMoveFileValidator::ResultCallback
                                   result_callback) override {
    std::move(result_callback).Run(base::File::FILE_ERROR_SECURITY);
  }

  void StartPostWriteValidation(const base::FilePath& dest_platform_path,
                                storage::CopyOrMoveFileValidator::ResultCallback
                                    result_callback) override {
    std::move(result_callback).Run(base::File::FILE_ERROR_SECURITY);
  }

 private:
  friend class ::MediaFileValidatorFactory;

  InvalidFileValidator() {}
};

}  // namespace

MediaFileValidatorFactory::MediaFileValidatorFactory(
    download::QuarantineConnectionCallback quarantine_connection_callback)
    : quarantine_connection_callback_(quarantine_connection_callback) {}
MediaFileValidatorFactory::~MediaFileValidatorFactory() = default;

storage::CopyOrMoveFileValidator*
MediaFileValidatorFactory::CreateCopyOrMoveFileValidator(
    const storage::FileSystemURL& src,
    const base::FilePath& platform_path) {
  base::FilePath src_path = src.virtual_path();
  if (SupportedImageTypeValidator::SupportsFileType(src_path))
    return new SupportedImageTypeValidator(platform_path,
                                           quarantine_connection_callback_);
  if (SupportedAudioVideoChecker::SupportsFileType(src_path))
    return new SupportedAudioVideoChecker(platform_path,
                                          quarantine_connection_callback_);

  return new InvalidFileValidator();
}
