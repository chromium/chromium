// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/hats/hats_dialog.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
constexpr char kHistogramName[] = "Some.Kind.Of.Histogram";
}  // namespace

class HatsDialogTest : public testing::Test {
 public:
  void TriggerAction(std::string action) {
    EXPECT_FALSE(
        HatsDialog::HandleClientTriggeredAction(action, kHistogramName));
  }

  void TriggerActionAndClose(std::string action) {
    EXPECT_TRUE(
        HatsDialog::HandleClientTriggeredAction(action, kHistogramName));
  }

  std::vector<base::Bucket> GetHistogramSamples() {
    return histogram_tester_.GetAllSamples(kHistogramName);
  }

 private:
  base::HistogramTester histogram_tester_;
};

TEST_F(HatsDialogTest, HandleClientTriggeredAction_Unknown) {
  // Client sent an invalid action, ignore it
  TriggerAction("Invalid");

  EXPECT_THAT(GetHistogramSamples(), testing::IsEmpty());
}

TEST_F(HatsDialogTest, HandleClientTriggeredAction_Loaded) {
  // Client asks to close the window
  TriggerAction("load");

  std::vector<base::Bucket> expected = {
      {2, 1}};  // 2 is the enumeration for "Displayed".
  EXPECT_EQ(GetHistogramSamples(), expected);
}

TEST_F(HatsDialogTest, HandleClientTriggeredAction_Close) {
  // Client asks to close the window
  TriggerActionAndClose("close");

  EXPECT_THAT(GetHistogramSamples(), testing::IsEmpty());
}

TEST_F(HatsDialogTest, HandleClientTriggeredAction_Complete) {
  // Client asks to close the window
  TriggerActionAndClose("complete");

  std::vector<base::Bucket> expected = {
      {3, 1}};  // 3 is the enumeration for "Complete".
  EXPECT_EQ(GetHistogramSamples(), expected);
}

TEST_F(HatsDialogTest, HandleClientTriggeredAction_Error) {
  // There was an unhandled error, close the window
  TriggerActionAndClose("survey-loading-error-12345");

  EXPECT_THAT(GetHistogramSamples(), testing::IsEmpty());
}

TEST_F(HatsDialogTest, HandleClientTriggeredAction_OldQuestionResponse) {
  TriggerAction("smiley-selected-2");
  TriggerAction("smiley-selected-2");
  TriggerAction("smiley-selected-4");

  EXPECT_THAT(GetHistogramSamples(), testing::IsEmpty());
}

TEST_F(HatsDialogTest, HandleClientTriggeredAction_InvalidQuestion) {
  TriggerAction("answer-a-2");

  EXPECT_THAT(GetHistogramSamples(), testing::IsEmpty());
}

TEST_F(HatsDialogTest, HandleClientTriggeredAction_FirstQuestion) {
  TriggerAction("answer-1-2");

  std::vector<base::Bucket> expected = {{102, 1}};
  EXPECT_EQ(GetHistogramSamples(), expected);
}

TEST_F(HatsDialogTest, HandleClientTriggeredAction_SingleSelectQuestion) {
  TriggerAction("answer-2-4");

  std::vector<base::Bucket> expected = {{204, 1}};
  EXPECT_EQ(GetHistogramSamples(), expected);
}

TEST_F(HatsDialogTest, HandleClientTriggeredAction_MultipleSelectQuestion) {
  TriggerAction("answer-3-2,4,5");

  std::vector<base::Bucket> expected = {{302, 1}, {304, 1}, {305, 1}};
  EXPECT_EQ(GetHistogramSamples(), expected);
}

TEST_F(HatsDialogTest, HandleClientTriggeredAction_FullWorkflow) {
  TriggerAction("load");
  TriggerAction("answer-1-2");
  TriggerAction("answer-2-3");
  TriggerAction("answer-3-4,5");
  TriggerActionAndClose("complete");

  std::vector<base::Bucket> expected = {{2, 1},   {3, 1},   {102, 1},
                                        {203, 1}, {304, 1}, {305, 1}};
  EXPECT_EQ(GetHistogramSamples(), expected);
}

TEST_F(HatsDialogTest, ParseAnswer) {
  int question;
  std::vector<int> scores;

  // Incomplete answers
  EXPECT_FALSE(HatsDialog::ParseAnswer("answer-", &question, &scores));
  EXPECT_FALSE(HatsDialog::ParseAnswer("answer-1", &question, &scores));
  EXPECT_FALSE(HatsDialog::ParseAnswer("answer-1-", &question, &scores));

  // Invalid integers.
  EXPECT_FALSE(HatsDialog::ParseAnswer("answer-a-1,2,3", &question, &scores));
  EXPECT_FALSE(HatsDialog::ParseAnswer("answer-1-a", &question, &scores));

  // Out of range
  EXPECT_FALSE(HatsDialog::ParseAnswer("answer--1-1,2,3", &question, &scores));
  EXPECT_FALSE(HatsDialog::ParseAnswer("answer-1--1", &question, &scores));
  EXPECT_FALSE(HatsDialog::ParseAnswer("answer-0-1,2,3", &question, &scores));
  EXPECT_FALSE(HatsDialog::ParseAnswer("answer-11-1", &question, &scores));
  EXPECT_FALSE(HatsDialog::ParseAnswer("answer-1-101", &question, &scores));

  // Overflow int.
  EXPECT_FALSE(
      HatsDialog::ParseAnswer("answer-2147483648-a", &question, &scores));
  EXPECT_FALSE(
      HatsDialog::ParseAnswer("answer-1-2147483648", &question, &scores));

  EXPECT_TRUE(HatsDialog::ParseAnswer("answer-1-10", &question, &scores));
  EXPECT_EQ(question, 1);
  EXPECT_EQ(scores.size(), 1UL);
  EXPECT_EQ(scores[0], 10);

  scores.clear();
  EXPECT_TRUE(HatsDialog::ParseAnswer("answer-2-1,2", &question, &scores));
  EXPECT_EQ(question, 2);
  EXPECT_EQ(scores.size(), 2UL);
  EXPECT_EQ(scores[0], 1);
  EXPECT_EQ(scores[1], 2);
}

}  // namespace ash
