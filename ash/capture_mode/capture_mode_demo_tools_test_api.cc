// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_demo_tools_test_api.h"

#include <vector>

#include "ash/capture_mode/capture_mode_demo_tools_controller.h"
#include "ash/capture_mode/key_combo_view.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

CaptureModeDemoToolsTestApi::CaptureModeDemoToolsTestApi(
    CaptureModeDemoToolsController* demo_tools_controller)
    : demo_tools_controller_(demo_tools_controller) {}

views::Widget* CaptureModeDemoToolsTestApi::GetKeyComboWidget() {
  DCHECK(demo_tools_controller_);
  return demo_tools_controller_->key_combo_widget_.get();
}

KeyComboView* CaptureModeDemoToolsTestApi::GetKeyComboView() {
  DCHECK(demo_tools_controller_);
  return demo_tools_controller_->key_combo_view_;
}

int CaptureModeDemoToolsTestApi::GetCurrentModifiersFlags() {
  DCHECK(demo_tools_controller_);
  return demo_tools_controller_->modifiers_;
}

ui::KeyboardCode CaptureModeDemoToolsTestApi::GetLastNonModifierKey() {
  DCHECK(demo_tools_controller_);
  return demo_tools_controller_->last_non_modifier_key_;
}

std::vector<ui::KeyboardCode>
CaptureModeDemoToolsTestApi::GetShownModifiersKeyCodes() {
  DCHECK(demo_tools_controller_);
  KeyComboView* key_combo_view = demo_tools_controller_->key_combo_view_;

  if (!key_combo_view || !key_combo_view->modifiers_container_view_) {
    return std::vector<ui::KeyboardCode>();
  }

  return key_combo_view->GetModifierKeycodeVector();
}

ui::KeyboardCode CaptureModeDemoToolsTestApi::GetShownNonModifierKeyCode() {
  DCHECK(demo_tools_controller_);
  KeyComboView* key_combo_view = demo_tools_controller_->key_combo_view_;

  if (key_combo_view == nullptr)
    return ui::VKEY_UNKNOWN;

  return key_combo_view->last_non_modifier_key_;
}

base::OneShotTimer* CaptureModeDemoToolsTestApi::GetRefreshKeyComboTimer() {
  DCHECK(demo_tools_controller_);
  return &(demo_tools_controller_->key_up_refresh_timer_);
}

views::ImageView* CaptureModeDemoToolsTestApi::GetNonModifierKeyItemIcon() {
  DCHECK(demo_tools_controller_);
  KeyComboView* key_combo_view = demo_tools_controller_->key_combo_view_;

  if (!key_combo_view || !key_combo_view->non_modifier_view_)
    return nullptr;

  return key_combo_view->non_modifier_view_->icon();
}

void CaptureModeDemoToolsTestApi::SetOnMouseHighlightAnimationEndedCallback(
    base::OnceClosure callback) {
  DCHECK(demo_tools_controller_);
  demo_tools_controller_
      ->on_mouse_highlight_animation_ended_callback_for_test_ =
      std::move(callback);
}

const MouseHighlightLayers&
CaptureModeDemoToolsTestApi::GetMouseHighlightLayers() const {
  DCHECK(demo_tools_controller_);
  return demo_tools_controller_->mouse_highlight_layers_;
}

const TouchHighlightLayersMap&
CaptureModeDemoToolsTestApi::GetTouchIdToHighlightLayerMap() const {
  DCHECK(demo_tools_controller_);
  return demo_tools_controller_->touch_pointer_id_to_highlight_layer_map_;
}

}  // namespace ash