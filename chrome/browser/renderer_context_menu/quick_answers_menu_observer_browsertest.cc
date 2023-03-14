// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/quick_answers_browsertest_base.h"

#include "chromeos/components/quick_answers/public/cpp/controller/quick_answers_controller.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "content/public/test/browser_test.h"

namespace {

using QuickAnswersMenuObserverTest = quick_answers::QuickAnswersBrowserTestBase;

}  // namespace

IN_PROC_BROWSER_TEST_F(QuickAnswersMenuObserverTest, FeatureIneligible) {
  QuickAnswersState::Get()->set_eligibility_for_testing(false);

  ShowMenuParams params;
  params.selected_text = "test";

  ShowMenu(params);

  // Quick Answers UI should stay hidden since the feature is not eligible.
  ASSERT_EQ(QuickAnswersVisibility::kClosed,
            controller()->GetVisibilityForTesting());
}

IN_PROC_BROWSER_TEST_F(QuickAnswersMenuObserverTest, PasswordField) {
  QuickAnswersState::Get()->set_eligibility_for_testing(true);

  ShowMenuParams params;
  params.selected_text = "test";
  params.is_password_field = true;

  ShowMenu(params);

  // Quick Answers UI should stay hidden since the input field is password
  // field.
  ASSERT_EQ(QuickAnswersVisibility::kClosed,
            controller()->GetVisibilityForTesting());
}

IN_PROC_BROWSER_TEST_F(QuickAnswersMenuObserverTest, NoSelectedText) {
  QuickAnswersState::Get()->set_eligibility_for_testing(true);

  ShowMenu(ShowMenuParams());

  // Quick Answers UI should stay hidden since no text is selected.
  ASSERT_EQ(QuickAnswersVisibility::kClosed,
            controller()->GetVisibilityForTesting());
}

IN_PROC_BROWSER_TEST_F(QuickAnswersMenuObserverTest, QuickAnswersPending) {
  QuickAnswersState::Get()->set_eligibility_for_testing(true);

  ShowMenuParams params;
  params.selected_text = "test";
  ShowMenu(params);

  // Quick Answers UI should be pending.
  ASSERT_EQ(QuickAnswersVisibility::kPending,
            controller()->GetVisibilityForTesting());
}
