// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/hps_notify_view.h"

#include "ash/public/cpp/session/session_observer.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "base/logging.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/image_view.h"

namespace ash {

HpsNotifyView::HpsNotifyView(Shelf* shelf) : TrayItemView(shelf) {
  CreateImageView();

  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  session_observation_.Observe(session_controller);

  HpsNotifyController* controller = Shell::Get()->hps_notify_controller();
  controller_observer_.Observe(controller);

  SetVisible(controller->IsIconVisible());
  UpdateIconColor(session_controller->GetSessionState());
}

HpsNotifyView::~HpsNotifyView() = default;

void HpsNotifyView::HandleLocaleChange() {}

void HpsNotifyView::OnSessionStateChanged(
    session_manager::SessionState session_state) {
  UpdateIconColor(session_state);
}

void HpsNotifyView::OnThemeChanged() {
  TrayItemView::OnThemeChanged();
  UpdateIconColor(Shell::Get()->session_controller()->GetSessionState());
}

const char* HpsNotifyView::GetClassName() const {
  return "HpsNotifyView";
}

void HpsNotifyView::ShouldUpdateVisibility(bool visible) {
  SetVisible(visible);
}

void HpsNotifyView::UpdateIconColor(
    session_manager::SessionState session_state) {
  const SkColor new_color = TrayIconColor(session_state);
  const gfx::ImageSkia new_icon = gfx::CreateVectorIcon(gfx::IconDescription(
      kSystemTrayHpsNotifyIcon, kUnifiedTrayIconSize, new_color));
  image_view()->SetImage(new_icon);
}

}  // namespace ash
