// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/hotl/hotl_validator.h"

#include <memory>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/session_journal_impl.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

namespace {

using ::actor::ActorJournalRecorder;
using ::actor::mojom::JournalEntryType;
using ::testing::Contains;
using ::testing::Pair;
using ::testing::SizeIs;

class HotlValidatorTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  actor::AggregatedJournal aggregated_journal_;
  ActorJournalRecorder recorder_{aggregated_journal_};
  SessionJournalImpl journal_{aggregated_journal_, actor::TaskId(1)};
};

TEST_F(HotlValidatorTest, ApproveResolvesApprovedAndJournalsAsyncSlice) {
  base::test::TestFuture<HotlValidator::Result> future;
  {
    HotlValidator validator(journal_, future.GetCallback());
    validator.Answer(/*approved=*/true);
    task_environment_.FastForwardBy(kHotlConfirmationTimeout.Get());
    EXPECT_EQ(future.Get(), HotlValidator::Result::kApproved);
  }

  const std::vector<ActorJournalRecorder::Entry> entries =
      recorder_.GetEntries("TtcHotlConfirmation");
  ASSERT_THAT(entries, SizeIs(2));
  EXPECT_EQ(entries[0].type, JournalEntryType::kBegin);
  EXPECT_THAT(
      entries[0].details,
      Contains(Pair("timeout_ms",
                    base::NumberToString(
                        kHotlConfirmationTimeout.Get().InMilliseconds()))));
  EXPECT_EQ(entries[1].type, JournalEntryType::kEnd);
  EXPECT_THAT(entries[1].details, Contains(Pair("outcome", "approved")));
  EXPECT_THAT(entries[1].details, Contains(Pair("source", "button")));
}

TEST_F(HotlValidatorTest, DenyResolvesDeniedAndJournalsAsyncSlice) {
  base::test::TestFuture<HotlValidator::Result> future;
  HotlValidator validator(journal_, future.GetCallback());

  validator.Answer(/*approved=*/false);

  EXPECT_EQ(future.Get(), HotlValidator::Result::kDenied);
  const std::vector<ActorJournalRecorder::Entry> entries =
      recorder_.GetEntries("TtcHotlConfirmation");
  ASSERT_THAT(entries, SizeIs(2));
  EXPECT_EQ(entries[1].type, JournalEntryType::kEnd);
  EXPECT_THAT(entries[1].details, Contains(Pair("outcome", "denied")));
  EXPECT_THAT(entries[1].details, Contains(Pair("source", "button")));
}

TEST_F(HotlValidatorTest, TimeoutResolvesTimedOutAndJournalsAsyncSlice) {
  base::test::TestFuture<HotlValidator::Result> future;
  HotlValidator validator(journal_, future.GetCallback());

  task_environment_.FastForwardBy(kHotlConfirmationTimeout.Get() -
                                  base::Milliseconds(1));
  EXPECT_FALSE(future.IsReady());
  task_environment_.FastForwardBy(base::Milliseconds(1));

  EXPECT_EQ(future.Get(), HotlValidator::Result::kTimedOut);
  const std::vector<ActorJournalRecorder::Entry> entries =
      recorder_.GetEntries("TtcHotlConfirmation");
  ASSERT_THAT(entries, SizeIs(2));
  EXPECT_EQ(entries[1].type, JournalEntryType::kEnd);
  EXPECT_THAT(entries[1].details, Contains(Pair("outcome", "timeout")));
  EXPECT_THAT(entries[1].details, Contains(Pair("source", "timeout")));
}

TEST_F(HotlValidatorTest, DestroyingWhilePendingDropsCallbackAndLogsCancelled) {
  base::test::TestFuture<HotlValidator::Result> future;
  auto validator =
      std::make_unique<HotlValidator>(journal_, future.GetCallback());

  validator.reset();
  task_environment_.FastForwardBy(kHotlConfirmationTimeout.Get());

  EXPECT_FALSE(future.IsReady());
  const std::vector<ActorJournalRecorder::Entry> entries =
      recorder_.GetEntries("TtcHotlConfirmation");
  ASSERT_THAT(entries, SizeIs(2));
  EXPECT_EQ(entries[0].type, JournalEntryType::kBegin);
  EXPECT_EQ(entries[1].type, JournalEntryType::kEnd);
  EXPECT_THAT(entries[1].details, Contains(Pair("outcome", "cancelled")));
}

TEST_F(HotlValidatorTest, CallbackCanDestroyValidatorOnAnswer) {
  std::unique_ptr<HotlValidator> validator;
  std::optional<HotlValidator::Result> observed;
  validator = std::make_unique<HotlValidator>(
      journal_, base::BindLambdaForTesting([&](HotlValidator::Result result) {
        validator.reset();
        observed = result;
      }));

  validator->Answer(/*approved=*/true);

  EXPECT_EQ(observed, HotlValidator::Result::kApproved);
  EXPECT_FALSE(validator);
}

TEST_F(HotlValidatorTest, CallbackCanDestroyValidatorOnTimeout) {
  std::unique_ptr<HotlValidator> validator;
  base::test::TestFuture<HotlValidator::Result> future;
  validator = std::make_unique<HotlValidator>(
      journal_, base::BindLambdaForTesting([&](HotlValidator::Result result) {
        validator.reset();
        future.SetValue(result);
      }));

  task_environment_.FastForwardBy(kHotlConfirmationTimeout.Get());

  EXPECT_EQ(future.Get(), HotlValidator::Result::kTimedOut);
  EXPECT_FALSE(validator);
}

TEST_F(HotlValidatorTest, CallbackCanReplaceValidator) {
  std::unique_ptr<HotlValidator> validator;
  base::test::TestFuture<HotlValidator::Result> second;
  validator = std::make_unique<HotlValidator>(
      journal_, base::BindLambdaForTesting([&](HotlValidator::Result result) {
        EXPECT_EQ(result, HotlValidator::Result::kDenied);
        validator =
            std::make_unique<HotlValidator>(journal_, second.GetCallback());
      }));

  validator->Answer(/*approved=*/false);
  ASSERT_TRUE(validator);
  validator->Answer(/*approved=*/true);

  EXPECT_EQ(second.Get(), HotlValidator::Result::kApproved);
}

TEST_F(HotlValidatorTest, AnswerAfterResolveCheckFails) {
  base::test::TestFuture<HotlValidator::Result> future;
  HotlValidator validator(journal_, future.GetCallback());
  validator.Answer(/*approved=*/true);

  EXPECT_CHECK_DEATH(validator.Answer(/*approved=*/false));
}

}  // namespace

}  // namespace ttc
