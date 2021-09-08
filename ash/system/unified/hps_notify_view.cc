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
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/image_view.h"

namespace ash {
namespace {

gfx::ImageSkia MakeIcon(SkColor color) {
  return gfx::CreateVectorIcon(gfx::IconDescription(
      kSystemTrayHpsNotifyIcon, kUnifiedTrayIconSize, color));
}

}  // namespace

HpsNotifyView::HpsNotifyView(Shelf* shelf)
    : TrayItemView(shelf),
      icon_color_(
          TrayIconColor(Shell::Get()->session_controller()->GetSessionState())),
      observation_(this) {
  CreateImageView();
  image_view()->SetImage(MakeIcon(icon_color_));
  observation_.Observe(Shell::Get()->session_controller());
}

HpsNotifyView::~HpsNotifyView() = default;

void HpsNotifyView::HandleLocaleChange() {}

void HpsNotifyView::OnSessionStateChanged(session_manager::SessionState state) {
  // Update color of icon if necessary.
  UpdateIcon();

  // Don't display during the OOBE flow.
  SetVisible(state != session_manager::SessionState::OOBE);
}

void HpsNotifyView::OnThemeChanged() {
  TrayItemView::OnThemeChanged();
  UpdateIcon();
}

const char* HpsNotifyView::GetClassName() const {
  return "HpsNotifyView";
}

void HpsNotifyView::UpdateIcon() {
  const SkColor new_color =
      TrayIconColor(Shell::Get()->session_controller()->GetSessionState());
  if (icon_color_ != new_color)
    image_view()->SetImage(MakeIcon(new_color));
}

}  // namespace ash
