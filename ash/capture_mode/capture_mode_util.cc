// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_util.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/stop_recording_button_tray.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/public/cpp/window_finder.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "ash/system/privacy/privacy_indicators_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider_manager.h"
#include "ui/compositor/layer.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash::capture_mode_util {

namespace {

constexpr int kBannerViewTopRadius = 0;
constexpr int kBannerViewBottomRadius = 8;
constexpr float kScaleUpFactor = 0.8f;

// The app ID used for the capture mode privacy indicators.
constexpr char kCaptureModePrivacyIndicatorsId[] = "system-capture-mode";

// Returns the target visibility of the camera preview, given the
// `confine_bounds_short_side_length`. The out parameter
// `out_is_surface_too_small` will be set to true if the preview should be
// hidden due to the surface within which it's confined is too small. Otherwise,
// it's unchanged.
bool CalculateCameraPreviewTargetVisibility(
    int confine_bounds_short_side_length,
    bool* out_is_surface_too_small) {
  DCHECK(out_is_surface_too_small);

  // If the short side of the bounds within which the camera preview should be
  // confined is too small, the camera should be hidden.
  if (confine_bounds_short_side_length <
      capture_mode::kMinCaptureSurfaceShortSideLengthForVisibleCamera) {
    *out_is_surface_too_small = true;
    return false;
  }

  // Now that we determined that its size doesn't affect its visibility, we need
  // to check if we're in a capture mode session that is in a state that affects
  // the camera preview's visibility.
  auto* controller = CaptureModeController::Get();
  return !controller->IsActive() ||
         controller->capture_mode_session()
             ->CalculateCameraPreviewTargetVisibility();
}

void FadeInWidget(views::Widget* widget,
                  const AnimationParams& animation_params) {
  DCHECK(widget);
  auto* layer = widget->GetLayer();
  DCHECK(!widget->GetNativeWindow()->TargetVisibility() ||
         layer->GetTargetOpacity() < 1.f);

  // Please notice the order matters here. When the opacity is set to 0.f, if
  // there's any on-going fade out animation, the `OnEnded` in `FadeOutWidget`
  // will be triggered, which will hide the widget and set its opacity to 1.f.
  // So `Show` should be triggered after setting the opacity to 0 to undo the
  // work done by the FadeOutWidget's OnEnded .
  if (layer->opacity() == 1.f)
    layer->SetOpacity(0.f);
  if (!widget->GetNativeWindow()->TargetVisibility())
    widget->Show();

  if (animation_params.apply_scale_up_animation) {
    layer->SetTransform(
        capture_mode_util::GetScaleTransformAboutCenter(layer, kScaleUpFactor));
  }

  views::AnimationBuilder builder;
  auto& animation_sequence_block =
      builder
          .SetPreemptionStrategy(
              ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
          .Once()
          .SetDuration(animation_params.animation_duration)
          .SetOpacity(layer, 1.f, animation_params.tween_type);

  // We should only set transform here if `apply_scale_up_animation` is true,
  // otherwise, it may mess up with the snap animation in
  // `SetCameraPreviewBounds`.
  if (animation_params.apply_scale_up_animation) {
    animation_sequence_block.SetTransform(layer, gfx::Transform(),
                                          gfx::Tween::ACCEL_20_DECEL_100);
  }
}

void FadeOutWidget(views::Widget* widget,
                   const AnimationParams& animation_params) {
  DCHECK(widget);
  DCHECK(widget->GetNativeWindow()->TargetVisibility());

  auto* layer = widget->GetLayer();
  DCHECK_EQ(layer->GetTargetOpacity(), 1.f);

  views::AnimationBuilder()
      .OnEnded(base::BindOnce(
          [](base::WeakPtr<views::Widget> the_widget) {
            if (!the_widget)
              return;

            // Please notice, the order matters here. If we set the layer's
            // opacity back to 1.f before calling `Hide`, flickering can be
            // seen.
            the_widget->Hide();
            the_widget->GetLayer()->SetOpacity(1.f);
          },
          widget->GetWeakPtr()))
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(animation_params.animation_duration)
      .SetOpacity(layer, 0.f, animation_params.tween_type);
}

}  // namespace

bool IsCaptureModeActive() {
  return CaptureModeController::Get()->IsActive();
}

gfx::Point GetLocationForFineTunePosition(const gfx::Rect& rect,
                                          FineTunePosition position) {
  switch (position) {
    case FineTunePosition::kTopLeft:
      return rect.origin();
    case FineTunePosition::kTopCenter:
      return rect.top_center();
    case FineTunePosition::kTopRight:
      return rect.top_right();
    case FineTunePosition::kRightCenter:
      return rect.right_center();
    case FineTunePosition::kBottomRight:
      return rect.bottom_right();
    case FineTunePosition::kBottomCenter:
      return rect.bottom_center();
    case FineTunePosition::kBottomLeft:
      return rect.bottom_left();
    case FineTunePosition::kLeftCenter:
      return rect.left_center();
    default:
      break;
  }

  NOTREACHED();
  return gfx::Point();
}

bool IsCornerFineTunePosition(FineTunePosition position) {
  switch (position) {
    case FineTunePosition::kTopLeft:
    case FineTunePosition::kTopRight:
    case FineTunePosition::kBottomRight:
    case FineTunePosition::kBottomLeft:
      return true;
    default:
      break;
  }
  return false;
}

StopRecordingButtonTray* GetStopRecordingButtonForRoot(aura::Window* root) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  // Recording can end when a display being fullscreen-captured gets removed, in
  // this case, we don't need to hide the button.
  if (root->is_destroying())
    return nullptr;

  // Can be null while shutting down.
  auto* root_window_controller = RootWindowController::ForWindow(root);
  if (!root_window_controller)
    return nullptr;

  auto* stop_recording_button = root_window_controller->GetStatusAreaWidget()
                                    ->stop_recording_button_tray();
  DCHECK(stop_recording_button);
  return stop_recording_button;
}

void SetStopRecordingButtonVisibility(aura::Window* root, bool visible) {
  if (auto* stop_recording_button = GetStopRecordingButtonForRoot(root))
    stop_recording_button->SetVisiblePreferred(visible);
}

void TriggerAccessibilityAlert(const std::string& message) {
  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(message);
}

void TriggerAccessibilityAlert(int message_id) {
  TriggerAccessibilityAlert(l10n_util::GetStringUTF8(message_id));
}

void TriggerAccessibilityAlertSoon(const std::string& message) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AccessibilityControllerImpl::TriggerAccessibilityAlertWithMessage,
          Shell::Get()->accessibility_controller()->GetWeakPtr(), message));
}

void TriggerAccessibilityAlertSoon(int message_id) {
  TriggerAccessibilityAlertSoon(l10n_util::GetStringUTF8(message_id));
}

CameraPreviewSnapPosition GetCameraNextHorizontalSnapPosition(
    CameraPreviewSnapPosition current,
    bool going_left) {
  switch (current) {
    case CameraPreviewSnapPosition::kTopLeft:
      return going_left ? current : CameraPreviewSnapPosition::kTopRight;
    case CameraPreviewSnapPosition::kTopRight:
      return going_left ? CameraPreviewSnapPosition::kTopLeft : current;
    case CameraPreviewSnapPosition::kBottomLeft:
      return going_left ? current : CameraPreviewSnapPosition::kBottomRight;
    case CameraPreviewSnapPosition::kBottomRight:
      return going_left ? CameraPreviewSnapPosition::kBottomLeft : current;
  }
}

CameraPreviewSnapPosition GetCameraNextVerticalSnapPosition(
    CameraPreviewSnapPosition current,
    bool going_up) {
  switch (current) {
    case CameraPreviewSnapPosition::kTopLeft:
      return going_up ? current : CameraPreviewSnapPosition::kBottomLeft;
    case CameraPreviewSnapPosition::kTopRight:
      return going_up ? current : CameraPreviewSnapPosition::kBottomRight;
    case CameraPreviewSnapPosition::kBottomLeft:
      return going_up ? CameraPreviewSnapPosition::kTopLeft : current;
    case CameraPreviewSnapPosition::kBottomRight:
      return going_up ? CameraPreviewSnapPosition::kTopRight : current;
  }
}

std::unique_ptr<views::View> CreateClipboardShortcutView() {
  std::unique_ptr<views::View> clipboard_shortcut_view =
      std::make_unique<views::View>();

  clipboard_shortcut_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  const std::u16string shortcut_key = l10n_util::GetStringUTF16(
      Shell::Get()->keyboard_capability()->HasLauncherButtonOnAnyKeyboard()
          ? IDS_ASH_SHORTCUT_MODIFIER_LAUNCHER
          : IDS_ASH_SHORTCUT_MODIFIER_SEARCH);

  const std::u16string label_text = l10n_util::GetStringFUTF16(
      IDS_ASH_MULTIPASTE_SCREENSHOT_NOTIFICATION_NUDGE, shortcut_key);

  views::Label* shortcut_label =
      clipboard_shortcut_view->AddChildView(std::make_unique<views::Label>());
  shortcut_label->SetText(label_text);
  shortcut_label->SetBackgroundColorId(
      chromeos::features::IsJellyEnabled()
          ? cros_tokens::kCrosSysPrimary
          : static_cast<ui::ColorId>(kColorAshControlBackgroundColorActive));
  shortcut_label->SetEnabledColorId(
      chromeos::features::IsJellyEnabled()
          ? cros_tokens::kCrosSysOnPrimary
          : static_cast<ui::ColorId>(kColorAshTextOnBackgroundColor));
  if (chromeos::features::IsJellyEnabled()) {
    ash::TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosBody2,
                                               *shortcut_label);
  }
  return clipboard_shortcut_view;
}

// Creates the banner view that will show on top of the notification image.
std::unique_ptr<views::View> CreateBannerView() {
  std::unique_ptr<views::View> banner_view = std::make_unique<views::View>();

  auto* layout =
      banner_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::VH(kBannerVerticalInsetDip, kBannerHorizontalInsetDip),
          kBannerIconTextSpacingDip));

  const ui::ColorId background_color_id =
      chromeos::features::IsJellyEnabled()
          ? cros_tokens::kCrosSysPrimary
          : static_cast<ui::ColorId>(kColorAshControlBackgroundColorActive);
  banner_view->SetBackground(views::CreateThemedRoundedRectBackground(
      background_color_id, kBannerViewTopRadius, kBannerViewBottomRadius,
      /*for_border_thickness=*/0));

  views::ImageView* icon =
      banner_view->AddChildView(std::make_unique<views::ImageView>());
  icon->SetImage(ui::ImageModel::FromVectorIcon(
      kCaptureModeCopiedToClipboardIcon,
      chromeos::features::IsJellyEnabled()
          ? cros_tokens::kCrosSysOnPrimary
          : static_cast<ui::ColorId>(kColorAshIconOnBackgroundColor),
      kBannerIconSizeDip));

  views::Label* label = banner_view->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_SCREEN_CAPTURE_SCREENSHOT_COPIED_TO_CLIPBOARD)));
  label->SetBackgroundColorId(kColorAshControlBackgroundColorActive);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColorId(
      chromeos::features::IsJellyEnabled()
          ? cros_tokens::kCrosSysOnPrimary
          : static_cast<ui::ColorId>(kColorAshTextOnBackgroundColor));
  if (chromeos::features::IsJellyEnabled()) {
    ash::TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosBody2,
                                               *label);
  }

  if (!Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    banner_view->AddChildView(CreateClipboardShortcutView());
    layout->SetFlexForView(label, 1);

    // Notify the clipboard history of the created notification.
    ClipboardHistoryController::Get()->OnScreenshotNotificationCreated();
  }
  return banner_view;
}

// Creates the play icon view which shows on top of the video thumbnail in the
// notification.
std::unique_ptr<views::View> CreatePlayIconView() {
  auto play_view = std::make_unique<views::ImageView>();
  play_view->SetImage(ui::ImageModel::FromVectorIcon(
      kCaptureModePlayIcon, kColorAshIconColorPrimary, kPlayIconSizeDip));
  play_view->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  play_view->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  play_view->SetBackground(views::CreateThemedRoundedRectBackground(
      kColorAshShieldAndBase80, kPlayIconBackgroundCornerRadiusDip));
  return play_view;
}

gfx::Point GetLocalCenterPoint(ui::Layer* layer) {
  return gfx::Rect(layer->GetTargetBounds().size()).CenterPoint();
}

gfx::Transform GetScaleTransformAboutCenter(ui::Layer* layer, float scale) {
  return gfx::GetScaleTransform(GetLocalCenterPoint(layer), scale);
}

CameraPreviewSizeSpecs CalculateCameraPreviewSizeSpecs(
    const gfx::Size& confine_bounds_size,
    bool is_collapsed) {
  // We divide the shorter side of the confine bounds by a divider to calculate
  // the expanded diameter. Note that both expanded and collapsed diameters are
  // clamped at a minimum value of `kMinCameraPreviewDiameter`.
  const int short_side =
      std::min(confine_bounds_size.width(), confine_bounds_size.height());
  const int expanded_diameter =
      std::max(short_side / capture_mode::kCaptureSurfaceShortSideDivider,
               capture_mode::kMinCameraPreviewDiameter);

  // If the expanded diameter is below a certain threshold, we consider it too
  // small to allow it to collapse, and in that case the resize button will be
  // hidden.
  const bool is_collapsible =
      expanded_diameter >= capture_mode::kMinCollapsibleCameraPreviewDiameter;

  // Pick the actual diameter based on whether the preview is currently expanded
  // or collapsed.
  const int diameter =
      !is_collapsed
          ? expanded_diameter
          : std::max(expanded_diameter / capture_mode::kCollapsedPreviewDivider,
                     capture_mode::kMinCameraPreviewDiameter);

  bool is_surface_too_small = false;
  const bool should_be_visible =
      CalculateCameraPreviewTargetVisibility(short_side, &is_surface_too_small);

  // If the surface was determined to be too small, the preview should be
  // hidden.
  DCHECK(!is_surface_too_small || !should_be_visible);

  return CameraPreviewSizeSpecs{gfx::Size(diameter, diameter), is_collapsible,
                                should_be_visible, is_surface_too_small};
}

aura::Window* GetTopMostCapturableWindowAtPoint(
    const gfx::Point& screen_point) {
  auto* controller = CaptureModeController::Get();
  std::set<aura::Window*> ignore_windows;
  auto* camera_controller = controller->camera_controller();
  if (auto* camera_preview_widget = camera_controller->camera_preview_widget())
    ignore_windows.insert(camera_preview_widget->GetNativeWindow());

  if (controller->IsActive()) {
    auto* capture_session = controller->capture_mode_session();
    DCHECK(capture_session->capture_mode_bar_widget());
    ignore_windows.insert(
        capture_session->capture_mode_bar_widget()->GetNativeWindow());

    if (auto* capture_settings_widget =
            capture_session->capture_mode_settings_widget()) {
      ignore_windows.insert(capture_settings_widget->GetNativeWindow());
    }

    if (auto* capture_label_widget = capture_session->capture_label_widget())
      ignore_windows.insert(capture_label_widget->GetNativeWindow());

    if (auto* capture_toast_widget = capture_session->capture_toast_controller()
                                         ->capture_toast_widget()) {
      ignore_windows.insert(capture_toast_widget->GetNativeWindow());
    }
  }

  return GetTopmostWindowAtPoint(screen_point, ignore_windows);
}

bool GetWidgetCurrentVisibility(views::Widget* widget) {
  // Note that we use `aura::Window::TargetVisibility()` rather than
  // `views::Widget::IsVisible()` (which in turn uses
  // `aura::Window::IsVisible()`). The reason is because the latter takes into
  // account whether window's layer is drawn or not. We want to calculate the
  // current visibility only based on the actual visibility of the window
  // itself, so that we can correctly compare it against `target_visibility`.
  // Note that the widget may be a child of the unparented container (which is
  // always hidden), yet the native window is shown.
  return widget->GetNativeWindow()->TargetVisibility() &&
         widget->GetLayer()->GetTargetOpacity() > 0.f;
}

bool SetWidgetVisibility(views::Widget* widget,
                         bool target_visibility,
                         absl::optional<AnimationParams> animation_params) {
  DCHECK(widget);
  if (target_visibility == GetWidgetCurrentVisibility(widget))
    return false;

  if (animation_params) {
    if (target_visibility)
      FadeInWidget(widget, *animation_params);
    else
      FadeOutWidget(widget, *animation_params);
  } else {
    if (target_visibility)
      widget->Show();
    else
      widget->Hide();
  }
  return true;
}

aura::Window* GetPreferredRootWindow(
    absl::optional<gfx::Point> location_in_screen) {
  const int64_t display_id =
      (location_in_screen
           ? display::Screen::GetScreen()->GetDisplayNearestPoint(
                 *location_in_screen)
           : Shell::Get()->cursor_manager()->GetDisplay())
          .id();

  // The Display object returned by `CursorManager::GetDisplay()` may be stale,
  // but will have the correct id.
  DCHECK_NE(display::kInvalidDisplayId, display_id);
  auto* root = Shell::GetRootWindowForDisplayId(display_id);
  return root ? root : Shell::GetPrimaryRootWindow();
}

void ConfigLabelView(views::Label* label_view) {
  label_view->SetEnabledColorId(kColorAshTextColorPrimary);
  label_view->SetBackgroundColor(SK_ColorTRANSPARENT);
  label_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_view->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_MIDDLE);
}

views::BoxLayout* CreateAndInitBoxLayoutForView(views::View* view) {
  auto* box_layout = view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      capture_mode::kBetweenChildSpacing));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  return box_layout;
}

void MaybeUpdateCaptureModePrivacyIndicators() {
  if (!features::IsPrivacyIndicatorsEnabled()) {
    return;
  }

  auto* controller = CaptureModeController::Get();
  const bool is_camera_used =
      !!controller->camera_controller()->camera_preview_widget();
  const bool is_microphone_used = controller->IsAudioRecordingInProgress();

  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      /*app_id=*/kCaptureModePrivacyIndicatorsId,
      /*app_name=*/
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAPTURE_MODE_BUTTON_LABEL),
      is_camera_used, is_microphone_used, /*delegate=*/
      base::MakeRefCounted<PrivacyIndicatorsNotificationDelegate>(),
      PrivacyIndicatorsSource::kScreenCapture);
}

ui::ColorProvider* GetColorProviderForNativeTheme() {
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  return ui::ColorProviderManager::Get().GetColorProviderFor(
      native_theme->GetColorProviderKey(nullptr));
}

bool IsEventTargetedOnWidget(const ui::LocatedEvent& event,
                             views::Widget* widget) {
  auto* target = static_cast<aura::Window*>(event.target());
  return widget && widget->GetNativeWindow()->Contains(target);
}

gfx::Rect CalculateHighlightLayerBounds(const gfx::PointF& center_point,
                                        int highlight_layer_radius) {
  return gfx::Rect(center_point.x() - highlight_layer_radius,
                   center_point.y() - highlight_layer_radius,
                   highlight_layer_radius * 2, highlight_layer_radius * 2);
}

void SetHighlightBorder(views::View* view,
                        int corner_radius,
                        views::HighlightBorder::Type type) {
  view->SetBorder(
      std::make_unique<views::HighlightBorder>(corner_radius, type));
}

}  // namespace ash::capture_mode_util
