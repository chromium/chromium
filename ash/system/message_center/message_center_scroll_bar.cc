// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/message_center_scroll_bar.h"

#include "ash/constants/ash_features.h"
#include "ash/controls/rounded_scroll_bar.h"
#include "base/metrics/histogram_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/presentation_time_recorder.h"
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

BEGIN_METADATA(RoundedMessageCenterScrollBar, RoundedScrollBar)
END_METADATA

MessageCenterScrollBar::MessageCenterScrollBar(
    MessageCenterScrollBar::Observer* observer)
    : views::OverlayScrollBar(false), observer_(observer) {
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
  return views::OverlayScrollBar::OnKeyPressed(event);
}

bool MessageCenterScrollBar::OnMouseWheel(const ui::MouseWheelEvent& event) {
  if (!stats_recorded_) {
    CollectScrollActionReason(ScrollActionReason::kByMouseWheel);
    stats_recorded_ = true;
  }

  bool result = views::OverlayScrollBar::OnMouseWheel(event);

  if (observer_)
    observer_->OnMessageCenterScrolled();

  return result;
}

const char* MessageCenterScrollBar::GetClassName() const {
  return "MessageCenterScrollBar";
}

void MessageCenterScrollBar::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
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

  if (event->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    if (presentation_time_recorder_)
      presentation_time_recorder_->RequestNext();
  }

  if (event->type() == ui::ET_GESTURE_END)
    presentation_time_recorder_.reset();

  views::OverlayScrollBar::OnGestureEvent(event);

  if (observer_)
    observer_->OnMessageCenterScrolled();
}

bool MessageCenterScrollBar::OnScroll(float dx, float dy) {
  bool result = views::OverlayScrollBar::OnScroll(dx, dy);
  if (observer_)
    observer_->OnMessageCenterScrolled();

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

RoundedMessageCenterScrollBar::RoundedMessageCenterScrollBar(
    MessageCenterScrollBar::Observer* observer)
    : RoundedScrollBar(false), observer_(observer) {
  GetThumb()->layer()->SetVisible(features::IsNotificationScrollBarEnabled());
  GetThumb()->layer()->CompleteAllAnimations();
}

RoundedMessageCenterScrollBar::~RoundedMessageCenterScrollBar() = default;

bool RoundedMessageCenterScrollBar::OnKeyPressed(const ui::KeyEvent& event) {
  if (!stats_recorded_ &&
      (event.key_code() == ui::VKEY_UP || event.key_code() == ui::VKEY_DOWN)) {
    CollectScrollActionReason(ScrollActionReason::kByArrowKey);
    stats_recorded_ = true;
  }
  return RoundedScrollBar::OnKeyPressed(event);
}

bool RoundedMessageCenterScrollBar::OnMouseWheel(
    const ui::MouseWheelEvent& event) {
  if (!stats_recorded_) {
    CollectScrollActionReason(ScrollActionReason::kByMouseWheel);
    stats_recorded_ = true;
  }

  const bool result = RoundedScrollBar::OnMouseWheel(event);

  if (observer_)
    observer_->OnMessageCenterScrolled();

  return result;
}

void RoundedMessageCenterScrollBar::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
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

  if (event->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    if (presentation_time_recorder_)
      presentation_time_recorder_->RequestNext();
  }

  if (event->type() == ui::ET_GESTURE_END)
    presentation_time_recorder_.reset();

  RoundedScrollBar::OnGestureEvent(event);

  if (observer_)
    observer_->OnMessageCenterScrolled();
}

bool RoundedMessageCenterScrollBar::OnScroll(float dx, float dy) {
  const bool result = RoundedScrollBar::OnScroll(dx, dy);
  if (observer_)
    observer_->OnMessageCenterScrolled();

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
