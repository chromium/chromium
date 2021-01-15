// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/notification_icons_controller.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

// Maximum number of notification icons shown in the system tray button.
constexpr int kMaxNotificationIconsShown = 2;

}  // namespace

class NotificationIconTrayItemView : public TrayItemView {
 public:
  explicit NotificationIconTrayItemView(Shelf* shelf) : TrayItemView(shelf) {
    CreateImageView();
    // TODO(crbug.com/1161557): Update icon to be the icon of the shown
    // notification.
    image_view()->SetImage(CreateVectorIcon(
        kSystemTrayCapsLockIcon,
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorPrimary)));
  }
  ~NotificationIconTrayItemView() override = default;
  NotificationIconTrayItemView(const NotificationIconTrayItemView&) = delete;
  NotificationIconTrayItemView& operator=(const NotificationIconTrayItemView&) =
      delete;

  // TrayItemView:
  void HandleLocaleChange() override {
    // TODO(crbug.com/1161557) : Finish this function.
  }

  const char* GetClassName() const override {
    return "NotificationIconTrayItemView";
  }
};

NotificationIconsController::NotificationIconsController(
    UnifiedSystemTray* tray)
    : tray_(tray) {
  system_tray_model_observation_.Observe(tray_->model());
}

NotificationIconsController::~NotificationIconsController() = default;

void NotificationIconsController::AddNotificationTrayItems(
    TrayContainer* tray_container) {
  for (int i = 0; i < kMaxNotificationIconsShown; ++i) {
    tray_items_.push_back(tray_container->AddChildView(
        std::make_unique<NotificationIconTrayItemView>(tray_->shelf())));
  }

  OnSystemTrayButtonSizeChanged(tray_->model()->GetSystemTrayButtonSize());

  // TODO(crbug.com/1161557): Handle only showing important notification icons.
}

void NotificationIconsController::SetVisible(bool visible) {
  for (TrayItemView* tray_item : tray_items_) {
    tray_item->SetVisible(visible);
  }
}

void NotificationIconsController::OnSystemTrayButtonSizeChanged(
    UnifiedSystemTrayModel::SystemTrayButtonSize system_tray_size) {
  SetVisible(system_tray_size !=
             UnifiedSystemTrayModel::SystemTrayButtonSize::kSmall);
}

}  // namespace ash
