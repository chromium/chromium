// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_tray.h"

#include <string>

#include "ash/public/cpp/shelf_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/system/privacy/screen_security_controller.h"
#include "ash/system/system_notification_controller.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/video_conference/bubble/bubble_view.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/base/l10n/l10n_util.h"
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

// Histogram names
constexpr char kToggleButtonHistogramName[] =
    "Ash.VideoConferenceTray.ToggleBubbleButton.Click";
constexpr char kCameraMuteHistogramName[] =
    "Ash.VideoConferenceTray.CameraMuteButton.Click";
constexpr char kMicrophoneMuteHistogramName[] =
    "Ash.VideoConferenceTray.MicrophoneMuteButton.Click";
constexpr char kStopScreenShareHistogramName[] =
    "Ash.VideoConferenceTray.StopScreenShareButton.Click";

// A customized toggle button for the VC tray's toggle bubble button.
class ToggleBubbleButton : public IconButton {
 public:
  ToggleBubbleButton(VideoConferenceTray* tray, PressedCallback callback)
      : IconButton(std::move(callback),
                   IconButton::Type::kMediumFloating,
                   &kVideoConferenceUpChevronIcon,
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
  const raw_ptr<VideoConferenceTray, ExperimentalAsh> tray_;
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
                 /*has_border=*/true),
      accessible_name_id_(accessible_name_id) {
  SetBackgroundToggledColorId(cros_tokens::kCrosSysSystemNegativeContainer);
  SetIconToggledColorId(cros_tokens::kCrosSysSystemOnNegativeContainer);

  SetToggledVectorIcon(*toggled_icon);

  SetAccessibleRole(ax::mojom::Role::kToggleButton);

  UpdateTooltip();
}

VideoConferenceTrayButton::~VideoConferenceTrayButton() = default;

void VideoConferenceTrayButton::SetIsCapturing(bool is_capturing) {
  if (is_capturing_ == is_capturing) {
    return;
  }

  is_capturing_ = is_capturing;
  UpdateCapturingState();
}

void VideoConferenceTrayButton::UpdateCapturingState() {
  // We should only show the privacy indicator when the button is not
  // muted/untoggled.
  const bool show_privacy_indicator = is_capturing_ && !toggled();

  // Always call `UpdateTooltip()` because even if `show_privacy_indicator_`
  // doesn't change, `is_capturing_` may have.
  base::ScopedClosureRunner scoped_closure(base::BindOnce(
      &VideoConferenceTrayButton::UpdateTooltip, base::Unretained(this)));

  if (show_privacy_indicator_ == show_privacy_indicator) {
    return;
  }

  show_privacy_indicator_ = show_privacy_indicator;
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

void VideoConferenceTrayButton::UpdateTooltip() {
  int capture_state_id = VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_OFF;
  if (show_privacy_indicator_) {
    capture_state_id = VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_ON_AND_IN_USE;
  } else if (!toggled()) {
    capture_state_id = VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_ON;
  }

  int base_string_id = VIDEO_CONFERENCE_TOGGLE_BUTTON_TOOLTIP;
  if (toggle_is_one_way_) {
    base_string_id = VIDEO_CONFERENCE_ONE_WAY_TOGGLE_BUTTON_TOOLTIP;
  }

  SetTooltipText(l10n_util::GetStringFUTF16(
      base_string_id, l10n_util::GetStringUTF16(accessible_name_id_),
      l10n_util::GetStringUTF16(capture_state_id)));
}

VideoConferenceTray::VideoConferenceTray(Shelf* shelf)
    : TrayBackgroundView(shelf,
                         TrayBackgroundViewCatalogName::kVideoConferenceTray) {
  // If the user pressed the body of the tray, just toggle the bubble.
  SetPressedCallback(base::BindRepeating(&VideoConferenceTray::ToggleBubble,
                                         weak_ptr_factory_.GetWeakPtr()));

  tray_container()->SetSpacingBetweenChildren(kTrayButtonsSpacing);

  audio_icon_ = tray_container()->AddChildView(
      std::make_unique<VideoConferenceTrayButton>(
          base::BindRepeating(&VideoConferenceTray::OnAudioButtonClicked,
                              weak_ptr_factory_.GetWeakPtr()),
          &kPrivacyIndicatorsMicrophoneIcon,
          &kVideoConferenceMicrophoneMutedIcon,
          VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_MICROPHONE));
  audio_icon_->SetVisible(false);

  camera_icon_ = tray_container()->AddChildView(
      std::make_unique<VideoConferenceTrayButton>(
          base::BindRepeating(&VideoConferenceTray::OnCameraButtonClicked,
                              weak_ptr_factory_.GetWeakPtr()),
          &kPrivacyIndicatorsCameraIcon, &kVideoConferenceCameraMutedIcon,
          VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_CAMERA));
  camera_icon_->SetVisible(false);

  screen_share_icon_ = tray_container()->AddChildView(
      std::make_unique<VideoConferenceTrayButton>(
          base::BindRepeating(&VideoConferenceTray::OnScreenShareButtonClicked,
                              weak_ptr_factory_.GetWeakPtr()),
          &kPrivacyIndicatorsScreenShareIcon,
          &kPrivacyIndicatorsScreenShareIcon,
          VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_SCREEN_SHARE));
  // Toggling screen share stops screen share, and removes the item.
  screen_share_icon_->set_toggle_is_one_way();
  screen_share_icon_->SetVisible(false);

  toggle_bubble_button_ =
      tray_container()->AddChildView(std::make_unique<ToggleBubbleButton>(
          this, base::BindRepeating(&VideoConferenceTray::ToggleBubble,
                                    weak_ptr_factory_.GetWeakPtr())));

  VideoConferenceTrayController::Get()->AddObserver(this);
  VideoConferenceTrayController::Get()->effects_manager().AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);

  // Update visibility of the tray and all child icons and indicators. If this
  // lives on a secondary display, it's possible a media session already exists
  // so force update all state.
  UpdateTrayAndIconsState();

  DCHECK_EQ(4u, tray_container()->children().size())
      << "Icons must be updated here in case a media session begins prior to "
         "connecting a secondary display.";
}

VideoConferenceTray::~VideoConferenceTray() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  VideoConferenceTrayController::Get()->effects_manager().RemoveObserver(this);
  VideoConferenceTrayController::Get()->RemoveObserver(this);
}

void VideoConferenceTray::CloseBubble() {
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

std::u16string VideoConferenceTray::GetAccessibleNameForBubble() {
  // TODO(b/261640628): Replace this placeholder with the appropriate string,
  // once it is decided.
  return u"Placeholder2";
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

void VideoConferenceTray::AnchorUpdated() {
  if (bubble_) {
    bubble_->bubble_view()->UpdateBubble();
  }
}

void VideoConferenceTray::OnHasMediaAppStateChange() {
  SetVisiblePreferred(VideoConferenceTrayController::Get()->ShouldShowTray());
}

void VideoConferenceTray::OnCameraPermissionStateChange() {
  camera_icon_->SetVisible(
      VideoConferenceTrayController::Get()->GetHasCameraPermissions());
}

void VideoConferenceTray::OnMicrophonePermissionStateChange() {
  audio_icon_->SetVisible(
      VideoConferenceTrayController::Get()->GetHasMicrophonePermissions());
}

void VideoConferenceTray::OnScreenSharingStateChange(bool is_capturing_screen) {
  screen_share_icon_->SetVisible(is_capturing_screen);
  screen_share_icon_->SetIsCapturing(
      /*is_capturing=*/is_capturing_screen);
}

void VideoConferenceTray::OnCameraCapturingStateChange(bool is_capturing) {
  camera_icon_->SetIsCapturing(is_capturing);
}

void VideoConferenceTray::OnMicrophoneCapturingStateChange(bool is_capturing) {
  audio_icon_->SetIsCapturing(is_capturing);
}

void VideoConferenceTray::OnEffectSupportStateChanged(VcEffectId effect_id,
                                                      bool is_supported) {
  // If the bubble is open, we close it so that when it is re-opened, the
  // bubble is updated with the correct effect support state.
  if (GetBubbleWidget()) {
    CloseBubble();
  }
}

SkScalar VideoConferenceTray::GetRotationValueForToggleBubbleButton() {
  // If `bubble_` is not null, it means that the bubble is opened.
  switch (shelf()->alignment()) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      return bubble_ ? 180 : 0;
    case ShelfAlignment::kLeft:
      return bubble_ ? 270 : 90;
    case ShelfAlignment::kRight:
      return bubble_ ? 90 : 270;
  }
}

void VideoConferenceTray::UpdateTrayAndIconsState() {
  auto* controller = VideoConferenceTrayController::Get();

  SetVisiblePreferred(controller->ShouldShowTray());

  camera_icon_->SetVisible(controller->GetHasCameraPermissions());
  camera_icon_->SetIsCapturing(controller->IsCapturingCamera());

  audio_icon_->SetVisible(controller->GetHasMicrophonePermissions());
  audio_icon_->SetIsCapturing(controller->IsCapturingMicrophone());

  bool is_capturing_screen = controller->IsCapturingScreen();
  screen_share_icon_->SetVisible(is_capturing_screen);
  screen_share_icon_->SetIsCapturing(is_capturing_screen);
}

void VideoConferenceTray::OnSessionStateChanged(
    session_manager::SessionState state) {
  SetVisiblePreferred(VideoConferenceTrayController::Get()->ShouldShowTray());
}

void VideoConferenceTray::ToggleBubble(const ui::Event& event) {
  const bool bubble_open = GetBubbleWidget();
  base::UmaHistogramBoolean(kToggleButtonHistogramName, !bubble_open);

  if (bubble_open) {
    CloseBubble();
    return;
  }

  TrayBubbleView::InitParams init_params;
  init_params.delegate = GetWeakPtr();
  init_params.parent_window = GetBubbleWindowContainer();
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = GetAnchorBoundsInScreen();
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

  toggle_bubble_button_->SetToggled(true);
}

void VideoConferenceTray::OnCameraButtonClicked(const ui::Event& event) {
  const bool muted = !camera_icon_->toggled();
  VideoConferenceTrayController::Get()->SetCameraMuted(muted);

  base::UmaHistogramBoolean(kCameraMuteHistogramName, !muted);
}

void VideoConferenceTray::OnAudioButtonClicked(const ui::Event& event) {
  const bool muted = !audio_icon_->toggled();
  VideoConferenceTrayController::Get()->SetMicrophoneMuted(muted);

  base::UmaHistogramBoolean(kMicrophoneMuteHistogramName, !muted);
}

void VideoConferenceTray::OnScreenShareButtonClicked(const ui::Event& event) {
  Shell::Get()
      ->system_notification_controller()
      ->screen_security_controller()
      ->StopAllSessions(/*is_screen_access=*/true);

  base::UmaHistogramBoolean(kStopScreenShareHistogramName, true);
}

BEGIN_METADATA(VideoConferenceTray, TrayBackgroundView)
END_METADATA

}  // namespace ash
