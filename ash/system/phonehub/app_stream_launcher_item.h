// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_APP_STREAM_LAUNCHER_ITEM_H_
#define ASH_SYSTEM_PHONEHUB_APP_STREAM_LAUNCHER_ITEM_H_

#include "ash/ash_export.h"
#include "ash/system/phonehub/phone_hub_recent_app_button.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"

namespace views {
class LabelButton;
}

namespace ash {

// A view contains a PhoneHubRecentAppButton and a label with app name.
class ASH_EXPORT AppStreamLauncherItem : public views::View {
  METADATA_HEADER(AppStreamLauncherItem, views::View)

 public:
  AppStreamLauncherItem(
      base::RepeatingClosure callback,
      const phonehub::Notification::AppMetadata& app_metadata);

  ~AppStreamLauncherItem() override;
  AppStreamLauncherItem(AppStreamLauncherItem&) = delete;
  AppStreamLauncherItem operator=(AppStreamLauncherItem&) = delete;

  // views::View:
  bool HasFocus() const override;
  void RequestFocus() override;

  views::LabelButton* GetLabelForTest();
  PhoneHubRecentAppButton* GetIconForTest();

 private:
  // Owned by views hierarchy.
  // TODO(b/259426750) refactor PhoneHubRecentAppButton to a more generic name.
  raw_ptr<PhoneHubRecentAppButton> recent_app_button_ = nullptr;
  raw_ptr<views::LabelButton> label_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_APP_STREAM_LAUNCHER_ITEM_H_
