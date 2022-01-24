// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_QUICK_ANSWERS_TEST_SUPPORT_QUICK_ANSWERS_TEST_BASE_H_
#define ASH_PUBLIC_CPP_QUICK_ANSWERS_TEST_SUPPORT_QUICK_ANSWERS_TEST_BASE_H_

#include <memory>

#include "ash/public/cpp/quick_answers/quick_answers_state.h"
#include "chromeos/services/assistant/test_support/fully_initialized_assistant_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// Helper class for Quick Answers related tests.
class QuickAnswersTestBase : public testing::Test {
 public:
  QuickAnswersTestBase();

  QuickAnswersTestBase(const QuickAnswersTestBase&) = delete;
  QuickAnswersTestBase& operator=(const QuickAnswersTestBase&) = delete;

  ~QuickAnswersTestBase() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 private:
  std::unique_ptr<chromeos::assistant::FullyInitializedAssistantState>
      assistant_state_;
  std::unique_ptr<QuickAnswersState> quick_answers_state_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_QUICK_ANSWERS_TEST_SUPPORT_QUICK_ANSWERS_TEST_BASE_H_
