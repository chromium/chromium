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
#include "ash/system/video_conference/bubble/bubble_view.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/functional/bind.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/scoped_canvas.h"

namespace ash {

namespace {

constexpr float kTrayButtonsSpacing = 4;
constexpr float kPrivacyIndicatorRadius = 4;
constexpr float kIndicatorBorderWidth = 1;

// A customized toggle button for the VC tray's toggle bubble button.
class ToggleBubbleButton : public IconButton {
 public:
  ToggleBubbleButton(VideoConferenceTray* tray, PressedCallback callback)
      : IconButton(std::move(callback),
                   IconButton::Type::kMediumFloating,
                   &kUnifiedMenuExpandIcon,
                   IDS_ASH_VIDEO_CONFERENCE_TOGGLE_BUBBLE_BUTTON_TOOLTIP,
                   /*is_togglable=*/true,
                   /*has_border=*/true),
        tray_(tray) {}
  ToggleBubbleButton(const ToggleBubbleButton&) = delete;
  ToggleBubbleButton& operator=(const ToggleBubbleButton&) = delete;
  ~ToggleBubbleButton() override = default;

  // IconButton:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    // Rotate the canvas to rotate the expand indicator according to toggle
    // state and shelf alignment. Note that when shelf alignment changes,
    // TrayBackgroundView::UpdateLayout() will be triggered and this button will
    // be automatically repainted, so we don't need to manually handle it.
    gfx::ScopedCanvas scoped(canvas);
    canvas->Translate(gfx::Vector2d(size().width() / 2, size().height() / 2));
    canvas->sk_canvas()->rotate(tray_->GetRotationValueForToggleBubbleButton());
    gfx::ImageSkia image = GetImageToPaint();
    canvas->DrawImageInt(image, -image.width() / 2, -image.height() / 2);
  }

 private:
  // Parent view of this button. Owned by the views hierarchy.
  VideoConferenceTray* const tray_;
};

}  // namespace

VideoConferenceTrayButton::VideoConferenceTrayButton(
    PressedCallback callback,
    const gfx::VectorIcon* icon,
    const gfx::VectorIcon* toggled_icon,
    const int accessible_name_id)
    : IconButton(std::move(callback),
                 IconButton::Type::kMedium,
                 icon,
                 accessible_name_id,
                 /*is_togglable=*/true,
                 /*has_border=*/true) {
  SetBackgroundToggledColorId(cros_tokens::kCrosSysSystemNegativeContainer);
  SetIconToggledColorId(cros_tokens::kCrosSysSystemOnNegativeContainer);

  SetToggledVectorIcon(*toggled_icon);
}

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
  SetVisiblePreferred(false);

  tray_container()->SetSpacingBetweenChildren(kTrayButtonsSpacing);

  audio_icon_ = tray_container()->AddChildView(
      std::make_unique<VideoConferenceTrayButton>(
          base::BindRepeating(&VideoConferenceTray::OnAudioButtonClicked,
                              weak_ptr_factory_.GetWeakPtr()),
          &kPrivacyIndicatorsMicrophoneIcon,
          &kVideoConferenceMicrophoneMutedIcon,
          IDS_PRIVACY_NOTIFICATION_TITLE_MIC));
  audio_icon_->SetVisible(false);

  camera_icon_ = tray_container()->AddChildView(
      std::make_unique<VideoConferenceTrayButton>(
          base::BindRepeating(&VideoConferenceTray::OnCameraButtonClicked,
                              weak_ptr_factory_.GetWeakPtr()),
          &kPrivacyIndicatorsCameraIcon, &kVideoConferenceCameraMutedIcon,
          IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA));
  camera_icon_->SetVisible(false);

  screen_share_icon_ = tray_container()->AddChildView(
      std::make_unique<VideoConferenceTrayButton>(
          base::BindRepeating(&VideoConferenceTray::OnScreenShareButtonClicked,
                              weak_ptr_factory_.GetWeakPtr()),
          &kPrivacyIndicatorsScreenShareIcon,
          &kPrivacyIndicatorsScreenShareIcon,
          IDS_ASH_STATUS_TRAY_SCREEN_SHARE_TITLE));
  screen_share_icon_->SetVisible(false);

  toggle_bubble_button_ =
      tray_container()->AddChildView(std::make_unique<ToggleBubbleButton>(
          this, base::BindRepeating(&VideoConferenceTray::ToggleBubble,
                                    weak_ptr_factory_.GetWeakPtr())));

  VideoConferenceTrayController::Get()->AddObserver(this);
}

VideoConferenceTray::~VideoConferenceTray() {
  VideoConferenceTrayController::Get()->RemoveObserver(this);
}

void VideoConferenceTray::CloseBubble() {
  SetIsActive(false);
  toggle_bubble_button_->SetToggled(false);

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
  // TODO(b/253646076): The following is a temporary fix to pass
  // https://crrev.com/c/4109611 browsertests and still needs to be replaced
  // with the proper name.
  return u"Placeholder";
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

void VideoConferenceTray::OnHasMediaAppStateChange(bool has_media_app) {
  SetVisiblePreferred(has_media_app);
}

void VideoConferenceTray::OnCameraPermissionStateChange(bool has_permission) {
  camera_icon_->SetVisible(has_permission);
}

void VideoConferenceTray::OnMicrophonePermissionStateChange(
    bool has_permission) {
  audio_icon_->SetVisible(has_permission);
}

void VideoConferenceTray::OnScreenSharingStateChange(bool is_capturing_screen) {
  screen_share_icon_->SetVisible(is_capturing_screen);
  screen_share_icon_->SetShowPrivacyIndicator(/*show=*/is_capturing_screen);
}

void VideoConferenceTray::OnCameraCapturingStateChange(bool is_capturing) {
  camera_icon_->SetShowPrivacyIndicator(/*show=*/is_capturing);
}

void VideoConferenceTray::OnMicrophoneCapturingStateChange(bool is_capturing) {
  audio_icon_->SetShowPrivacyIndicator(/*show=*/is_capturing);
}

SkScalar VideoConferenceTray::GetRotationValueForToggleBubbleButton() {
  switch (shelf()->alignment()) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      return is_active() ? 180 : 0;
    case ShelfAlignment::kLeft:
      return is_active() ? 270 : 90;
    case ShelfAlignment::kRight:
      return is_active() ? 90 : 270;
  }
}

void VideoConferenceTray::ToggleBubble(const ui::Event& event) {
  if (GetBubbleWidget()) {
    CloseBubble();
    return;
  }

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
  auto bubble_view = std::make_unique<video_conference::BubbleView>(
      init_params, VideoConferenceTrayController::Get());
  bubble_ = std::make_unique<TrayBubbleWrapper>(this);
  bubble_->ShowBubble(std::move(bubble_view));

  SetIsActive(true);
  toggle_bubble_button_->SetToggled(true);
}

void VideoConferenceTray::OnCameraButtonClicked(const ui::Event& event) {
  VideoConferenceTrayController::Get()->SetCameraMuted(
      /*muted=*/!camera_icon_->toggled());
}

void VideoConferenceTray::OnAudioButtonClicked(const ui::Event& event) {
  // TODO(b/253275993): Implement the callback for `audio_icon_`.
  VideoConferenceTrayController::Get()->SetMicrophoneMuted(
      /*muted=*/!audio_icon_->toggled());
}

void VideoConferenceTray::OnScreenShareButtonClicked(const ui::Event& event) {
  // TODO(b/253277644): Implement the callback for `screen_share_icon_`.
}

BEGIN_METADATA(VideoConferenceTray, TrayBackgroundView)
END_METADATA

}  // namespace ash
