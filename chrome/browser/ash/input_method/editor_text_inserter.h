// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_INSERTER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_INSERTER_H_

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ash {
namespace input_method {

class EditorTextInserter {
 public:
  EditorTextInserter();
  ~EditorTextInserter();

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

  void CancelTextInsertion();

  // Holds any pending text insertions. It is assumed that only one text
  // insertion will be requested at any given time.
  std::optional<PendingTextInsert> pending_text_insert_;

  // Holds the context of a focused text client.
  std::optional<TextClientContext> focused_client_;

  base::OneShotTimer text_insertion_timer_;

  base::WeakPtrFactory<EditorTextInserter> weak_ptr_factory_{this};
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_INSERTER_H_
