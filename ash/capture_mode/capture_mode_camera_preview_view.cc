// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_camera_preview_view.h"

#include "ash/accessibility/scoped_a11y_override_window_setter.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The duration for the resize button fading in process.
constexpr base::TimeDelta kResizeButtonFadeInDuration = base::Milliseconds(150);

// The duration for the reize button fading out process.
constexpr base::TimeDelta kResizeButtonFadeOutDuration = base::Milliseconds(50);

const gfx::VectorIcon& GetIconOfResizeButton(
    const bool is_camera_preview_collapsed) {
  return is_camera_preview_collapsed ? kCaptureModeCameraPreviewExpandIcon
                                     : kCaptureModeCameraPreviewCollapseIcon;
}

bool IsArrowKeyEvent(const ui::KeyEvent* event) {
  const ui::KeyboardCode key_code = event->key_code();
  return key_code == ui::VKEY_DOWN || key_code == ui::VKEY_RIGHT ||
         key_code == ui::VKEY_LEFT || key_code == ui::VKEY_UP;
}

}  // namespace

// -----------------------------------------------------------------------------
// CameraPreviewResizeButton:

CameraPreviewResizeButton::CameraPreviewResizeButton(
    CameraPreviewView* camera_preview_view,
    views::Button::PressedCallback callback,
    const gfx::VectorIcon& icon)
    : IconButton(std::move(callback),
                 IconButton::Type::kMediumFloating,
                 &icon,
                 // the tooltip depends on the current expanded state of the
                 // button and will be updated by `CameraPreviewView`.
                 /*accessible_name=*/u"",
                 /*is_togglable=*/false,
                 /*has_border=*/true),
      camera_preview_view_(camera_preview_view) {}

CameraPreviewResizeButton::~CameraPreviewResizeButton() = default;

views::View* CameraPreviewResizeButton::GetView() {
  return this;
}

void CameraPreviewResizeButton::PseudoFocus() {
  DCHECK(camera_preview_view_->is_collapsible());

  CaptureModeSessionFocusCycler::HighlightableView::PseudoFocus();
  camera_preview_view_->RefreshResizeButtonVisibility();
}

void CameraPreviewResizeButton::PseudoBlur() {
  CaptureModeSessionFocusCycler::HighlightableView::PseudoBlur();
  camera_preview_view_->ScheduleRefreshResizeButtonVisibility();
}

BEGIN_METADATA(CameraPreviewResizeButton)
END_METADATA

// -----------------------------------------------------------------------------
// CameraPreviewView:

CameraPreviewView::CameraPreviewView(
    CaptureModeCameraController* camera_controller,
    const CameraId& camera_id,
    mojo::Remote<video_capture::mojom::VideoSource> camera_video_source,
    const media::VideoCaptureFormat& capture_format,
    bool should_flip_frames_horizontally)
    : camera_controller_(camera_controller),
      camera_id_(camera_id),
      camera_video_renderer_(std::move(camera_video_source),
                             capture_format,
                             should_flip_frames_horizontally),
      camera_video_host_view_(
          AddChildView(std::make_unique<views::NativeViewHost>())),
      resize_button_(AddChildView(std::make_unique<CameraPreviewResizeButton>(
          this,
          base::BindRepeating(&CameraPreviewView::OnResizeButtonPressed,
                              base::Unretained(this)),
          GetIconOfResizeButton(
              camera_controller_->is_camera_preview_collapsed())))),
      scoped_a11y_overrider_(
          std::make_unique<ScopedA11yOverrideWindowSetter>()) {
  // Add an empty border to the camera preview. This is done to keep some gap
  // between the focus ring and the contents of the camera preview, as focus
  // ring will extend a little beyond the border.
  SetBorder(views::CreateEmptyBorder(capture_mode::kCameraPreviewBorderSize));

  resize_button_->SetPaintToLayer();
  resize_button_->layer()->SetFillsBoundsOpaquely(false);
  resize_button_->SetBackground(views::CreateThemedRoundedRectBackground(
      kColorAshShieldAndBase80,
      resize_button_->GetPreferredSize().height() / 2.f));

  accessibility_observation_.Observe(Shell::Get()->accessibility_controller());
  RefreshResizeButtonVisibility();
  UpdateResizeButtonTooltip();

  GetViewAccessibility().SetRole(ax::mojom::Role::kVideo);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_CAMERA_PREVIEW_FOCUSED));
}

CameraPreviewView::~CameraPreviewView() {
  auto* controller = CaptureModeController::Get();
  if (controller->IsActive() && !controller->is_recording_in_progress())
    controller->capture_mode_session()->OnCameraPreviewDestroyed();
  capture_mode_util::MaybeUpdateCaptureModePrivacyIndicators();
  controller->MaybeUpdateVcPanel();
}

void CameraPreviewView::Initialize() {
  CHECK(GetWidget()) << "The view should have been added to a widget first.";

  camera_video_renderer_.Initialize();

  capture_mode_util::MaybeUpdateCaptureModePrivacyIndicators();
  CaptureModeController::Get()->MaybeUpdateVcPanel();
}

void CameraPreviewView::SetIsCollapsible(bool value) {
  if (value != is_collapsible_) {
    is_collapsible_ = value;
    RefreshResizeButtonVisibility();
  }
}

bool CameraPreviewView::MaybeHandleKeyEvent(const ui::KeyEvent* event) {
  // Handle control+arrow keys to snap the camera preview to different corners
  // when it has focus.
  if (has_focus() && event->IsControlDown() && IsArrowKeyEvent(event)) {
    const CameraPreviewSnapPosition current_snap_position =
        camera_controller_->camera_preview_snap_position();
    CameraPreviewSnapPosition new_snap_position = current_snap_position;
    const ui::KeyboardCode key_code = event->key_code();
    if (key_code == ui::VKEY_LEFT || key_code == ui::VKEY_RIGHT) {
      new_snap_position =
          capture_mode_util::GetCameraNextHorizontalSnapPosition(
              current_snap_position, /*going_left=*/key_code == ui::VKEY_LEFT);
    } else {
      DCHECK(key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN);
      new_snap_position = capture_mode_util::GetCameraNextVerticalSnapPosition(
          current_snap_position, /*going_up=*/key_code == ui::VKEY_UP);
    }

    // Still call `SetCameraPreviewSnapPosition` even if the snap position does
    // not change. As we still want to trigger a11y alert in this case.
    camera_controller_->SetCameraPreviewSnapPosition(new_snap_position,
                                                     /*animate=*/true);
    return new_snap_position != current_snap_position;
  }

  auto* controller = CaptureModeController::Get();
  if (controller->IsActive() || !controller->is_recording_in_progress())
    return false;

  // Handle the key events on camera preview when it has focus (either the
  // preview view or the resize button) in video recording.
  const bool camera_preview_has_focus =
      has_focus() || resize_button_->has_focus();
  bool should_handle_event = false;
  switch (event->key_code()) {
    case ui::VKEY_TAB:
      if (camera_preview_has_focus) {
        if (resize_button_->has_focus()) {
          DCHECK(is_collapsible_);
          resize_button_->PseudoBlur();
          PseudoFocus();
        } else if (is_collapsible_) {
          PseudoBlur();
          resize_button_->PseudoFocus();
        }
        should_handle_event = true;
      }
      break;
    case ui::VKEY_SPACE:
      if (resize_button_->has_focus()) {
        resize_button_->ClickView();
        should_handle_event = true;
      }
      break;
    case ui::VKEY_ESCAPE:
      if (camera_preview_has_focus) {
        BlurA11yFocus();
        should_handle_event = true;
      }
      break;
    default:
      break;
  }
  return should_handle_event;
}

void CameraPreviewView::RefreshResizeButtonVisibility() {
  const float target_opacity = CalculateResizeButtonTargetOpacity();
  if (target_opacity == resize_button_->layer()->GetTargetOpacity())
    return;

  resize_button_hide_timer_.Stop();
  if (target_opacity == 1.f) {
    FadeInResizeButton();
    ScheduleRefreshResizeButtonVisibility();
  } else {
    FadeOutResizeButton();
  }
}

void CameraPreviewView::UpdateA11yOverrideWindow() {
  if (has_focus() || resize_button_->has_focus()) {
    scoped_a11y_overrider_->MaybeUpdateA11yOverrideWindow(
        GetWidget()->GetNativeWindow());
  } else {
    scoped_a11y_overrider_->MaybeUpdateA11yOverrideWindow(nullptr);
  }
}

void CameraPreviewView::MaybeBlurFocus(const ui::MouseEvent& event) {
  auto* target = static_cast<aura::Window*>(event.target());
  if (!GetWidget()->GetNativeWindow()->Contains(target))
    BlurA11yFocus();
}

void CameraPreviewView::AddedToWidget() {
  camera_video_host_view_->Attach(camera_video_renderer_.host_window());
  // This must be called after the renderer's `host_window()` has been attached
  // to the `NativeViewHost`, and before we call `Initialize()`, since
  // `Initialize()` will create a layer tree frame sink for the `host_window()`
  // and we're not allowed to change the event targeting policy after that.
  DisableEventHandlingInCameraVideoHostHierarchy();
}

bool CameraPreviewView::OnMousePressed(const ui::MouseEvent& event) {
  camera_controller_->StartDraggingPreview(
      capture_mode_util::GetEventScreenLocation(event));
  return true;
}

bool CameraPreviewView::OnMouseDragged(const ui::MouseEvent& event) {
  camera_controller_->ContinueDraggingPreview(
      capture_mode_util::GetEventScreenLocation(event));
  return true;
}

void CameraPreviewView::OnMouseReleased(const ui::MouseEvent& event) {
  camera_controller_->EndDraggingPreview(
      capture_mode_util::GetEventScreenLocation(event),
      /*is_touch=*/false);
}

void CameraPreviewView::OnGestureEvent(ui::GestureEvent* event) {
  const gfx::PointF screen_location =
      capture_mode_util::GetEventScreenLocation(*event);

  switch (event->type()) {
    case ui::EventType::kGestureScrollBegin:
      camera_controller_->StartDraggingPreview(screen_location);
      break;
    case ui::EventType::kGestureScrollUpdate:
      DCHECK(camera_controller_->is_drag_in_progress());
      camera_controller_->ContinueDraggingPreview(screen_location);
      break;
    case ui::EventType::kScrollFlingStart:
      // TODO(conniekxu): Handle fling event.
      break;
    case ui::EventType::kGestureScrollEnd:
      DCHECK(camera_controller_->is_drag_in_progress());
      camera_controller_->EndDraggingPreview(screen_location,
                                             /*is_touch=*/true);
      break;
    case ui::EventType::kGestureEnd:
      if (camera_controller_->is_drag_in_progress()) {
        camera_controller_->EndDraggingPreview(screen_location,
                                               /*is_touch=*/true);
      }
      break;
    case ui::EventType::kGestureTap:
      has_been_tapped_ = true;
      RefreshResizeButtonVisibility();
      has_been_tapped_ = false;
      break;
    default:
      break;
  }

  event->StopPropagation();
  event->SetHandled();
}

void CameraPreviewView::OnMouseEntered(const ui::MouseEvent& event) {
  RefreshResizeButtonVisibility();
}

void CameraPreviewView::OnMouseExited(const ui::MouseEvent& event) {
  ScheduleRefreshResizeButtonVisibility();
}

void CameraPreviewView::Layout(PassKey) {
  const gfx::Size resize_button_size = resize_button_->GetPreferredSize();
  const gfx::Rect bounds(
      (width() - resize_button_size.width()) / 2.f,
      height() - resize_button_size.height() -
          capture_mode::kSpaceBetweenResizeButtonAndCameraPreview -
          capture_mode::kCameraPreviewBorderSize,
      resize_button_size.width(), resize_button_size.height());
  resize_button_->SetBoundsRect(bounds);

  camera_video_host_view_->SetBoundsRect(GetContentsBounds());

  // The size must have changed, and we need to update the rounded corners. Note
  // that the assumption is that this view must have a square size, so when
  // rounded corners are applied, it looks like a perfect circle.
  // The rounded corners is applied on the window hosting the camera frames.
  // That window's layer is a solid-color layer, so applying rounded corners on
  // it won't make it produce a render surface. This is better than applying it
  // on the preview widget's layer (which is a texture layer) and would cause a
  // a render surface. This also avoids the rendering artifacts seen in
  // https://crbug.com/1312059.
  DCHECK_EQ(width(), height());
  auto* layer = camera_video_renderer_.host_window()->layer();
  layer->SetRoundedCornerRadius(gfx::RoundedCornersF(height() / 2.f));
  layer->SetIsFastRoundedCorner(true);

  // Refocus the camera preview to relayout the focus ring on it.
  if (has_focus())
    PseudoFocus();
}

views::View* CameraPreviewView::GetView() {
  return this;
}

std::unique_ptr<views::HighlightPathGenerator>
CameraPreviewView::CreatePathGenerator() {
  return std::make_unique<views::CircleHighlightPathGenerator>(
      gfx::Insets(views::FocusRing::kDefaultHaloThickness / 2));
}

void CameraPreviewView::OnAccessibilityStatusChanged() {
  RefreshResizeButtonVisibility();
}

void CameraPreviewView::OnAccessibilityControllerShutdown() {
  accessibility_observation_.Reset();
}

void CameraPreviewView::OnResizeButtonPressed() {
  camera_controller_->ToggleCameraPreviewSize();
  UpdateResizeButton();
}

void CameraPreviewView::UpdateResizeButton() {
  resize_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(
          GetIconOfResizeButton(
              camera_controller_->is_camera_preview_collapsed()),
          kColorAshIconColorPrimary));
  UpdateResizeButtonTooltip();
}

void CameraPreviewView::UpdateResizeButtonTooltip() {
  resize_button_->SetTooltipText(l10n_util::GetStringUTF16(
      camera_controller_->is_camera_preview_collapsed()
          ? IDS_ASH_SCREEN_CAPTURE_TOOLTIP_EXPAND_SELFIE_CAMERA
          : IDS_ASH_SCREEN_CAPTURE_TOOLTIP_COLLAPSE_SELFIE_CAMERA));
}

void CameraPreviewView::DisableEventHandlingInCameraVideoHostHierarchy() {
  camera_video_host_view_->SetCanProcessEventsWithinSubtree(false);
  camera_video_host_view_->SetFocusBehavior(FocusBehavior::NEVER);
  auto* const host_window = camera_video_renderer_.host_window();
  auto* const widget_window = GetWidget()->GetNativeWindow();
  DCHECK(host_window);
  DCHECK(host_window->parent());
  DCHECK(widget_window);

  for (auto* window = host_window; window && window != widget_window;
       window = window->parent()) {
    window->SetEventTargetingPolicy(aura::EventTargetingPolicy::kNone);
  }
}

void CameraPreviewView::FadeInResizeButton() {
  resize_button_->SetVisible(true);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kResizeButtonFadeInDuration)
      .SetOpacity(resize_button_->layer(), 1.0f);
}

void CameraPreviewView::FadeOutResizeButton() {
  views::AnimationBuilder()
      .OnEnded(base::BindOnce(
          [](base::WeakPtr<CameraPreviewView> camera_preview_view) {
            if (camera_preview_view)
              camera_preview_view->resize_button()->SetVisible(false);
          },
          weak_ptr_factory_.GetWeakPtr()))
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kResizeButtonFadeOutDuration)
      .SetOpacity(resize_button_->layer(), 0.0f);
}

void CameraPreviewView::ScheduleRefreshResizeButtonVisibility() {
  resize_button_hide_timer_.Start(
      FROM_HERE, capture_mode::kResizeButtonShowDuration, this,
      &CameraPreviewView::RefreshResizeButtonVisibility);
}

float CameraPreviewView::CalculateResizeButtonTargetOpacity() {
  if (!is_collapsible_ || camera_controller_->is_drag_in_progress())
    return 0.f;

  if (Shell::Get()->accessibility_controller()->IsSwitchAccessRunning() ||
      IsMouseHovered() || resize_button_->IsMouseHovered() ||
      resize_button_->has_focus() || has_been_tapped_) {
    return 1.f;
  }

  return 0.f;
}

void CameraPreviewView::BlurA11yFocus() {
  PseudoBlur();
  resize_button_->PseudoBlur();
  UpdateA11yOverrideWindow();
}

BEGIN_METADATA(CameraPreviewView)
END_METADATA

}  // namespace ash
