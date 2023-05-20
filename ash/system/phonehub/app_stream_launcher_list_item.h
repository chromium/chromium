// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_APP_STREAM_LAUNCHER_LIST_ITEM_H_
#define ASH_SYSTEM_PHONEHUB_APP_STREAM_LAUNCHER_LIST_ITEM_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {

// A view contains a Label button with app icon and name
class ASH_EXPORT AppStreamLauncherListItem : public views::View {
 public:
  AppStreamLauncherListItem(
      views::LabelButton::PressedCallback callback,
      const phonehub::Notification::AppMetadata& app_metadata);

  ~AppStreamLauncherListItem() override;
  AppStreamLauncherListItem(AppStreamLauncherListItem&) = delete;
  AppStreamLauncherListItem operator=(AppStreamLauncherListItem&) = delete;

  std::u16string GetAppAccessibleName(
      const phonehub::Notification::AppMetadata& app_metadata);

  // views::View:
  bool HasFocus() const override;
  void RequestFocus() override;
  const char* GetClassName() const override;

  views::LabelButton* GetAppButtonForTest();

 private:
  class AppButton : public views::LabelButton {
   public:
    explicit AppButton(views::LabelButton::PressedCallback callback,
                       const std::u16string& text);
    ~AppButton() override;
    AppButton(AppButton&) = delete;
    AppButton operator=(AppButton&) = delete;

    // views::View:
    const char* GetClassName() const override;
  };

  // Owned by views hierarchy.
  raw_ptr<AppButton, ExperimentalAsh> app_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_APP_STREAM_LAUNCHER_LIST_ITEM_H_
