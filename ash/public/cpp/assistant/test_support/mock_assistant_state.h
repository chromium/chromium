// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_STATE_H_
#define ASH_PUBLIC_CPP_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_STATE_H_

#include "ash/public/cpp/assistant/assistant_state.h"

namespace ash {

class MockAssistantState : public ash::AssistantState {
 public:
  MockAssistantState();
  MockAssistantState(const MockAssistantState&) = delete;
  MockAssistantState& operator=(const MockAssistantState&) = delete;
  ~MockAssistantState() override;

  void SetAllowedState(assistant::AssistantAllowedState allowed_state);

  void SetSettingsEnabled(bool enabled);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_STATE_H_
