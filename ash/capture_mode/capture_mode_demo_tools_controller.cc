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
#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/location.h"
#include "base/notreached.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr float kHighlightLayerFinalOpacity = 0.f;
constexpr float kHighlightLayerInitialScale = 0.1f;
constexpr float kHighlightLayerFinalScale = 1.0f;
constexpr float kTouchHighlightLayerTouchDownScale = 56.f / 72;
constexpr base::TimeDelta kMouseScaleUpDuration = base::Milliseconds(1500);
constexpr base::TimeDelta kTouchDownScaleUpDuration = base::Milliseconds(200);
constexpr base::TimeDelta kTouchUpScaleUpDuration = base::Milliseconds(1000);

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
  params.name = "KeyComboWidget";
  return params;
}

}  // namespace

CaptureModeDemoToolsController::CaptureModeDemoToolsController(
    VideoRecordingWatcher* video_recording_watcher)
    : video_recording_watcher_(video_recording_watcher) {
  ui::InputMethod* input_method =
      Shell::Get()->window_tree_host_manager()->input_method();
  input_method->AddObserver(this);
  UpdateTextInputType(input_method->GetTextInputClient());
}

CaptureModeDemoToolsController::~CaptureModeDemoToolsController() {
  Shell::Get()->window_tree_host_manager()->input_method()->RemoveObserver(
      this);
}

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
      .SetDuration(kMouseScaleUpDuration)
      .SetTransform(highlight_layer, scale_up_transform,
                    gfx::Tween::ACCEL_0_40_DECEL_100)
      .SetOpacity(highlight_layer, kHighlightLayerFinalOpacity,
                  gfx::Tween::ACCEL_0_80_DECEL_80);
}

void CaptureModeDemoToolsController::RefreshBounds() {
  if (key_combo_widget_) {
    key_combo_widget_->SetBounds(CalculateKeyComboWidgetBounds());
  }
}

void CaptureModeDemoToolsController::OnTouchEvent(
    ui::EventType event_type,
    ui::PointerId pointer_id,
    const gfx::PointF& event_location_in_window) {
  switch (event_type) {
    case ui::ET_TOUCH_PRESSED: {
      OnTouchDown(pointer_id, event_location_in_window);
      return;
    }
    case ui::ET_TOUCH_RELEASED:
    case ui::ET_TOUCH_CANCELLED: {
      OnTouchUp(pointer_id, event_location_in_window);
      return;
    }
    case ui::ET_TOUCH_MOVED: {
      OnTouchDragged(pointer_id, event_location_in_window);
      return;
    }
    default:
      NOTREACHED();
  }
}

void CaptureModeDemoToolsController::OnTextInputStateChanged(
    const ui::TextInputClient* client) {
  UpdateTextInputType(client);
}

void CaptureModeDemoToolsController::OnKeyUpEvent(ui::KeyEvent* event) {
  const ui::KeyboardCode key_code = event->key_code();
  const int modifier_flag = GetModifierFlagForKeyCode(key_code);
  modifiers_ &= ~modifier_flag;

  if (last_non_modifier_key_ == key_code) {
    last_non_modifier_key_ = ui::VKEY_UNKNOWN;
  }

  if (key_up_refresh_timer_.IsRunning() &&
      key_up_refresh_timer_.GetCurrentDelay() ==
          capture_mode::kRefreshKeyComboWidgetLongDelay) {
    // If the timer is running with a delay of
    // `capture_mode::kRefreshKeyComboWidgetLongDelay`, it means that the
    // non-modifier key of the key combo has recently been released with no
    // modifier keys pressed or the last modifier key has been released with
    // no non-modifier key that can be displayed when independently pressed
    // which will trigger the hide timer to hide the entire widget when it
    // expires. If there are other key up events, we want to ignore them such
    // that the key combo continues to show on the screen as a complete combo
    // until the timer expires.
    return;
  }

  const auto& target_delay =
      ShouldResetKeyComboWidget()
          ? capture_mode::kRefreshKeyComboWidgetLongDelay
          : capture_mode::kRefreshKeyComboWidgetShortDelay;

  key_up_refresh_timer_.Start(
      FROM_HERE, target_delay, this,
      &CaptureModeDemoToolsController::RefreshKeyComboViewer);
}

void CaptureModeDemoToolsController::OnKeyDownEvent(ui::KeyEvent* event) {
  // We will not show key combo widget if the cursor is in the input text field
  // to respect the user privacy. This check needs to be placed after checking
  // the key up event as the key combo widget on display will still need to be
  // refreshed.
  if (in_text_input_) {
    return;
  }

  const ui::KeyboardCode key_code = event->key_code();

  // Return directly if it is a repeated key event for non-modifier key.
  if (key_code == last_non_modifier_key_)
    return;

  key_up_refresh_timer_.Stop();

  const int modifier_flag = GetModifierFlagForKeyCode(key_code);
  modifiers_ |= modifier_flag;

  if (modifier_flag == ui::EF_NONE)
    last_non_modifier_key_ = key_code;

  RefreshKeyComboViewer();
}

void CaptureModeDemoToolsController::RefreshKeyComboViewer() {
  if (ShouldResetKeyComboWidget()) {
    AnimateToResetKeyComboWidget();
    return;
  }

  if (!key_combo_widget_) {
    key_combo_widget_ = std::make_unique<views::Widget>();
    key_combo_widget_->Init(CreateWidgetParams(video_recording_watcher_));
    key_combo_view_ =
        key_combo_widget_->SetContentsView(std::make_unique<KeyComboView>());
    key_combo_widget_->SetVisibilityAnimationTransition(
        views::Widget::ANIMATE_NONE);
    ui::Layer* layer = key_combo_widget_->GetLayer();
    layer->SetFillsBoundsOpaquely(false);
    layer->SetMasksToBounds(true);
    key_combo_widget_->Show();
  }

  key_combo_view_->RefreshView(modifiers_, last_non_modifier_key_);
  RefreshBounds();
}

gfx::Rect CaptureModeDemoToolsController::CalculateKeyComboWidgetBounds()
    const {
  const gfx::Size preferred_size = key_combo_view_->GetPreferredSize();
  const auto confine_bounds =
      video_recording_watcher_->GetCaptureSurfaceConfineBounds();
  const int key_combo_x =
      preferred_size.width() > confine_bounds.width()
          ? confine_bounds.right() - preferred_size.width() -
                capture_mode::kKeyWidgetBorderPadding
          : confine_bounds.CenterPoint().x() - preferred_size.width() / 2;
  const int key_combo_y = confine_bounds.bottom() -
                          capture_mode::kKeyWidgetDistanceFromBottom -
                          preferred_size.height();
  return gfx::Rect(gfx::Point(key_combo_x, key_combo_y), preferred_size);
}

bool CaptureModeDemoToolsController::ShouldResetKeyComboWidget() const {
  return (modifiers_ == 0) && !ShouldConsiderKey(last_non_modifier_key_);
}

void CaptureModeDemoToolsController::AnimateToResetKeyComboWidget() {
  // TODO(http://b/258349669): apply animation to the hide process when the
  // specs are ready.
  key_combo_widget_.reset();
  key_combo_view_ = nullptr;
}

void CaptureModeDemoToolsController::UpdateTextInputType(
    const ui::TextInputClient* client) {
  in_text_input_ =
      client && client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE;
}

void CaptureModeDemoToolsController::OnMouseHighlightAnimationEnded(
    PointerHighlightLayer* pointer_highlight_layer_ptr) {
  base::EraseIf(mouse_highlight_layers_,
                base::MatchesUniquePtr(pointer_highlight_layer_ptr));

  if (on_mouse_highlight_animation_ended_callback_for_test_)
    std::move(on_mouse_highlight_animation_ended_callback_for_test_).Run();
}

void CaptureModeDemoToolsController::OnTouchDown(
    const ui::PointerId& pointer_id,
    const gfx::PointF& event_location_in_window) {
  std::unique_ptr<PointerHighlightLayer> touch_highlight_layer =
      std::make_unique<PointerHighlightLayer>(
          event_location_in_window,
          video_recording_watcher_->GetOnCaptureSurfaceWidgetParentWindow()
              ->layer());
  ui::Layer* highlight_layer = touch_highlight_layer->layer();
  highlight_layer->SetTransform(capture_mode_util::GetScaleTransformAboutCenter(
      highlight_layer, kHighlightLayerInitialScale));
  touch_pointer_id_to_highlight_layer_map_.emplace(
      pointer_id, std::move(touch_highlight_layer));

  const gfx::Transform scale_up_transform =
      capture_mode_util::GetScaleTransformAboutCenter(
          highlight_layer, kTouchHighlightLayerTouchDownScale);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kTouchDownScaleUpDuration)
      .SetTransform(highlight_layer, scale_up_transform,
                    gfx::Tween::ACCEL_0_40_DECEL_100);
}

void CaptureModeDemoToolsController::OnTouchUp(
    const ui::PointerId& pointer_id,
    const gfx::PointF& event_location_in_window) {
  auto iter = touch_pointer_id_to_highlight_layer_map_.find(pointer_id);
  DCHECK(iter != touch_pointer_id_to_highlight_layer_map_.end());

  std::unique_ptr<PointerHighlightLayer> touch_highlight_layer =
      std::move(iter->second);
  touch_pointer_id_to_highlight_layer_map_.erase(pointer_id);

  ui::Layer* highlight_layer = touch_highlight_layer->layer();
  DCHECK(highlight_layer);

  const gfx::Transform scale_up_transform =
      capture_mode_util::GetScaleTransformAboutCenter(
          highlight_layer, kHighlightLayerFinalScale);

  views::AnimationBuilder()
      .OnEnded(base::BindOnce(
          [](std::unique_ptr<PointerHighlightLayer> touch_highlight_layer) {},
          std::move(touch_highlight_layer)))
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kTouchUpScaleUpDuration)
      .SetTransform(highlight_layer, scale_up_transform,
                    gfx::Tween::ACCEL_0_40_DECEL_100)
      .SetOpacity(highlight_layer, kHighlightLayerFinalOpacity,
                  gfx::Tween::ACCEL_0_80_DECEL_80);
}

void CaptureModeDemoToolsController::OnTouchDragged(
    const ui::PointerId& pointer_id,
    const gfx::PointF& event_location_in_window) {
  auto* highlight_layer =
      touch_pointer_id_to_highlight_layer_map_[pointer_id].get();
  DCHECK(highlight_layer);
  highlight_layer->CenterAroundPoint(event_location_in_window);
}

}  // namespace ash