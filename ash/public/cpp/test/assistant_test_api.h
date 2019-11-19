// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_ASSISTANT_TEST_API_H_
#define ASH_PUBLIC_CPP_TEST_ASSISTANT_TEST_API_H_

#include <memory>

#include "ash/ash_export.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Textfield;
class View;
}  // namespace views

namespace ash {

// Public test API for the Assistant UI.
// This API works both for ash and browser tests.
class ASH_EXPORT AssistantTestApi {
 public:
  static std::unique_ptr<AssistantTestApi> Create();

  AssistantTestApi() = default;
  virtual ~AssistantTestApi() = default;

  virtual void DisableAnimations() = 0;

  // Returns true if the Assistant UI is visible.
  virtual bool IsVisible() = 0;

  // Sends a text query. This requires the UI to be ready to accept text
  // queries, i.e. the input text field must be visible and focussed.
  virtual void SendTextQuery(const std::string& query) = 0;

  // Enables Assistant in settings.
  virtual void EnableAssistant() = 0;

  virtual void SetTabletMode(bool enable) = 0;

  // Changes the user setting controlling whether the user prefers voice or
  // keyboard.
  virtual void SetPreferVoice(bool value) = 0;

  // Returns the top-level Assistant specific view.
  // Can only be used after the Assistant UI has been shown at least once.
  virtual views::View* page_view() = 0;

  // Returns the Assistant main view.
  // Can only be used after the Assistant UI has been shown at least once.
  virtual views::View* main_view() = 0;

  // Returns the text field used for inputting new queries.
  // Can only be used after the Assistant UI has been shown at least once.
  virtual views::Textfield* input_text_field() = 0;

  // Returns the mic field used for dictating new queries.
  // Can only be used after the Assistant UI has been shown at least once.
  virtual views::View* mic_view() = 0;

  // Returns the greeting label shown when the Assistant is displayed.
  // Can only be used after the Assistant UI has been shown at least once.
  virtual views::View* greeting_label() = 0;

  // Returns the window containing the Assistant UI.
  // Note that this window is shared for all components of the |AppList|.
  virtual aura::Window* window() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_ASSISTANT_TEST_API_H_
