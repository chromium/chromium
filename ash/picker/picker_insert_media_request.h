// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_INSERT_MEDIA_REQUEST_H_
#define ASH_PICKER_PICKER_INSERT_MEDIA_REQUEST_H_

#include <string>
#include <variant>

#include "ash/ash_export.h"
#include "ash/picker/picker_rich_media.h"
#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/ime/input_method_observer.h"
#include "url/gurl.h"

namespace ui {
class InputMethod;
class TextInputClient;
}  // namespace ui

namespace ash {

// Inserts rich media such as text and images into an input field.
class ASH_EXPORT PickerInsertMediaRequest : public ui::InputMethodObserver {
 public:
  using InsertFailedCallback = base::OnceClosure;

  // Creates a request to insert `data` in the next focused input field.
  // If there's no focus change within `insert_timeout`, then this
  // request is cancelled. If this request is destroyed before insertion could
  // happen, the request is cancelled.
  // If `insert_failed_callback` is valid, it is called if the input field does
  // not support inserting `data`, or no insertion happened before the timeout.
  explicit PickerInsertMediaRequest(
      ui::InputMethod* input_method,
      const PickerRichMedia& media,
      base::TimeDelta insert_timeout,
      InsertFailedCallback insert_failed_callback = {});
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

  std::optional<PickerRichMedia> media_to_insert_;
  base::ScopedObservation<ui::InputMethod, ui::InputMethodObserver>
      observation_{this};
  base::OneShotTimer insert_timeout_timer_;
  InsertFailedCallback insert_failed_callback_;
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_INSERT_MEDIA_REQUEST_H_
