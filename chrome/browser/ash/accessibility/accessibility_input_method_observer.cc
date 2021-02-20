// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/accessibility_input_method_observer.h"

#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

AccessibilityInputMethodObserver::AccessibilityInputMethodObserver(
    ui::InputMethod* input_method)
    : input_method_(input_method) {
  input_method_->AddObserver(this);
}

AccessibilityInputMethodObserver::~AccessibilityInputMethodObserver() {
  input_method_->RemoveObserver(this);
}

void AccessibilityInputMethodObserver::ResetCaretBounds() {
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  if (!accessibility_manager)  // Null in unit tests.
    return;

  // Reset the bounds when the IME session ends (e.g. when a remote app closes).
  accessibility_manager->SetCaretBounds(gfx::Rect());
}

void AccessibilityInputMethodObserver::OnCaretBoundsChanged(
    const ui::TextInputClient* client) {
  UpdateCaretBounds(client);
}

void AccessibilityInputMethodObserver::OnTextInputStateChanged(
    const ui::TextInputClient* client) {
  UpdateCaretBounds(client);
}

void AccessibilityInputMethodObserver::UpdateCaretBounds(
    const ui::TextInputClient* client) {
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  if (!accessibility_manager)  // Null in unit tests.
    return;

  if (!client || client->GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE)
    accessibility_manager->SetCaretBounds(gfx::Rect());
  else
    accessibility_manager->SetCaretBounds(client->GetCaretBounds());
}

}  // namespace ash
