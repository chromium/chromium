// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/ash_notification_expand_button.h"

#include <string>

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

}  // namespace

BEGIN_METADATA(AshNotificationExpandButton)
END_METADATA

AshNotificationExpandButton::AshNotificationExpandButton() = default;
AshNotificationExpandButton::~AshNotificationExpandButton() = default;

void AshNotificationExpandButton::AnimateSingleToGroupNotification() {
  message_center_utils::FadeInView(
      label(), /*delay_in_ms=*/0, kConvertFromSingleToGroupFadeInDurationMs,
      gfx::Tween::LINEAR,
      "Ash.NotificationView.ExpandButton.ConvertSingleToGroup.FadeIn."
      "AnimationSmoothness");

  AnimateBoundsChange(
      kConvertFromSingleToGroupBoundsChangeDurationMs,
      gfx::Tween::ACCEL_20_DECEL_100,
      "Ash.NotificationView.ExpandButton.ConvertSingleToGroup.BoundsChange."
      "AnimationSmoothness");
}

void AshNotificationExpandButton::SetNotificationTitleForButtonTooltip(
    const std::u16string& notification_title) {
  if (notification_title_ == notification_title) {
    return;
  }
  notification_title_ = notification_title;
  UpdateTooltip();
}

const std::string AshNotificationExpandButton::GetAnimationHistogramName(
    AnimationType type) {
  switch (type) {
    case AnimationType::kFadeInLabel:
      return "Ash.NotificationView.ExpandButtonLabel.FadeIn."
             "AnimationSmoothness";
    case AnimationType::kFadeOutLabel:
      return "Ash.NotificationView.ExpandButtonLabel.FadeOut."
             "AnimationSmoothness";
    case AnimationType::kBoundsChange:
      return "Ash.NotificationView.ExpandButton.BoundsChange."
             "AnimationSmoothness";
  }
}

std::u16string AshNotificationExpandButton::GetExpandedStateTooltipText()
    const {
  // The tooltip tells users that clicking on the button will collapse the
  // notification group.
  return l10n_util::GetStringFUTF16(IDS_ASH_NOTIFICATION_COLLAPSE_TOOLTIP,
                                    notification_title_);
}

std::u16string AshNotificationExpandButton::GetCollapsedStateTooltipText()
    const {
  // The tooltip tells users that clicking on the button will expand the
  // notification group.
  return l10n_util::GetStringFUTF16(IDS_ASH_NOTIFICATION_EXPAND_TOOLTIP,
                                    notification_title_);
}

}  // namespace ash
