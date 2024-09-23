// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_INSERTION_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_INSERTION_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ash {
namespace input_method {

class EditorTextInsertion {
 public:
  enum class InsertionStrategy {
    kInsertAsMultipleParagraphs,
    kInsertAsASingleParagraph,
  };

  explicit EditorTextInsertion(const std::string& text,
                               InsertionStrategy strategy);
  ~EditorTextInsertion();

  // To help ensure that we do not insert the pending text in text fields other
  // then the target text input, we set a timeout for the text insertion
  // operation to occur within otherwise it is cancelled. This method returns
  // the state of that timeout.
  bool HasTimedOut();

  // Attempts to commit the pending text into the currently focused text input.
  bool Commit();

  // Returns the length of the text to be inserted with this operation.
  int GetTextLength();

 private:
  enum State {
    kPending,
    kTimedOut,
  };

  // This method is invoked after the text insertion timeout has elapsed, and
  // it renders this operation inert. Consumers of this class can no longer
  // commit the pending text once this method has been called.
  void CancelTextInsertion();

  const std::string pending_text_;
  State state_;
  const InsertionStrategy strategy_;
  base::OneShotTimer text_insertion_timeout_;
  base::WeakPtrFactory<EditorTextInsertion> weak_ptr_factory_{this};
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_INSERTION_H_
