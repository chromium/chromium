// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/quick_answers/test_support/quick_answers_test_base.h"

#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/quick_answers/quick_answers_state.h"

namespace ash {

QuickAnswersTestBase::QuickAnswersTestBase() = default;

QuickAnswersTestBase::~QuickAnswersTestBase() = default;

void QuickAnswersTestBase::SetUp() {
  testing::Test::SetUp();

  if (!AssistantState::Get()) {
    assistant_state_ =
        std::make_unique<chromeos::assistant::FullyInitializedAssistantState>();
  }

  if (!QuickAnswersState::Get())
    quick_answers_state_ = std::make_unique<QuickAnswersState>();
}

void QuickAnswersTestBase::TearDown() {
  quick_answers_state_.reset();
  assistant_state_.reset();

  testing::Test::TearDown();
}

}  // namespace ash
