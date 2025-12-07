// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_RECORDER_H_
#define CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_RECORDER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

class Profile;

namespace extensions::api::pdf_viewer_private {
struct SaveToDriveProgress;
}  // namespace extensions::api::pdf_viewer_private

namespace save_to_drive {

// This class is used to record UMA metrics for Save to Drive events.
class SaveToDriveRecorder {
 public:
  explicit SaveToDriveRecorder(Profile* profile);
  virtual ~SaveToDriveRecorder();
  SaveToDriveRecorder(const SaveToDriveRecorder&) = delete;
  SaveToDriveRecorder& operator=(const SaveToDriveRecorder&) = delete;

  // Records UMA metrics for the given `progress`.
  virtual void Record(
      const extensions::api::pdf_viewer_private::SaveToDriveProgress& progress);

 private:
  // The time that the upload started. This is set when the status is
  // `kUploadStarted`.
  std::optional<base::TimeTicks> start_time_;
  raw_ptr<Profile> profile_;
};

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_RECORDER_H_
