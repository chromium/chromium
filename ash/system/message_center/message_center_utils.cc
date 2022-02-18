// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/message_center_utils.h"

#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/vm_camera_mic_constants.h"
#include "base/metrics/histogram_functions.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/message_center/message_center.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/view.h"

namespace {

void ReportAnimationSmoothness(const std::string& animation_histogram_name,
                               int smoothness) {
  // Record animation smoothness if `animation_histogram_name` is given.
  if (!animation_histogram_name.empty())
    base::UmaHistogramPercentage(animation_histogram_name, smoothness);
}

}  // namespace

namespace ash {

namespace message_center_utils {

bool CompareNotifications(message_center::Notification* n1,
                          message_center::Notification* n2) {
  if (n1->pinned() && !n2->pinned())
    return true;
  if (!n1->pinned() && n2->pinned())
    return false;
  return message_center::CompareTimestampSerial()(n1, n2);
}

std::vector<message_center::Notification*> GetSortedNotificationsWithOwnView() {
  auto visible_notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  std::vector<message_center::Notification*> sorted_notifications;
  std::copy_if(visible_notifications.begin(), visible_notifications.end(),
               std::back_inserter(sorted_notifications),
               [](message_center::Notification* notification) {
                 return !notification->group_child();
               });
  std::sort(sorted_notifications.begin(), sorted_notifications.end(),
            CompareNotifications);
  return sorted_notifications;
}

size_t GetNotificationCount() {
  size_t count = 0;
  for (message_center::Notification* notification :
       message_center::MessageCenter::Get()->GetVisibleNotifications()) {
    const std::string& notifier = notification->notifier_id().id;
    // Don't count these notifications since we have `CameraMicTrayItemView` to
    // show indicators on the systray.
    if (notifier == kVmCameraMicNotifierId)
      continue;

    // Don't count group child notifications since they're contained in a single
    // parent view.
    if (notification->group_child())
      continue;

    ++count;
  }
  return count;
}

void InitLayerForAnimations(views::View* view) {
  view->SetPaintToLayer();
  view->layer()->SetFillsBoundsOpaquely(false);
}

void FadeInView(views::View* view,
                int delay_in_ms,
                int duration_in_ms,
                gfx::Tween::Type tween_type,
                const std::string& animation_histogram_name) {
  // If we are in testing with animation (non zero duration), we shouldn't have
  // delays so that we can properly track when animation is completed in test.
  if (ui::ScopedAnimationDurationScaleMode::duration_multiplier() ==
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION) {
    delay_in_ms = 0;
  }

  // The view must have a layer to perform animation.
  DCHECK(view->layer());

  ui::AnimationThroughputReporter reporter(
      view->layer()->GetAnimator(),
      metrics_util::ForSmoothness(base::BindRepeating(
          &ReportAnimationSmoothness, animation_histogram_name)));

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::TimeDelta())
      .SetOpacity(view, 0.0f)
      .At(base::Milliseconds(delay_in_ms))
      .SetDuration(base::Milliseconds(duration_in_ms))
      .SetOpacity(view, 1.0f, tween_type);
}

void FadeOutView(views::View* view,
                 base::OnceClosure on_animation_ended,
                 int delay_in_ms,
                 int duration_in_ms,
                 gfx::Tween::Type tween_type,
                 const std::string& animation_histogram_name) {
  // If we are in testing with animation (non zero duration), we shouldn't have
  // delays so that we can properly track when animation is completed in test.
  if (ui::ScopedAnimationDurationScaleMode::duration_multiplier() ==
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION) {
    delay_in_ms = 0;
  }

  std::pair<base::OnceClosure, base::OnceClosure> split =
      base::SplitOnceCallback(std::move(on_animation_ended));

  // The view must have a layer to perform animation.
  DCHECK(view->layer());

  ui::AnimationThroughputReporter reporter(
      view->layer()->GetAnimator(),
      metrics_util::ForSmoothness(base::BindRepeating(
          &ReportAnimationSmoothness, animation_histogram_name)));

  view->SetVisible(true);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(std::move(split.first))
      .OnAborted(std::move(split.second))
      .Once()
      .At(base::Milliseconds(delay_in_ms))
      .SetDuration(base::Milliseconds(duration_in_ms))
      .SetVisibility(view, false)
      .SetOpacity(view, 0.0f, tween_type);
}

}  // namespace message_center_utils

}  // namespace ash
