// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/test_support/mock_assistant_state.h"

namespace ash {

MockAssistantState::MockAssistantState() {
  allowed_state_ = chromeos::assistant::AssistantAllowedState::ALLOWED;
  settings_enabled_ = true;
}

MockAssistantState::~MockAssistantState() = default;

void MockAssistantState::SetAllowedState(
    chromeos::assistant::AssistantAllowedState allowed_state) {
  if (allowed_state_ != allowed_state) {
    allowed_state_ = allowed_state;
    for (auto& observer : observers_)
      observer.OnAssistantFeatureAllowedChanged(allowed_state_.value());
  }
}

void MockAssistantState::SetSettingsEnabled(bool enabled) {
  if (settings_enabled_ != enabled) {
    settings_enabled_ = enabled;
    for (auto& observer : observers_)
      observer.OnAssistantSettingsEnabled(settings_enabled_.value());
  }
}

}  // namespace ash
