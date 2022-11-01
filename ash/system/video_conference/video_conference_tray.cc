// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_tray.h"

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
class VideoConferenceTrayButton : public IconButton {
 public:
  VideoConferenceTrayButton(const gfx::VectorIcon* icon,
                            const int accessible_name_id)
      : IconButton(base::BindRepeating(&VideoConferenceTrayButton::ToggleButton,
                                       base::Unretained(this)),
                   IconButton::Type::kMedium,
                   icon,
                   accessible_name_id,
                   /*is_togglable=*/true,
                   /*has_border=*/true) {}
  VideoConferenceTrayButton(const VideoConferenceTrayButton&) = delete;
  VideoConferenceTrayButton& operator=(const VideoConferenceTrayButton&) =
      delete;
  ~VideoConferenceTrayButton() override = default;

 private:
  void ToggleButton() { SetToggled(!toggled()); }
};

VideoConferenceTray::VideoConferenceTray(Shelf* shelf)
    : TrayBackgroundView(shelf,
                         TrayBackgroundViewCatalogName::kVideoConferenceTray) {
  audio_icon_ = tray_container()->AddChildView(
      std::make_unique<VideoConferenceTrayButton>(
          &kPrivacyIndicatorsMicrophoneIcon,
          IDS_PRIVACY_NOTIFICATION_TITLE_MIC));
  camera_icon_ = tray_container()->AddChildView(
      std::make_unique<VideoConferenceTrayButton>(
          &kPrivacyIndicatorsCameraIcon,
          IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA));
  screen_share_icon_ = tray_container()->AddChildView(
      std::make_unique<VideoConferenceTrayButton>(
          &kPrivacyIndicatorsScreenShareIcon,
          IDS_ASH_STATUS_TRAY_SCREEN_SHARE_TITLE));
  expand_indicator_ =
      tray_container()->AddChildView(std::make_unique<views::ImageView>());
}

VideoConferenceTray::~VideoConferenceTray() = default;

void VideoConferenceTray::ShowBubble() {
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

void VideoConferenceTray::CloseBubble() {
  SetIsActive(false);
  UpdateExpandIndicator();

  bubble_.reset();
  shelf()->UpdateAutoHideState();
}

TrayBubbleView* VideoConferenceTray::GetBubbleView() {
  return bubble_ ? bubble_->bubble_view() : nullptr;
}

views::Widget* VideoConferenceTray::GetBubbleWidget() const {
  return bubble_ ? bubble_->bubble_widget() : nullptr;
}

std::u16string VideoConferenceTray::GetAccessibleNameForTray() {
  // TODO(b/253646076): Finish this function.
  return std::u16string();
}

void VideoConferenceTray::HideBubbleWithView(
    const TrayBubbleView* bubble_view) {
  if (bubble_ && bubble_->bubble_view() == bubble_view)
    CloseBubble();
}

void VideoConferenceTray::ClickedOutsideBubble() {
  CloseBubble();
}

void VideoConferenceTray::HandleLocaleChange() {
  // TODO(b/253646076): Finish this function.
}

void VideoConferenceTray::UpdateLayout() {
  TrayBackgroundView::UpdateLayout();

  // Updates expand indicator for shelf alignment change.
  UpdateExpandIndicator();
}

void VideoConferenceTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();

  UpdateExpandIndicator();
}

void VideoConferenceTray::UpdateAfterLoginStatusChange() {
  SetVisiblePreferred(true);
}

void VideoConferenceTray::UpdateExpandIndicator() {
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

BEGIN_METADATA(VideoConferenceTray, TrayBackgroundView)
END_METADATA

}  // namespace ash
