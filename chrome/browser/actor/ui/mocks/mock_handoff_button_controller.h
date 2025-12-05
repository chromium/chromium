// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_MOCKS_MOCK_HANDOFF_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_MOCKS_MOCK_HANDOFF_BUTTON_CONTROLLER_H_

#include "chrome/browser/actor/ui/handoff_button_controller.h"
#include "chrome/browser/actor/ui/states/handoff_button_state.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace actor::ui {

class MockHandoffButtonController : public HandoffButtonController {
 public:
  explicit MockHandoffButtonController(
      views::View* anchor_view,
      ActorUiWindowController* window_controller);
  ~MockHandoffButtonController() override;

  MOCK_METHOD(void,
              UpdateState,
              (HandoffButtonState state, bool is_visible, base::OnceClosure),
              (override));
  MOCK_METHOD(bool, IsHovering, (), (override));
  MOCK_METHOD(bool, IsFocused, (), (override));
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_MOCKS_MOCK_HANDOFF_BUTTON_CONTROLLER_H_
