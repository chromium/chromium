// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_ACTION_ON_NEXT_FOCUS_REQUEST_H_
#define ASH_PICKER_PICKER_ACTION_ON_NEXT_FOCUS_REQUEST_H_

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/ime/input_method_observer.h"

namespace ui {
class InputMethod;
}  // namespace ui

namespace ash {

// A request to perform an action the next time an input field gains focus.
class ASH_EXPORT PickerActionOnNextFocusRequest
    : public ui::InputMethodObserver {
 public:
  // Creates a request to perform `action_callback` the next time an input field
  // gains focus. If there's no focus change within `action_timeout`, then
  // `timeout_callback` is run instead of `action_callback`. If this request is
  // destroyed before the action could happen, then neither `action_callback`
  // nor `timeout_callback` are run.
  PickerActionOnNextFocusRequest(ui::InputMethod* input_method,
                                 base::TimeDelta action_timeout,
                                 base::OnceClosure action_callback,
                                 base::OnceClosure timeout_callback);
  ~PickerActionOnNextFocusRequest() override;

  // ui::InputMethodObserver:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override;
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override;

 private:
  void OnTimeout();

  base::ScopedObservation<ui::InputMethod, ui::InputMethodObserver>
      observation_{this};
  base::OneShotTimer action_timeout_timer_;
  base::OnceClosure action_callback_;
  base::OnceClosure timeout_callback_;
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_ACTION_ON_NEXT_FOCUS_REQUEST_H_
