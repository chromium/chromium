// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/screen_capture_tray_item_view.h"

#include "ash/multi_capture/multi_capture_service_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/image_view.h"

namespace ash {

ScreenCaptureTrayItemView::ScreenCaptureTrayItemView(Shelf* shelf)
    : TrayItemView(shelf) {
  CreateImageView();
  const gfx::VectorIcon* icon = &kSystemTrayRecordingIcon;
  image_view()->SetImage(gfx::CreateVectorIcon(gfx::IconDescription(
      *icon, kUnifiedTrayIconSize,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorAlert))));

  multi_capture_service_client_observation_.Observe(
      Shell::Get()->multi_capture_service_client());
  Refresh();
}

ScreenCaptureTrayItemView::~ScreenCaptureTrayItemView() = default;

const char* ScreenCaptureTrayItemView::GetClassName() const {
  return "ScreenCaptureTrayItemView";
}

views::View* ScreenCaptureTrayItemView::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return HitTestPoint(point) ? this : nullptr;
}

std::u16string ScreenCaptureTrayItemView::GetTooltipText(
    const gfx::Point& point) const {
  return l10n_util::GetStringUTF16(IDS_ASH_ADMIN_SCREEN_CAPTURE);
}

void ScreenCaptureTrayItemView::Refresh() {
  SetVisible(!request_ids_.empty());
}

void ScreenCaptureTrayItemView::MultiCaptureStarted(const std::string& label,
                                                    const url::Origin& origin) {
  request_ids_.insert(label);
  Refresh();
}

void ScreenCaptureTrayItemView::MultiCaptureStopped(const std::string& label) {
  request_ids_.erase(label);
  Refresh();
}

void ScreenCaptureTrayItemView::MultiCaptureServiceClientDestroyed() {
  multi_capture_service_client_observation_.Reset();
}
}  // namespace ash
