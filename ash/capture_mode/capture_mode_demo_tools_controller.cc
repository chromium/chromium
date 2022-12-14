// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_demo_tools_controller.h"

#include <memory>

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/capture_mode/key_combo_view.h"
#include "ash/capture_mode/pointer_highlight_layer.h"
#include "ash/capture_mode/video_recording_watcher.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/unique_ptr_adapters.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kDistanceFromBottom = 24;

constexpr float kHighlightLayerFinalOpacity = 0.f;
constexpr float kHighlightLayerInitialScale = 0.1f;
constexpr float kHighlightLayerFinalScale = 1.0f;
constexpr base::TimeDelta kScaleUpDuration = base::Milliseconds(1500);

int GetModifierFlagForKeyCode(ui::KeyboardCode key_code) {
  switch (key_code) {
    case ui::VKEY_COMMAND:
    case ui::VKEY_RWIN:
      return ui::EF_COMMAND_DOWN;
    case ui::VKEY_CONTROL:
    case ui::VKEY_LCONTROL:
    case ui::VKEY_RCONTROL:
      return ui::EF_CONTROL_DOWN;
    case ui::VKEY_MENU:
    case ui::VKEY_LMENU:
    case ui::VKEY_RMENU:
      return ui::EF_ALT_DOWN;
    case ui::VKEY_SHIFT:
    case ui::VKEY_LSHIFT:
    case ui::VKEY_RSHIFT:
      return ui::EF_SHIFT_DOWN;
    default:
      return ui::EF_NONE;
  }
}

// Includes non-modifier keys that can be shown independently without a modifier
// key being pressed.
constexpr ui::KeyboardCode kNotNeedingModifierKeys[] = {
    ui::VKEY_COMMAND,
    ui::VKEY_RWIN,
    ui::VKEY_ESCAPE,
    ui::VKEY_TAB,
    ui::VKEY_BROWSER_BACK,
    ui::VKEY_BROWSER_FORWARD,
    ui::VKEY_BROWSER_REFRESH,
    ui::VKEY_ZOOM,
    ui::VKEY_MEDIA_LAUNCH_APP1,
    ui::VKEY_BRIGHTNESS_DOWN,
    ui::VKEY_BRIGHTNESS_UP,
    ui::VKEY_VOLUME_MUTE,
    ui::VKEY_VOLUME_DOWN,
    ui::VKEY_VOLUME_UP,
    ui::VKEY_UP,
    ui::VKEY_DOWN,
    ui::VKEY_LEFT,
    ui::VKEY_RIGHT};

// Returns true if `key_code` is a non-modifier key for which a `KeyComboViewer`
// can be shown even if there are no modifier keys are currently pressed.
bool ShouldConsiderKey(ui::KeyboardCode key_code) {
  return base::Contains(kNotNeedingModifierKeys, key_code);
}

views::Widget::InitParams CreateWidgetParams(
    VideoRecordingWatcher* video_recording_watcher) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.parent =
      video_recording_watcher->GetOnCaptureSurfaceWidgetParentWindow();
  params.child = true;
  params.name = "CaptureModeDemoToolsWidget";
  return params;
}

}  // namespace

CaptureModeDemoToolsController::CaptureModeDemoToolsController(
    VideoRecordingWatcher* video_recording_watcher)
    : video_recording_watcher_(video_recording_watcher) {}

CaptureModeDemoToolsController::~CaptureModeDemoToolsController() = default;

void CaptureModeDemoToolsController::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() == ui::ET_KEY_RELEASED) {
    OnKeyUpEvent(event);
    return;
  }

  DCHECK_EQ(event->type(), ui::ET_KEY_PRESSED);
  OnKeyDownEvent(event);
}

void CaptureModeDemoToolsController::PerformMousePressAnimation(
    const gfx::PointF& event_location_in_window) {
  std::unique_ptr<PointerHighlightLayer> mouse_highlight_layer =
      std::make_unique<PointerHighlightLayer>(
          event_location_in_window,
          video_recording_watcher_->GetOnCaptureSurfaceWidgetParentWindow()
              ->layer());
  PointerHighlightLayer* mouse_highlight_layer_ptr =
      mouse_highlight_layer.get();
  mouse_highlight_layers_.push_back(std::move(mouse_highlight_layer));

  ui::Layer* highlight_layer = mouse_highlight_layer_ptr->layer();
  highlight_layer->SetTransform(capture_mode_util::GetScaleTransformAboutCenter(
      highlight_layer, kHighlightLayerInitialScale));
  const gfx::Transform scale_up_transform =
      capture_mode_util::GetScaleTransformAboutCenter(
          highlight_layer, kHighlightLayerFinalScale);

  views::AnimationBuilder()
      .OnEnded(base::BindOnce(
          &CaptureModeDemoToolsController::OnMouseHighlightAnimationEnded,
          weak_ptr_factory_.GetWeakPtr(), mouse_highlight_layer_ptr))
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kScaleUpDuration)
      .SetTransform(highlight_layer, scale_up_transform,
                    gfx::Tween::ACCEL_0_40_DECEL_100)
      .SetOpacity(highlight_layer, kHighlightLayerFinalOpacity,
                  gfx::Tween::ACCEL_0_80_DECEL_80);
}

void CaptureModeDemoToolsController::OnKeyUpEvent(ui::KeyEvent* event) {
  const ui::KeyboardCode key_code = event->key_code();
  const int modifier_flag = GetModifierFlagForKeyCode(key_code);
  modifiers_ &= ~modifier_flag;

  // If the timer is running, it means that the non-modifier key of the
  // key combo has recently been released and the timer is about to hide the
  // entire widget when it expires. When the modifier keys of the shortcut get
  // released, we want to ignore them such that the key combo continues to show
  // on the screen as a complete combo until the timer expires.
  if (hide_timer_.IsRunning() && modifier_flag != ui::EF_NONE)
    return;

  if (last_non_modifier_key_ == key_code) {
    last_non_modifier_key_ = ui::VKEY_UNKNOWN;
    hide_timer_.Start(FROM_HERE, capture_mode::kDelayToHideKeyComboDuration,
                      this,
                      &CaptureModeDemoToolsController::AnimateToResetTheWidget);
    return;
  }

  RefreshKeyComboViewer();
}

void CaptureModeDemoToolsController::OnKeyDownEvent(ui::KeyEvent* event) {
  const ui::KeyboardCode key_code = event->key_code();

  // On any key down, we want to cancel any ongoing request to hide the widget,
  // since this is considered a new key combo other than the one the timer was
  // running for.
  hide_timer_.Stop();

  // Return directly if it is a repeated key event for non-modifier key.
  if (key_code == last_non_modifier_key_)
    return;

  const int modifier_flag = GetModifierFlagForKeyCode(key_code);
  modifiers_ |= modifier_flag;

  if (modifier_flag == ui::EF_NONE)
    last_non_modifier_key_ = key_code;

  RefreshKeyComboViewer();
}

void CaptureModeDemoToolsController::RefreshKeyComboViewer() {
  if ((modifiers_ == 0) && !ShouldConsiderKey(last_non_modifier_key_)) {
    AnimateToResetTheWidget();
    return;
  }

  if (!demo_tools_widget_) {
    demo_tools_widget_ = std::make_unique<views::Widget>();
    demo_tools_widget_->Init(CreateWidgetParams(video_recording_watcher_));
    key_combo_view_ =
        demo_tools_widget_->SetContentsView(std::make_unique<KeyComboView>());
    demo_tools_widget_->SetVisibilityAnimationTransition(
        views::Widget::ANIMATE_NONE);
    ui::Layer* layer = demo_tools_widget_->GetLayer();
    layer->SetFillsBoundsOpaquely(false);
    layer->SetMasksToBounds(true);
    demo_tools_widget_->Show();
  }

  key_combo_view_->RefreshView(modifiers_, last_non_modifier_key_);
  demo_tools_widget_->SetBounds(CalculateBounds());
}

gfx::Rect CaptureModeDemoToolsController::CalculateBounds() const {
  const gfx::Size preferred_size = key_combo_view_->GetPreferredSize();
  auto bounds = video_recording_watcher_->GetCaptureSurfaceConfineBounds();
  int demo_tools_y =
      bounds.bottom() - kDistanceFromBottom - preferred_size.height();
  bounds.ClampToCenteredSize(preferred_size);
  bounds.set_y(demo_tools_y);
  return bounds;
}

void CaptureModeDemoToolsController::AnimateToResetTheWidget() {
  // TODO(http://b/258349669): apply animation to the hide process when the
  // specs are ready.
  demo_tools_widget_.reset();
  key_combo_view_ = nullptr;
}

void CaptureModeDemoToolsController::OnMouseHighlightAnimationEnded(
    PointerHighlightLayer* pointer_highlight_layer_ptr) {
  base::EraseIf(mouse_highlight_layers_,
                base::MatchesUniquePtr(pointer_highlight_layer_ptr));

  if (on_mouse_highlight_animation_ended_callback_for_test_)
    std::move(on_mouse_highlight_animation_ended_callback_for_test_).Run();
}

}  // namespace ash