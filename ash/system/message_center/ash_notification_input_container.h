// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_INPUT_CONTAINER_H_
#define ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_INPUT_CONTAINER_H_

#include "ash/ash_export.h"
#include "ui/message_center/views/notification_input_container.h"

namespace ash {

// Customized NotificationInputContainer for notifications on ChromeOS. This
// view is used to display an editable textfield for inline-reply and a
// send button.
class ASH_EXPORT AshNotificationInputContainer
    : public message_center::NotificationInputContainer {
 public:
  explicit AshNotificationInputContainer(
      message_center::NotificationInputDelegate* delegate);
  AshNotificationInputContainer(const AshNotificationInputContainer&) = delete;
  AshNotificationInputContainer& operator=(
      const AshNotificationInputContainer&) = delete;
  ~AshNotificationInputContainer() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AshNotificationViewTest, ButtonStateUpdated);
  // message_center::NotificationInputContainer:
  views::BoxLayout* InstallLayoutManager() override;
  views::InkDropContainerView* InstallInkDrop() override;
  gfx::Insets GetTextfieldPadding() const override;
  int GetDefaultPlaceholderStringId() const override;
  void StyleTextfield() override;
  gfx::Insets GetSendButtonPadding() const override;
  void SetSendButtonHighlightPath() override;
  void UpdateButtonImage() override;
  void UpdateButtonState();
  bool IsInputEmpty();

  // views::View:
  void OnThemeChanged() override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_INPUT_CONTAINER_H_
