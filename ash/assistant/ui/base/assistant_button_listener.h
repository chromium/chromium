// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_BASE_ASSISTANT_BUTTON_LISTENER_H_
#define ASH_ASSISTANT_UI_BASE_ASSISTANT_BUTTON_LISTENER_H_

namespace ash {

enum class AssistantButtonId;

// Listener to observe presses on |AssistantButton|.
class AssistantButtonListener {
 public:
  AssistantButtonListener() = default;
  virtual ~AssistantButtonListener() = default;

  virtual void OnButtonPressed(AssistantButtonId button_id) = 0;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_BASE_ASSISTANT_BUTTON_LISTENER_H_
