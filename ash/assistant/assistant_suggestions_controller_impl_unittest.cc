// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_suggestions_controller_impl.h"

#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/cpp/features.h"

namespace ash {

namespace {

using chromeos::assistant::prefs::AssistantOnboardingMode;

// AssistantSuggestionsControllerImplTest --------------------------------------

class AssistantSuggestionsControllerImplTest
    : public AssistantAshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  AssistantSuggestionsControllerImplTest() {
    feature_list_.InitWithFeatureState(
        chromeos::assistant::features::kAssistantBetterOnboarding, GetParam());
  }

  AssistantSuggestionsControllerImpl* controller() {
    return static_cast<AssistantSuggestionsControllerImpl*>(
        AssistantSuggestionsController::Get());
  }

  const AssistantSuggestionsModel* model() { return controller()->GetModel(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

// Tests -----------------------------------------------------------------------

TEST_P(AssistantSuggestionsControllerImplTest,
       ShouldMaybeHaveOnboardingSuggestions) {
  for (int i = 0; i < static_cast<int>(AssistantOnboardingMode::kMaxValue);
       ++i) {
    const auto onboarding_mode = static_cast<AssistantOnboardingMode>(i);
    SetOnboardingMode(onboarding_mode);
    EXPECT_NE(GetParam(), model()->GetOnboardingSuggestions().empty());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         AssistantSuggestionsControllerImplTest,
                         testing::Bool());

}  // namespace ash
