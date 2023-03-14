// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CRASH_UPLOAD_LIST_CRASH_UPLOAD_LIST_CHROMEOS_H_
#define CHROME_BROWSER_CRASH_UPLOAD_LIST_CRASH_UPLOAD_LIST_CHROMEOS_H_

#include "components/upload_list/text_log_upload_list.h"

#include <memory>

// An UploadList that retrieves the list of crash reports available on the
// client. This includes fatal_crash_type, a field that is specific to ChromeOS.
class CrashUploadListChromeOS : public TextLogUploadList {
 public:
  class CrashUploadInfo : public TextLogUploadList::UploadInfo {
   public:
    explicit CrashUploadInfo(TextLogUploadList::UploadInfo&&);
    enum class FatalCrashType { Unknown = 0, Kernel, EmbeddedController };
    // The type of the fatal crash. Unknown if either the field
    // "fatal_crash_type" is missing from the uploads.log entry, or the field
    // contains a string that isn't recognized.
    FatalCrashType fatal_crash_type = FatalCrashType::Unknown;
  };

  explicit CrashUploadListChromeOS(const base::FilePath& upload_log_path);

  CrashUploadListChromeOS(const CrashUploadListChromeOS&) = delete;
  CrashUploadListChromeOS& operator=(const CrashUploadListChromeOS&) = delete;

 protected:
  ~CrashUploadListChromeOS() override;

  // TextLogUploadList::
  std::unique_ptr<UploadList::UploadInfo> TryParseJsonLogEntry(
      const base::Value::Dict& dict) override;
};

#endif  // CHROME_BROWSER_CRASH_UPLOAD_LIST_CRASH_UPLOAD_LIST_CHROMEOS_H_
