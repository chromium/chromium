// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_MANAGER_UMA_HELPER_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_MANAGER_UMA_HELPER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"

namespace base {
class TickClock;
}  // namespace base

class PictureInPictureWindowManagerUmaHelper {
 public:
  PictureInPictureWindowManagerUmaHelper();
  ~PictureInPictureWindowManagerUmaHelper() = default;

  // Records the total time spent on a picture in picture window, regardless of
  // the Picture-in-Picture window type (document vs video) and the reason for
  // closing the window (UI interaction, returning back to opener tab, etc.).
  //
  // The resulting histogram is configured to allow analyzing closures that take
  // place within a short period of time, to account for user reaction time
  // (~273 ms).
  void MaybeRecordPictureInPictureChanged(bool is_picture_in_picture);

  void SetClockForTest(const base::TickClock* testing_clock);

 private:
  std::optional<base::TimeTicks> current_enter_pip_time_ = std::nullopt;
  raw_ptr<const base::TickClock> clock_ = nullptr;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_MANAGER_UMA_HELPER_H_
