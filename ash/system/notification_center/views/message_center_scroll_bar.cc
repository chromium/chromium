// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/message_center_scroll_bar.h"

#include "ash/constants/ash_features.h"
#include "ash/controls/rounded_scroll_bar.h"
#include "base/metrics/histogram_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"
#include "ui/views/widget/widget.h"

namespace {

// The UMA histogram that records presentation time for scrolling through the
// notification list in message center.
constexpr char kMessageCenterScrollHistogram[] =
    "Ash.MessageCenter.Scroll.PresentationTime";

// The UMA histogram that records max latency of presentation time for scrolling
// through the notification list in message center.
constexpr char kMessageCenterScrollMaxLatencyHistogram[] =
    "Ash.MessageCenter.Scroll.PresentationTime.MaxLatency";

enum class ScrollActionReason {
  kUnknown,
  kByMouseWheel,
  kByTouch,
  kByArrowKey,
  kCount,
};

void CollectScrollActionReason(ScrollActionReason reason) {
  UMA_HISTOGRAM_ENUMERATION("ChromeOS.MessageCenter.ScrollActionReason", reason,
                            ScrollActionReason::kCount);
}

}  // namespace

namespace ash {

BEGIN_METADATA(MessageCenterScrollBar)
END_METADATA

MessageCenterScrollBar::MessageCenterScrollBar()
    : RoundedScrollBar(views::ScrollBar::Orientation::kVertical) {
  GetThumb()->layer()->SetVisible(features::IsNotificationScrollBarEnabled());
  GetThumb()->layer()->CompleteAllAnimations();
}

MessageCenterScrollBar::~MessageCenterScrollBar() = default;

bool MessageCenterScrollBar::OnKeyPressed(const ui::KeyEvent& event) {
  if (!stats_recorded_ &&
      (event.key_code() == ui::VKEY_UP || event.key_code() == ui::VKEY_DOWN)) {
    CollectScrollActionReason(ScrollActionReason::kByArrowKey);
    stats_recorded_ = true;
  }
  return RoundedScrollBar::OnKeyPressed(event);
}

bool MessageCenterScrollBar::OnMouseWheel(const ui::MouseWheelEvent& event) {
  if (!stats_recorded_) {
    CollectScrollActionReason(ScrollActionReason::kByMouseWheel);
    stats_recorded_ = true;
  }

  const bool result = RoundedScrollBar::OnMouseWheel(event);

  return result;
}

void MessageCenterScrollBar::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureScrollBegin) {
    if (!presentation_time_recorder_ && GetWidget()) {
      presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
          GetWidget()->GetCompositor(), kMessageCenterScrollHistogram,
          kMessageCenterScrollMaxLatencyHistogram);
    }
    if (!stats_recorded_) {
      CollectScrollActionReason(ScrollActionReason::kByTouch);
      stats_recorded_ = true;
    }
  }

  if (event->type() == ui::EventType::kGestureScrollUpdate) {
    if (presentation_time_recorder_)
      presentation_time_recorder_->RequestNext();
  }

  if (event->type() == ui::EventType::kGestureEnd) {
    presentation_time_recorder_.reset();
  }

  RoundedScrollBar::OnGestureEvent(event);
}

bool MessageCenterScrollBar::OnScroll(float dx, float dy) {
  const bool result = RoundedScrollBar::OnScroll(dx, dy);
  // Widget might be null in tests.
  if (GetWidget() && !presentation_time_recorder_) {
    // Create a recorder if needed. We stop and record metrics when the
    // object goes out of scope (when message center is closed).
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        GetWidget()->GetCompositor(), kMessageCenterScrollHistogram,
        kMessageCenterScrollMaxLatencyHistogram);
  }
  if (presentation_time_recorder_)
    presentation_time_recorder_->RequestNext();

  return result;
}

}  // namespace ash
