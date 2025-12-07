// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAVE_TO_DRIVE_TIME_REMAINING_CALCULATOR_H_
#define CHROME_BROWSER_SAVE_TO_DRIVE_TIME_REMAINING_CALCULATOR_H_

#include <optional>
#include <string>

#include "base/byte_count.h"
#include "components/download/public/common/rate_estimator.h"

namespace extensions::api::pdf_viewer_private {
struct SaveToDriveProgress;
}  // namespace extensions::api::pdf_viewer_private

namespace save_to_drive {

// This class is used to calculate the time remaining for a save to drive
// operation. It will only calculate the time remaining when the upload has
// started or is in progress.
class TimeRemainingCalculator {
 public:
  TimeRemainingCalculator() = default;
  virtual ~TimeRemainingCalculator();

  // Calculates and returns a human-readable string for the estimated time
  // remaining for the save to drive operation. Returns std::nullopt if the
  // upload speed is zero or progress is invalid. It determines the upload speed
  // by using the `RateEstimator`. This should only be called when the progress
  // status is `kUploadStarted` or `kUploadInProgress`.
  virtual std::optional<std::u16string> CalculateTimeRemainingText(
      const extensions::api::pdf_viewer_private::SaveToDriveProgress& progress);

 private:
  // Returns the remaining time for the save to drive operation.
  std::optional<base::TimeDelta> GetRemainingTime(
      const base::ByteCount& uploaded_bytes,
      const base::ByteCount& file_size_bytes) const;

  // The last uploaded bytes.
  base::ByteCount last_uploaded_bytes_;
  // Used to get a smooth estimate of the upload speed.
  download::RateEstimator upload_speed_estimator_;
};

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_SAVE_TO_DRIVE_TIME_REMAINING_CALCULATOR_H_
