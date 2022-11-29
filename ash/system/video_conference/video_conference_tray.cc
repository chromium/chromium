// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_tray.h"

#include <string>

#include "ash/public/cpp/shelf_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/video_conference/video_conference_bubble.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/functional/bind.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

constexpr float kPrivacyIndicatorRadius = 4;
constexpr float kIndicatorBorderWidth = 1;

}  // namespace

VideoConferenceTrayButton::VideoConferenceTrayButton(
    PressedCallback callback,
    const gfx::VectorIcon* icon,
    const int accessible_name_id)
    : IconButton(std::move(callback),
                 IconButton::Type::kMedium,
                 icon,
                 accessible_name_id,
                 /*is_togglable=*/true,
                 /*has_border=*/true) {}

VideoConferenceTrayButton::~VideoConferenceTrayButton() = default;

void VideoConferenceTrayButton::SetShowPrivacyIndicator(bool show) {
  if (show_privacy_indicator_ == show)
    return;

  show_privacy_indicator_ = show;
  SchedulePaint();
}

void VideoConferenceTrayButton::PaintButtonContents(gfx::Canvas* canvas) {
  IconButton::PaintButtonContents(canvas);

  if (!show_privacy_indicator_)
    return;

  const gfx::RectF bounds(GetContentsBounds());
  auto image = GetImageToPaint();
  auto indicator_origin_x = (bounds.width() - image.width()) / 2 +
                            image.width() - kPrivacyIndicatorRadius;
  auto indicator_origin_y = (bounds.height() - image.height()) / 2 +
                            image.height() - kPrivacyIndicatorRadius;

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);

  // Draw the outer border of the green dot.
  flags.setColor(GetBackgroundColor());
  canvas->DrawCircle(
      gfx::PointF(indicator_origin_x - kIndicatorBorderWidth / 2,
                  indicator_origin_y - kIndicatorBorderWidth / 2),
      kPrivacyIndicatorRadius + kIndicatorBorderWidth, flags);

  // Draw the green dot privacy indicator.
  flags.setColor(
      GetColorProvider()->GetColor(ui::kColorAshPrivacyIndicatorsBackground));
  canvas->DrawCircle(gfx::PointF(indicator_origin_x, indicator_origin_y),
                     kPrivacyIndicatorRadius, flags);
}

VideoConferenceTray::VideoConferenceTray(Shelf* shelf)
    : TrayBackgroundView(shelf,
                         TrayBackgroundViewCatalogName::kVideoConferenceTray) {
  audio_icon_ = tray_container()->AddChildView(
      std::make_unique<VideoConferenceTrayButton>(
          base::BindRepeating(&VideoConferenceTray::OnAudioButtonClicked,
                              weak_ptr_factory_.GetWeakPtr()),
          &kPrivacyIndicatorsMicrophoneIcon,
          IDS_PRIVACY_NOTIFICATION_TITLE_MIC));
  camera_icon_ = tray_container()->AddChildView(
      std::make_unique<VideoConferenceTrayButton>(
          base::BindRepeating(&VideoConferenceTray::OnCameraButtonClicked,
                              weak_ptr_factory_.GetWeakPtr()),
          &kPrivacyIndicatorsCameraIcon,
          IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA));
  screen_share_icon_ = tray_container()->AddChildView(
      std::make_unique<VideoConferenceTrayButton>(
          base::BindRepeating(&VideoConferenceTray::OnScreenShareButtonClicked,
                              weak_ptr_factory_.GetWeakPtr()),
          &kPrivacyIndicatorsScreenShareIcon,
          IDS_ASH_STATUS_TRAY_SCREEN_SHARE_TITLE));
  expand_indicator_ =
      tray_container()->AddChildView(std::make_unique<views::ImageView>());

  VideoConferenceTrayController::Get()->AddObserver(this);
}

VideoConferenceTray::~VideoConferenceTray() {
  VideoConferenceTrayController::Get()->RemoveObserver(this);
}

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
  auto bubble_view = std::make_unique<VideoConferenceBubbleView>(init_params);
  bubble_ = std::make_unique<TrayBubbleWrapper>(this);
  bubble_->ShowBubble(std::move(bubble_view));

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

void VideoConferenceTray::OnCameraCapturingStateChange(bool is_capturing) {
  camera_icon_->SetShowPrivacyIndicator(/*show=*/is_capturing);
}

void VideoConferenceTray::OnMicrophoneCapturingStateChange(bool is_capturing) {
  audio_icon_->SetShowPrivacyIndicator(/*show=*/is_capturing);
}

void VideoConferenceTray::UpdateExpandIndicator() {
  auto image = gfx::CreateVectorIcon(
      kUnifiedMenuExpandIcon,
      GetColorProvider()->GetColor(kColorAshIconColorPrimary));

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

void VideoConferenceTray::OnCameraButtonClicked(const ui::Event& event) {
  VideoConferenceTrayController::Get()->SetCameraSoftwareMuted(
      /*mute_camera=*/!camera_icon_->toggled());
}

void VideoConferenceTray::OnAudioButtonClicked(const ui::Event& event) {
  // TODO(b/253275993): Implement the callback for `audio_icon_`.
}

void VideoConferenceTray::OnScreenShareButtonClicked(const ui::Event& event) {
  // TODO(b/253277644): Implement the callback for `screen_share_icon_`.
}

BEGIN_METADATA(VideoConferenceTray, TrayBackgroundView)
END_METADATA

}  // namespace ash
