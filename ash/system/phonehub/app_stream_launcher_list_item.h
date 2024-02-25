// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_APP_STREAM_LAUNCHER_LIST_ITEM_H_
#define ASH_SYSTEM_PHONEHUB_APP_STREAM_LAUNCHER_LIST_ITEM_H_

#include "ash/ash_export.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {

// A LabelButton that represents an app in the launcher.
class ASH_EXPORT AppStreamLauncherListItem : public views::LabelButton {
  METADATA_HEADER(AppStreamLauncherListItem, views::LabelButton)
 public:
  AppStreamLauncherListItem(
      PressedCallback callback,
      const phonehub::Notification::AppMetadata& app_metadata);

  ~AppStreamLauncherListItem() override;
  AppStreamLauncherListItem(AppStreamLauncherListItem&) = delete;
  AppStreamLauncherListItem operator=(AppStreamLauncherListItem&) = delete;

  std::u16string GetAppAccessibleName(
      const phonehub::Notification::AppMetadata& app_metadata);

};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_APP_STREAM_LAUNCHER_LIST_ITEM_H_
