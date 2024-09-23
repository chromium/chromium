// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_ASH_NOTIFICATION_EXPAND_BUTTON_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_ASH_NOTIFICATION_EXPAND_BUTTON_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/style/counter_expand_button.h"

namespace ash {

// Customized expand button for ash notification view. Used for grouped as
// well as singular notifications.
class AshNotificationExpandButton : public CounterExpandButton {
  METADATA_HEADER(AshNotificationExpandButton, CounterExpandButton)
 public:
  AshNotificationExpandButton();
  AshNotificationExpandButton(const AshNotificationExpandButton&) = delete;
  AshNotificationExpandButton& operator=(const AshNotificationExpandButton&) =
      delete;
  ~AshNotificationExpandButton() override;

  // Perform converting from single to group notification
  // animation. This include bounds change and fade in/out `label_`.
  void AnimateSingleToGroupNotification();

  void SetNotificationTitleForButtonTooltip(
      const std::u16string& notification_title);

  // CounterExpandButton:
  const std::string GetAnimationHistogramName(AnimationType type) override;

 private:
  // CounterExpandButton:
  std::u16string GetExpandedStateTooltipText() const override;
  std::u16string GetCollapsedStateTooltipText() const override;

  // Cache of the notification title. Used this to display in the button
  // tooltip.
  std::u16string notification_title_;
};

BEGIN_VIEW_BUILDER(/*no export*/,
                   AshNotificationExpandButton,
                   CounterExpandButton)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::AshNotificationExpandButton)

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_ASH_NOTIFICATION_EXPAND_BUTTON_H_
