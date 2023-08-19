// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_ACTUATOR_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_ACTUATOR_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace input_method {

class EditorTextActuator {
 public:
  EditorTextActuator();
  ~EditorTextActuator();

  // Enqueues some text to be inserted in the next text client to be focused.
  void InsertTextOnNextFocus(std::string_view text);

  // Text input event handlers.
  void OnFocus(int context_id);
  void OnBlur();

 private:
  // Holds the details of the currently focused text input's context.
  struct TextClientContext {
    int id;
  };

  // Represents a pending text insertion command.
  struct PendingTextInsert {
    std::string text;
  };

  // Holds any pending text insertions. It is assumed that only one text
  // insertion will be requested at any given time.
  absl::optional<PendingTextInsert> pending_text_insert_;

  // Holds the context of a focused text client.
  absl::optional<TextClientContext> focused_client_;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_ACTUATOR_H_
