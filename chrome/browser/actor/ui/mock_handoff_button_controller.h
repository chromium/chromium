// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_MOCK_HANDOFF_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_MOCK_HANDOFF_BUTTON_CONTROLLER_H_

#include "chrome/browser/actor/ui/handoff_button_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace actor::ui {

// A mock class for HandoffButtonController.
class MockHandoffButtonController : public HandoffButtonController {
 public:
  explicit MockHandoffButtonController(tabs::TabInterface& tab_interface);
  ~MockHandoffButtonController() override;

  MOCK_METHOD(void,
              UpdateState,
              (const HandoffButtonState& state, bool is_visible),
              (override));
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_MOCK_HANDOFF_BUTTON_CONTROLLER_H_
