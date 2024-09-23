// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_tray.h"

#include <memory>
#include <string>

#include "ash/public/cpp/shelf_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/system/camera/camera_effects_controller.h"
#include "ash/system/privacy/screen_security_controller.h"
#include "ash/system/system_notification_controller.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/video_conference/bubble/bubble_view.h"
#include "ash/system/video_conference/bubble/linux_apps_bubble_view.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

constexpr float kTrayButtonsSpacing = 4;
constexpr float kPrivacyIndicatorRadius = 3;

// The offset value from the bottom right corner of the icon to the place where
// we actually want to draw the privacy indicator.
constexpr float kPrivacyIndicatorOffset = 2;

// Histogram names
constexpr char kToggleButtonHistogramName[] =
    "Ash.VideoConferenceTray.ToggleBubbleButton.Click";
constexpr char kCameraMuteHistogramName[] =
    "Ash.VideoConferenceTray.CameraMuteButton.Click";
constexpr char kMicrophoneMuteHistogramName[] =
    "Ash.VideoConferenceTray.MicrophoneMuteButton.Click";
constexpr char kStopScreenShareHistogramName[] =
    "Ash.VideoConferenceTray.StopScreenShareButton.Click";

// Check if there's a non-linux app(s) from the given `apps`.
bool HasNonLinuxMediaApps(const MediaApps& apps) {
  for (auto& app : apps) {
    if (app->app_type != crosapi::mojom::VideoConferenceAppType::kCrostiniVm &&
        app->app_type != crosapi::mojom::VideoConferenceAppType::kPluginVm &&
        app->app_type != crosapi::mojom::VideoConferenceAppType::kBorealis) {
      return true;
    }
  }
  return false;
}

// A customized toggle button for the VC tray's toggle bubble button.
class ToggleBubbleButton : public IconButton {
  METADATA_HEADER(ToggleBubbleButton, IconButton)

 public:
  ToggleBubbleButton(VideoConferenceTray* tray, PressedCallback callback)
      : IconButton(std::move(callback),
                   IconButton::Type::kMediumFloating,
                   &kVideoConferenceUpChevronIcon,
                   IDS_ASH_VIDEO_CONFERENCE_TOGGLE_BUBBLE_BUTTON_TOOLTIP,
                   /*is_togglable=*/true,
                   /*has_border=*/true),
        tray_(tray) {
    SetButtonController(std::make_unique<views::ButtonController>(
        /*views::Button*=*/this,
        std::make_unique<TrayBackgroundView::TrayButtonControllerDelegate>(
            /*views::Button*=*/this,
            TrayBackgroundViewCatalogName::kVideoConferenceTray)));
    // Reduce the focus ring padding which is installed by default by
    // `IconButton`. The default padding results in the focus ring being painted
    // outside of the available bounds.
    auto* focus_ring = views::FocusRing::Get(this);
    focus_ring->SetPathGenerator(
        std::make_unique<views::CircleHighlightPathGenerator>(
            -gfx::Insets(focus_ring->GetHaloThickness() / 2)));
  }
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
  const raw_ptr<VideoConferenceTray> tray_;
};

BEGIN_METADATA(ToggleBubbleButton)
END_METADATA

}  // namespace

VideoConferenceTrayButton::VideoConferenceTrayButton(
    PressedCallback callback,
    const gfx::VectorIcon* icon,
    const gfx::VectorIcon* toggled_icon,
    const gfx::VectorIcon* capturing_icon,
    const int accessible_name_id)
    : IconButton(std::move(callback),
                 IconButton::Type::kMedium,
                 icon,
                 accessible_name_id,
                 /*is_togglable=*/true,
                 /*has_border=*/true),
      accessible_name_id_(accessible_name_id),
      icon_(icon),
      capturing_icon_(capturing_icon) {
  SetButtonController(std::make_unique<views::ButtonController>(
      /*views::Button*=*/this,
      std::make_unique<TrayBackgroundView::TrayButtonControllerDelegate>(
          /*views::Button*=*/this,
          TrayBackgroundViewCatalogName::kVideoConferenceTray)));

  SetBackgroundToggledColor(cros_tokens::kCrosSysSystemNegativeContainer);
  SetIconToggledColor(cros_tokens::kCrosSysSystemOnNegativeContainer);

  SetBackgroundColor(cros_tokens::kCrosSysSystemOnBase1);

  SetToggledVectorIcon(*toggled_icon);

  GetViewAccessibility().SetRole(ax::mojom::Role::kToggleButton);

  // Reduce the focus ring padding which is installed by default by
  // `IconButton`. The default padding results in the focus ring being painted
  // outside of the available bounds.
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetPathGenerator(
      std::make_unique<views::CircleHighlightPathGenerator>(
          -gfx::Insets(focus_ring->GetHaloThickness() / 2)));

  UpdateTooltip();
}

VideoConferenceTrayButton::~VideoConferenceTrayButton() = default;

void VideoConferenceTrayButton::SetIsCapturing(bool is_capturing) {
  if (is_capturing_ == is_capturing) {
    return;
  }

  is_capturing_ = is_capturing;

  SetVectorIcon(is_capturing_ ? *capturing_icon_ : *icon_);
  UpdateCapturingState();
}

void VideoConferenceTrayButton::UpdateCapturingState() {
  // We should only show the privacy indicator when the button is not
  // muted/untoggled.
  const bool show_privacy_indicator = is_capturing_ && !toggled();

  // Always call `UpdateTooltip()` because even if `show_privacy_indicator_`
  // doesn't change, `is_capturing_` may have.
  absl::Cleanup scoped_tooltip_update = [this] { UpdateTooltip(); };

  if (show_privacy_indicator_ == show_privacy_indicator) {
    return;
  }

  show_privacy_indicator_ = show_privacy_indicator;
  SchedulePaint();
}

void VideoConferenceTrayButton::PaintButtonContents(gfx::Canvas* canvas) {
  IconButton::PaintButtonContents(canvas);

  if (!show_privacy_indicator_) {
    return;
  }

  const gfx::RectF bounds(GetContentsBounds());
  auto image = GetImageToPaint();
  auto indicator_origin_x = (bounds.width() - image.width()) / 2 +
                            image.width() - kPrivacyIndicatorRadius;
  auto indicator_origin_y = (bounds.height() - image.height()) / 2 +
                            image.height() - kPrivacyIndicatorRadius;

  // Draw the green dot privacy indicator.
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(
      GetColorProvider()->GetColor(ui::kColorAshPrivacyIndicatorsBackground));
  canvas->DrawCircle(gfx::PointF(indicator_origin_x - kPrivacyIndicatorOffset,
                                 indicator_origin_y),
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

BEGIN_METADATA(VideoConferenceTrayButton)
END_METADATA

VideoConferenceTray::VideoConferenceTray(Shelf* shelf)
    : TrayBackgroundView(shelf,
                         TrayBackgroundViewCatalogName::kVideoConferenceTray) {
  SetCallback(base::BindRepeating(&VideoConferenceTray::ToggleBubble,
                                  weak_ptr_factory_.GetWeakPtr()));

  tray_container()->SetSpacingBetweenChildren(kTrayButtonsSpacing);

  audio_icon_ = tray_container()->AddChildView(std::make_unique<
                                               VideoConferenceTrayButton>(
      base::BindRepeating(&VideoConferenceTray::OnAudioButtonClicked,
                          weak_ptr_factory_.GetWeakPtr()),
      /*icon=*/&kPrivacyIndicatorsMicrophoneIcon,
      /*toggled_icon=*/&kVideoConferenceMicrophoneMutedIcon,
      /*capturing_icon=*/&kVideoConferenceMicrophoneCapturingIcon,
      /*accessible_name_id=*/VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_MICROPHONE));
  audio_icon_->SetVisible(false);

  camera_icon_ = tray_container()->AddChildView(
      std::make_unique<VideoConferenceTrayButton>(
          base::BindRepeating(&VideoConferenceTray::OnCameraButtonClicked,
                              weak_ptr_factory_.GetWeakPtr()),
          &kPrivacyIndicatorsCameraIcon, &kVideoConferenceCameraMutedIcon,
          &kVideoConferenceCameraCapturingIcon,
          VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_CAMERA));
  camera_icon_->SetVisible(false);

  const bool allow_stop_screen_share =
      base::FeatureList::IsEnabled(features::kVcStopAllScreenShare);

  if (allow_stop_screen_share) {
    screen_share_icon_ = tray_container()->AddChildView(
        std::make_unique<VideoConferenceTrayButton>(
            base::BindRepeating(
                &VideoConferenceTray::OnScreenShareButtonClicked,
                weak_ptr_factory_.GetWeakPtr()),
            &kVideoConferenceScreenShareIcon, &kVideoConferenceScreenShareIcon,
            &kVideoConferenceScreenShareIcon,
            VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_SCREEN_SHARE));
    // Toggling screen share stops screen share, and removes the item.
    screen_share_icon_->set_toggle_is_one_way();
    screen_share_icon_->SetVisible(false);
  }

  toggle_bubble_button_ =
      tray_container()->AddChildView(std::make_unique<ToggleBubbleButton>(
          this, base::BindRepeating(&VideoConferenceTray::ToggleBubble,
                                    weak_ptr_factory_.GetWeakPtr())));

  VideoConferenceTrayController::Get()->AddObserver(this);
  VideoConferenceTrayController::Get()->GetEffectsManager().AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);

  // Update visibility of the tray and all child icons and indicators. If this
  // lives on a secondary display, it's possible a media session already exists
  // so force update all state.
  UpdateTrayAndIconsState();

  DCHECK_EQ(allow_stop_screen_share ? 4u : 3u,
            tray_container()->children().size())
      << "Icons must be updated here in case a media session begins prior to "
         "connecting a secondary display.";
}

VideoConferenceTray::~VideoConferenceTray() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  VideoConferenceTrayController::Get()->GetEffectsManager().RemoveObserver(
      this);
  VideoConferenceTrayController::Get()->RemoveObserver(this);
}

void VideoConferenceTray::CloseBubbleInternal() {
  bubble_open_ = false;
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
  return l10n_util::GetStringUTF16(IDS_ASH_VIDEO_CONFERENCE_ACCESSIBLE_NAME);
}

std::u16string VideoConferenceTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

void VideoConferenceTray::HideBubbleWithView(
    const TrayBubbleView* bubble_view) {
  if (bubble_ && bubble_->bubble_view() == bubble_view) {
    CloseBubble();
  }
}

void VideoConferenceTray::HideBubble(const TrayBubbleView* bubble_view) {
  if (bubble_ && bubble_->bubble_view() == bubble_view) {
    CloseBubble();
  }
}

void VideoConferenceTray::ClickedOutsideBubble(const ui::LocatedEvent& event) {
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

void VideoConferenceTray::OnAnimationEnded() {
  TrayBackgroundView::OnAnimationEnded();

  if (!visible_preferred()) {
    return;
  }

  auto* controller = VideoConferenceTrayController::Get();
  controller->MaybeRunNudgeRequest();
  controller->MaybeShowSpeakOnMuteOptInNudge();
}

bool VideoConferenceTray::ShouldEnterPushedState(const ui::Event& event) {
  // Never enter pushed state to avoid displaying unnecessary ink drop in
  // `ButtonController::OnMousePressed()`.
  return false;
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
  if (screen_share_icon_) {
    screen_share_icon_->SetVisible(is_capturing_screen);
    screen_share_icon_->SetIsCapturing(
        /*is_capturing=*/is_capturing_screen);
  }
}

void VideoConferenceTray::OnDlcDownloadStateChanged(
    bool add_warning,
    const std::u16string& feature_tile_title) {
  auto* bubble_view = GetBubbleView();
  if (!bubble_view) {
    return;
  }
  views::AsViewClass<video_conference::BubbleView>(bubble_view)
      ->OnDLCDownloadStateInError(add_warning, feature_tile_title);
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
  camera_icon_->SetToggled(/*toggled=*/controller->GetCameraMuted());

  audio_icon_->SetVisible(controller->GetHasMicrophonePermissions());
  audio_icon_->SetIsCapturing(controller->IsCapturingMicrophone());
  audio_icon_->SetToggled(/*toggled=*/controller->GetMicrophoneMuted());

  if (screen_share_icon_) {
    bool is_capturing_screen = controller->IsCapturingScreen();
    screen_share_icon_->SetVisible(is_capturing_screen);
    screen_share_icon_->SetIsCapturing(is_capturing_screen);
  }
}

IconButton* VideoConferenceTray::GetToggleBubbleButtonForTest() {
  return toggle_bubble_button_;
}

void VideoConferenceTray::OnSessionStateChanged(
    session_manager::SessionState state) {
  SetVisiblePreferred(VideoConferenceTrayController::Get()->ShouldShowTray());
}

void VideoConferenceTray::ToggleBubble(const ui::Event& event) {
  bubble_open_ = !bubble_open_;
  base::UmaHistogramBoolean(kToggleButtonHistogramName, bubble_open_);

  if (!bubble_open_) {
    CloseBubble();
    return;
  }

  VideoConferenceTrayController::Get()->CloseAllVcNudges();

  VideoConferenceTrayController::Get()
      ->GetEffectsManager()
      .NotifyVideoConferenceBubbleOpened();

  // If we are already in the process of getting the media apps, we don't need
  // to get it again.
  if (!getting_media_apps_) {
    getting_media_apps_ = true;
    // Get all the currently running media apps from the controller and use it
    // to construct the bubble.
    VideoConferenceTrayController::Get()->GetMediaApps(
        base::BindOnce(&VideoConferenceTray::ConstructBubbleWithMediaApps,
                       weak_ptr_factory_.GetWeakPtr()));
  }
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
  if (features::IsStopAllScreenShareEnabled()) {
    VideoConferenceTrayController::Get()->StopAllScreenShare();
    base::UmaHistogramBoolean(kStopScreenShareHistogramName, true);
  }
}

void VideoConferenceTray::ConstructBubbleWithMediaApps(MediaApps apps) {
  getting_media_apps_ = false;

  // Should not show anything if bubble is intended to be close.
  if (!bubble_open_) {
    return;
  }

  std::unique_ptr<TrayBubbleView> bubble_view;
  auto init_params = CreateInitParamsForTrayBubble(/*tray=*/this);
  init_params.preferred_width = kWideTrayMenuWidth;
  init_params.corner_radius = kUpdatedBubbleCornerRadius;

  // If all of the apps are Linux apps, we will just use `LinuxAppsBubbleView`
  // specifically for this situation.
  if (!HasNonLinuxMediaApps(apps)) {
    init_params.corner_radius = 18;
    bubble_view = std::make_unique<video_conference::LinuxAppsBubbleView>(
        init_params, apps);
    bubble_view->SetPreferredWidth(bubble_view->GetPreferredSize().width());
  } else {
    bubble_view = std::make_unique<video_conference::BubbleView>(
        /*init_params=*/init_params, /*media_apps=*/apps,
        /*controller=*/VideoConferenceTrayController::Get());
  }

  bubble_ = std::make_unique<TrayBubbleWrapper>(this);
  bubble_->ShowBubble(std::move(bubble_view));

  toggle_bubble_button_->SetToggled(true);
}

void VideoConferenceTray::SetBackgroundReplaceUiVisible(bool visible) {
  auto* bubble_view = GetBubbleView();
  if (bubble_view) {
    views::AsViewClass<video_conference::BubbleView>(bubble_view)
        ->SetBackgroundReplaceUiVisible(visible);
  }
}

BEGIN_METADATA(VideoConferenceTray)
END_METADATA

}  // namespace ash
