// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_CARET_BOUNDS_CHANGED_WAITER_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_CARET_BOUNDS_CHANGED_WAITER_H_

#include "base/run_loop.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/base/ime/input_method_observer.h"

namespace ash {

// A class that waits for caret bounds changed, used in tests.
class CaretBoundsChangedWaiter : public ui::InputMethodObserver {
 public:
  explicit CaretBoundsChangedWaiter(ui::InputMethod* input_method);
  CaretBoundsChangedWaiter(const CaretBoundsChangedWaiter&) = delete;
  CaretBoundsChangedWaiter& operator=(const CaretBoundsChangedWaiter&) = delete;
  ~CaretBoundsChangedWaiter() override;

  // Waits for bounds changed within the input method.
  void Wait();

 private:
  // ui::InputMethodObserver:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override {}
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override;

  ui::InputMethod* input_method_;
  base::RunLoop run_loop_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_CARET_BOUNDS_CHANGED_WAITER_H_
