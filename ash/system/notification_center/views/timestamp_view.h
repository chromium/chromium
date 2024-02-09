// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_TIMESTAMP_VIEW_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_TIMESTAMP_VIEW_H_

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"

namespace ash {

// A label view which shows a timestamp. The label is automatically updated
// periodically.
class ASH_EXPORT TimestampView : public views::Label {
  METADATA_HEADER(TimestampView, views::Label)

 public:
  TimestampView();
  TimestampView(const TimestampView&) = delete;
  TimestampView& operator=(const TimestampView&) = delete;
  ~TimestampView() override;

  // Sets the base time used to determine the text to be displayed for this
  // timestamp.
  void SetTimestamp(base::Time timestamp);

 private:
  // Updates the currently displayed text that specifies the duration for this
  // timestamp.
  void UpdateTimestampText();

  base::OneShotTimer timestamp_update_timer_;
  base::Time timestamp_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_TIMESTAMP_VIEW_H_
