// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/timestamp_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/message_center/views/relative_time_formatter.h"

namespace ash {

TimestampView::TimestampView() = default;

TimestampView::~TimestampView() = default;

void TimestampView::SetTimestamp(base::Time timestamp) {
  timestamp_ = timestamp;
  UpdateTimestampText();
}

void TimestampView::UpdateTimestampText() {
  std::u16string relative_time;
  base::TimeDelta next_update;
  message_center::GetRelativeTimeStringAndNextUpdateTime(
      timestamp_ - base::Time::Now(), &relative_time, &next_update);

  SetText(relative_time);

  // Unretained is safe as the timer cancels the task on destruction.
  timestamp_update_timer_.Start(
      FROM_HERE, next_update,
      base::BindOnce(&TimestampView::UpdateTimestampText,
                     base::Unretained(this)));
}

BEGIN_METADATA(TimestampView)
END_METADATA

}  // namespace ash
