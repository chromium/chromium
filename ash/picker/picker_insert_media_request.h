// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_INSERT_MEDIA_REQUEST_H_
#define ASH_PICKER_PICKER_INSERT_MEDIA_REQUEST_H_

#include <string>

#include "ash/ash_export.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/ime/input_method_observer.h"

namespace ui {
class InputMethod;
class TextInputClient;
}  // namespace ui

namespace ash {

// Inserts rich media such as text and images into an input field.
class ASH_EXPORT PickerInsertMediaRequest : public ui::InputMethodObserver {
 public:
  // Creates a request to insert `text_to_insert_` in the next focused input
  // field. If there's no focus change within `insert_timeout`, then this
  // request is cancelled. If this request is destroyed before insertion could
  // happen, the request is cancelled.
  explicit PickerInsertMediaRequest(ui::InputMethod* input_method,
                                    std::u16string_view text_to_insert,
                                    base::TimeDelta insert_timeout);
  ~PickerInsertMediaRequest() override;

  // ui::InputMethodObserver:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override;
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override;

 private:
  // Cancels the insertion if it's still pending.
  // Does nothing if the insertion has already happened.
  void CancelPendingInsert();

  std::optional<std::u16string> text_to_insert_;
  base::ScopedObservation<ui::InputMethod, ui::InputMethodObserver>
      observation_{this};
  base::OneShotTimer insert_timeout_timer_;
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_INSERT_MEDIA_REQUEST_H_
