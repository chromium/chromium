// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SUPPORTED_AUDIO_VIDEO_CHECKER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SUPPORTED_AUDIO_VIDEO_CHECKER_H_

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media_galleries/fileapi/av_scanning_file_validator.h"
#include "components/download/public/common/quarantine_connection.h"

class MediaFileValidatorFactory;
class SafeAudioVideoChecker;

// Uses SafeAudioVideoChecker to validate supported audio and video files in
// the utility process and then uses AVScanningFileValidator to ask the OS to
// virus scan them. The entire file is not decoded so a positive result from
// this class does not make the file safe to use in the browser process.
class SupportedAudioVideoChecker : public AVScanningFileValidator {
 public:
  SupportedAudioVideoChecker(const SupportedAudioVideoChecker&) = delete;
  SupportedAudioVideoChecker& operator=(const SupportedAudioVideoChecker&) =
      delete;

  ~SupportedAudioVideoChecker() override;

  static bool SupportsFileType(const base::FilePath& path);

  void StartPreWriteValidation(ResultCallback result_callback) override;

 private:
  friend class MediaFileValidatorFactory;

  SupportedAudioVideoChecker(
      const base::FilePath& file,
      download::QuarantineConnectionCallback quarantine_connection_callback);

  void OnFileOpen(base::File file);

  base::FilePath path_;
  storage::CopyOrMoveFileValidator::ResultCallback callback_;
  std::unique_ptr<SafeAudioVideoChecker> safe_checker_;
  base::WeakPtrFactory<SupportedAudioVideoChecker> weak_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SUPPORTED_AUDIO_VIDEO_CHECKER_H_
