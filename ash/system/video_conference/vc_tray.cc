// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/vc_tray.h"

#include <string>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/image_view.h"

namespace ash {

VcTray::VcTray(Shelf* shelf)
    : TrayBackgroundView(shelf, TrayBackgroundViewCatalogName::kVcTray) {
  audio_icon_ =
      tray_container()->AddChildView(std::make_unique<views::ImageView>());
}

VcTray::~VcTray() = default;

void VcTray::ShowBubble() {
  TrayBubbleView::InitParams init_params;
  init_params.delegate = GetWeakPtr();
  init_params.parent_window = GetBubbleWindowContainer();
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = shelf()->GetSystemTrayAnchorRect();
  init_params.insets = GetTrayBubbleInsets();
  init_params.shelf_alignment = shelf()->alignment();
  init_params.preferred_width = kTrayMenuWidth;
  init_params.close_on_deactivate = true;
  init_params.translucent = true;

  // Create top-level bubble.
  TrayBubbleView* bubble_view = new TrayBubbleView(init_params);

  // TODO(b/253088232): Added an icon so that the bubble can show. Will remove
  // this with the newly created class VcBubbleView.
  auto icon = std::make_unique<views::ImageView>();
  icon->SetImage(gfx::CreateVectorIcon(
      kPrivacyIndicatorsMicrophoneIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));
  bubble_view->AddChildView(std::move(icon));

  bubble_ = std::make_unique<TrayBubbleWrapper>(this, bubble_view);
  SetIsActive(true);
}

void VcTray::CloseBubble() {
  SetIsActive(false);
  bubble_.reset();
  shelf()->UpdateAutoHideState();
}

TrayBubbleView* VcTray::GetBubbleView() {
  return bubble_ ? bubble_->bubble_view() : nullptr;
}

views::Widget* VcTray::GetBubbleWidget() const {
  return bubble_ ? bubble_->bubble_widget() : nullptr;
}

std::u16string VcTray::GetAccessibleNameForTray() {
  // TODO(b/253646076): Finish this function.
  return std::u16string();
}

void VcTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  if (bubble_ && bubble_->bubble_view() == bubble_view)
    CloseBubble();
}

void VcTray::ClickedOutsideBubble() {
  CloseBubble();
}

void VcTray::HandleLocaleChange() {
  // TODO(b/253646076): Finish this function.
}

void VcTray::OnThemeChanged() {
  views::View::OnThemeChanged();

  audio_icon_->SetImage(gfx::CreateVectorIcon(
      kPrivacyIndicatorsMicrophoneIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));
}

void VcTray::UpdateAfterLoginStatusChange() {
  SetVisiblePreferred(true);
}

BEGIN_METADATA(VcTray, TrayBackgroundView)
END_METADATA

}  // namespace ash
