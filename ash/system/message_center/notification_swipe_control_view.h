// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_NOTIFICATION_SWIPE_CONTROL_VIEW_H_
#define ASH_SYSTEM_MESSAGE_CENTER_NOTIFICATION_SWIPE_CONTROL_VIEW_H_

#include "ash/ash_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"

namespace message_center {
class MessageView;
}  // namespace message_center

namespace ash {

// View containing 2 buttons that appears behind notification by swiping.
class ASH_EXPORT NotificationSwipeControlView : public views::View {
 public:
  // Physical positions to show buttons in the swipe control. This is invariant
  // across RTL/LTR languages because buttons should be shown on one side which
  // is made uncovered by the overlapping view after user's swipe action.
  enum class ButtonPosition { RIGHT, LEFT };

  // String to be returned by GetClassName() method.
  static const char kViewClassName[];

  explicit NotificationSwipeControlView(
      message_center::MessageView* message_view);
  NotificationSwipeControlView(const NotificationSwipeControlView&) = delete;
  NotificationSwipeControlView& operator=(const NotificationSwipeControlView&) =
      delete;
  ~NotificationSwipeControlView() override;

  // views::View
  const char* GetClassName() const override;

  // Update the visibility of control buttons.
  void UpdateButtonsVisibility();

  // Update the radii of background corners.
  void UpdateCornerRadius(int top_radius, int bottom_radius);

 private:
  FRIEND_TEST_ALL_PREFIXES(NotificationSwipeControlViewTest,
                           DeleteOnSettingsButtonPressed);
  FRIEND_TEST_ALL_PREFIXES(NotificationSwipeControlViewTest,
                           DeleteOnSnoozeButtonPressed);
  FRIEND_TEST_ALL_PREFIXES(NotificationSwipeControlViewTest,
                           SettingsButtonVisibility);
  enum class ButtonId {
    kSettings,
    kSnooze,
  };

  // Change the visibility of the settings and snooze button.
  void ShowButtons(ButtonPosition button_position,
                   bool show_settings,
                   bool show_snooze);
  void HideButtons();

  // Change the visibility of the settings button. True to show, false to hide.
  void ShowSettingsButton(bool show);

  // Change the visibility of the snooze button. True to show, false to hide.
  void ShowSnoozeButton(bool show);

  void ButtonPressed(ButtonId button, const ui::Event& event);

  message_center::MessageView* const message_view_;

  // Owned by views hierarchy.
  views::ImageButton* settings_button_ = nullptr;
  views::ImageButton* snooze_button_ = nullptr;

  base::WeakPtrFactory<NotificationSwipeControlView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_NOTIFICATION_SWIPE_CONTROL_VIEW_H_
