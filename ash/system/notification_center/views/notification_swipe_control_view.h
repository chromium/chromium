// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_NOTIFICATION_SWIPE_CONTROL_VIEW_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_NOTIFICATION_SWIPE_CONTROL_VIEW_H_

#include "ash/ash_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"

namespace message_center {
class MessageView;
}  // namespace message_center

namespace ash {

// View containing the settings button that appears behind notification by
// swiping.
class ASH_EXPORT NotificationSwipeControlView : public views::View {
  METADATA_HEADER(NotificationSwipeControlView, views::View)

 public:
  // Physical positions to show buttons in the swipe control. This is invariant
  // across RTL/LTR languages because buttons should be shown on one side which
  // is made uncovered by the overlapping view after user's swipe action.
  enum class ButtonPosition { RIGHT, LEFT };

  explicit NotificationSwipeControlView(
      message_center::MessageView* message_view);
  NotificationSwipeControlView(const NotificationSwipeControlView&) = delete;
  NotificationSwipeControlView& operator=(const NotificationSwipeControlView&) =
      delete;
  ~NotificationSwipeControlView() override;

  // Update the visibility of control buttons.
  void UpdateButtonsVisibility();

 private:
  FRIEND_TEST_ALL_PREFIXES(NotificationSwipeControlViewTest,
                           DeleteOnSettingsButtonPressed);
  FRIEND_TEST_ALL_PREFIXES(NotificationSwipeControlViewTest,
                           SettingsButtonVisibility);

  // Change the visibility of the settings button.
  void ShowButtons(ButtonPosition button_position, bool show_settings);
  void HideButtons();

  // Change the visibility of the settings button. True to show, false to hide.
  void ShowSettingsButton(bool show);

  void ButtonPressed(const ui::Event& event);

  const raw_ptr<message_center::MessageView> message_view_;

  // Owned by views hierarchy.
  raw_ptr<views::ImageButton> settings_button_ = nullptr;

  base::WeakPtrFactory<NotificationSwipeControlView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_NOTIFICATION_SWIPE_CONTROL_VIEW_H_
