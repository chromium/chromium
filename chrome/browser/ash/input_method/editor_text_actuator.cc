// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_text_actuator.h"

namespace ash::input_method {

EditorTextActuator::EditorTextActuator(
    mojo::PendingAssociatedReceiver<orca::mojom::TextActuator> receiver,
    Delegate* delegate)
    : text_actuator_receiver_(this, std::move(receiver)), delegate_(delegate) {}

EditorTextActuator::~EditorTextActuator() = default;

void EditorTextActuator::InsertText(const std::string& text) {
  // We queue the text to be inserted here rather then insert it directly into
  // the input.
  inserter_.InsertTextOnNextFocus(text);
  delegate_->OnTextInserted();
}

void EditorTextActuator::OnFocus(int context_id) {
  inserter_.OnFocus(context_id);
}

void EditorTextActuator::OnBlur() {
  inserter_.OnBlur();
}
}  // namespace ash::input_method
