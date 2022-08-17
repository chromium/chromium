// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_tray.h"

#include <string>

#include "ash/shelf/shelf.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"

namespace ash {

NotificationCenterTray::NotificationCenterTray(Shelf* shelf)
    : TrayBackgroundView(shelf, RoundedCornerBehavior::kStartRounded) {
  SetLayoutManager(std::make_unique<views::FlexLayout>());
}

NotificationCenterTray::~NotificationCenterTray() = default;

std::u16string NotificationCenterTray::GetAccessibleNameForTray() {
  return std::u16string();
}

void NotificationCenterTray::HandleLocaleChange() {}

void NotificationCenterTray::HideBubbleWithView(
    const TrayBubbleView* bubble_view) {}

void NotificationCenterTray::ClickedOutsideBubble() {
  CloseBubble();
}

void NotificationCenterTray::CloseBubble() {}

void NotificationCenterTray::ShowBubble() {}

void NotificationCenterTray::UpdateAfterLoginStatusChange() {}

TrayBubbleView* NotificationCenterTray::GetBubbleView() {
  return nullptr;
}

views::Widget* NotificationCenterTray::GetBubbleWidget() const {
  return nullptr;
}

BEGIN_METADATA(NotificationCenterTray, TrayBackgroundView)
END_METADATA

}  // namespace ash
