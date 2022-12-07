// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/caret_bounds_changed_waiter.h"

namespace ash {

CaretBoundsChangedWaiter::CaretBoundsChangedWaiter(
    ui::InputMethod* input_method)
    : input_method_(input_method) {
  input_method_->AddObserver(this);
}
CaretBoundsChangedWaiter::~CaretBoundsChangedWaiter() {
  input_method_->RemoveObserver(this);
}

void CaretBoundsChangedWaiter::Wait() {
  run_loop_.Run();
}

void CaretBoundsChangedWaiter::OnCaretBoundsChanged(
    const ui::TextInputClient* client) {
  run_loop_.Quit();
}

}  // namespace ash
