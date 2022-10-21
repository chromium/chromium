// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/vc_tray.h"

#include <string>

#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "base/functional/bind.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/controls/image_view.h"

namespace ash {

// A toggle icon button in the VC tray, which is used for toggling camera,
// microphone, and screen sharing.
// Note that it's safe to use `base::Unretained()` in IconButton since callback
// is destroyed with `this`.
class VcTrayButton : public IconButton {
 public:
  VcTrayButton(const gfx::VectorIcon* icon, const int accessible_name_id)
      : IconButton(base::BindRepeating(&VcTrayButton::ToggleButton,
                                       base::Unretained(this)),
                   IconButton::Type::kMedium,
                   icon,
                   accessible_name_id,
                   /*is_togglable=*/true,
                   /*has_border=*/true) {}
  VcTrayButton(const VcTrayButton&) = delete;
  VcTrayButton& operator=(const VcTrayButton&) = delete;
  ~VcTrayButton() override = default;

 private:
  void ToggleButton() { SetToggled(!toggled()); }
};

VcTray::VcTray(Shelf* shelf)
    : TrayBackgroundView(shelf, TrayBackgroundViewCatalogName::kVcTray) {
  audio_icon_ = tray_container()->AddChildView(std::make_unique<VcTrayButton>(
      &kPrivacyIndicatorsMicrophoneIcon, IDS_PRIVACY_NOTIFICATION_TITLE_MIC));
  camera_icon_ = tray_container()->AddChildView(std::make_unique<VcTrayButton>(
      &kPrivacyIndicatorsCameraIcon, IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA));
  screen_share_icon_ = tray_container()->AddChildView(
      std::make_unique<VcTrayButton>(&kPrivacyIndicatorsScreenShareIcon,
                                     IDS_ASH_STATUS_TRAY_SCREEN_SHARE_TITLE));
  expand_indicator_ =
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
  UpdateExpandIndicator();
}

void VcTray::CloseBubble() {
  SetIsActive(false);
  UpdateExpandIndicator();

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

void VcTray::UpdateLayout() {
  TrayBackgroundView::UpdateLayout();

  // Updates expand indicator for shelf alignment change.
  UpdateExpandIndicator();
}

void VcTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();

  UpdateExpandIndicator();
}

void VcTray::UpdateAfterLoginStatusChange() {
  SetVisiblePreferred(true);
}

void VcTray::UpdateExpandIndicator() {
  auto image = gfx::CreateVectorIcon(
      kUnifiedMenuExpandIcon,
      TrayIconColor(Shell::Get()->session_controller()->GetSessionState()));

  SkBitmapOperations::RotationAmount rotation;
  switch (shelf()->alignment()) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      if (!is_active()) {
        // When bubble is not showing in horizontal shelf, no need to rotate and
        // return early.
        expand_indicator_->SetImage(image);
        return;
      }
      rotation = SkBitmapOperations::ROTATION_180_CW;
      break;
    case ShelfAlignment::kLeft:
      rotation = is_active() ? SkBitmapOperations::ROTATION_270_CW
                             : SkBitmapOperations::ROTATION_90_CW;
      break;
    case ShelfAlignment::kRight:
      rotation = is_active() ? SkBitmapOperations::ROTATION_90_CW
                             : SkBitmapOperations::ROTATION_270_CW;
      break;
  }

  expand_indicator_->SetImage(
      gfx::ImageSkiaOperations::CreateRotatedImage(image, rotation));
}

BEGIN_METADATA(VcTray, TrayBackgroundView)
END_METADATA

}  // namespace ash
