// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_suggestions_controller_impl.h"

#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"

namespace ash {

namespace {

using assistant::prefs::AssistantOnboardingMode;

const AssistantSuggestionsModel* GetModel() {
  return AssistantSuggestionsController::Get()->GetModel();
}

using AssistantSuggestionsControllerImplTest = AssistantAshTestBase;

TEST_F(AssistantSuggestionsControllerImplTest,
       ShouldHaveOnboardingSuggestions) {
  for (int i = 0; i < static_cast<int>(AssistantOnboardingMode::kMaxValue);
       ++i) {
    const auto onboarding_mode = static_cast<AssistantOnboardingMode>(i);
    SetOnboardingMode(onboarding_mode);
    EXPECT_FALSE(GetModel()->GetOnboardingSuggestions().empty());
  }
}

}  // namespace
}  // namespace ash
