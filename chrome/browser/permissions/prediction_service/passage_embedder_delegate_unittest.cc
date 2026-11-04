// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_service/passage_embedder_delegate.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/permissions/test/mock_passage_embedder.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace permissions {

namespace {
using ::test::PassageEmbedderMock;

class TestPassageEmbedderDelegate : public PassageEmbedderDelegate {
 public:
  using PassageEmbedderDelegate::PassageEmbedderDelegate;

  void SetEmbedder(passage_embeddings::Embedder* embedder) {
    embedder_ = embedder;
  }

 protected:
  passage_embeddings::Embedder* GetPassageEmbedder() override {
    return embedder_;
  }

 private:
  raw_ptr<passage_embeddings::Embedder> embedder_ = nullptr;
};

// Keeps the callback of every job until the test runs it, unlike
// `test::DelayedPassageEmbedderMock`, which keeps only the last one.
class CallbackKeepingPassageEmbedder : public PassageEmbedderMock {
 public:
  // PassageEmbedderMock:
  passage_embeddings::Embedder::Job ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority priority,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) override {
    const uint64_t job_id = next_job_id_++;
    callbacks_[job_id] = std::move(callback);
    return passage_embeddings::Embedder::Job(GetWeakPtr(), job_id);
  }

  uint64_t last_job_id() const { return next_job_id_ - 1; }

  // Runs the callback of the job with `job_id` with `status`, and with one
  // embedding if `status` is kSuccess.
  void RunCallback(uint64_t job_id,
                   passage_embeddings::ComputeEmbeddingsStatus status) {
    auto node = callbacks_.extract(job_id);
    ASSERT_FALSE(node.empty());
    std::vector<passage_embeddings::Embedding> embeddings;
    if (status == passage_embeddings::ComputeEmbeddingsStatus::kSuccess) {
      // The Embedding constructor requires unit length.
      embeddings.emplace_back(std::vector<float>{1.0f});
    }
    std::move(node.mapped())
        .Run({"text"}, std::move(embeddings), job_id, status);
  }

 private:
  std::map<uint64_t, ComputePassagesEmbeddingsCallback> callbacks_;
};

struct SplittingTestCase {
  std::string test_name;
  size_t text_length;
  int requested_passage_count;
  std::vector<size_t> expected_passage_lengths;
};

}  // namespace

class PassageEmbedderDelegateTest : public ChromeRenderViewHostTestHarness {
 public:
  PassageEmbedderDelegateTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // Initialize delegate
    delegate_ = std::make_unique<TestPassageEmbedderDelegate>(profile());
    delegate_->SetEmbedder(&passage_embedder_);
  }

  void TearDown() override {
    delegate_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Helper to execute and wait for callback
  std::optional<std::vector<float>> RunEmbedder(std::string text,
                                                int passage_count) {
    std::optional<std::vector<float>> result;
    base::RunLoop run_loop;

    delegate_->CreatePassageEmbeddingsFromRenderedText(
        text, passage_count,
        base::BindLambdaForTesting([&](std::vector<float> embedding) {
          result = std::move(embedding);
          run_loop.Quit();
        }),
        base::BindLambdaForTesting([&]() { run_loop.Quit(); }));

    run_loop.Run();
    return result;
  }

 protected:
  PassageEmbedderMock passage_embedder_;
  std::unique_ptr<TestPassageEmbedderDelegate> delegate_;
};

class PassageEmbedderDelegateSplittingTest
    : public PassageEmbedderDelegateTest,
      public testing::WithParamInterface<SplittingTestCase> {};

// Uses mock time, and an embedder that keeps every callback so that tests
// decide when and how each job reports back.
class PassageEmbedderDelegateStaleJobTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PassageEmbedderDelegateStaleJobTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    delegate_ = std::make_unique<TestPassageEmbedderDelegate>(profile());
    delegate_->SetEmbedder(&passage_embedder_);
  }

  void TearDown() override {
    delegate_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  CallbackKeepingPassageEmbedder passage_embedder_;
  std::unique_ptr<TestPassageEmbedderDelegate> delegate_;
};

TEST_P(PassageEmbedderDelegateSplittingTest, SplitsTextIntoCorrectPassages) {
  const auto& test_case = GetParam();
  // Mock success status so the mock stores passages.
  passage_embedder_.set_status(
      passage_embeddings::ComputeEmbeddingsStatus::kSuccess);

  // Generate text of the requested length.
  // We use different characters to verify content integrity if needed,
  // but for length checks repetition is fine.
  std::string text(test_case.text_length, 'a');

  RunEmbedder(text, test_case.requested_passage_count);
  std::vector<std::string> passages = passage_embedder_.GetLastPassages();

  ASSERT_EQ(passages.size(), test_case.expected_passage_lengths.size());

  size_t current_index = 0;
  for (size_t i = 0; i < passages.size(); ++i) {
    EXPECT_EQ(passages[i].size(), test_case.expected_passage_lengths[i]);
    EXPECT_EQ(passages[i], text.substr(current_index,
                                       test_case.expected_passage_lengths[i]));
    current_index += test_case.expected_passage_lengths[i];
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PassageEmbedderDelegateSplittingTest,
    testing::ValuesIn<SplittingTestCase>({
        {
            /*test_name=*/"ShortTextCount1",
            /*text_length=*/5,
            /*requested_passage_count=*/1,
            /*expected_passage_lengths=*/{5},
        },
        {
            /*test_name=*/"ShortTextCount2",
            /*text_length=*/5,
            /*requested_passage_count=*/2,
            /*expected_passage_lengths=*/{5},
        },
        {
            /*test_name=*/"ExactMaxLen",
            /*text_length=*/500,
            /*requested_passage_count=*/1,
            /*expected_passage_lengths=*/{500},
        },
        {
            /*test_name=*/"LongTextCount2",
            /*text_length=*/1200,
            /*requested_passage_count=*/2,
            /*expected_passage_lengths=*/{500, 500},
        },
        {
            /*test_name=*/"LongTextCount3",
            /*text_length=*/1200,
            /*requested_passage_count=*/3,
            /*expected_passage_lengths=*/{500, 500, 200},
        },
    }),
    [](const testing::TestParamInfo<
        PassageEmbedderDelegateSplittingTest::ParamType>& info) {
      return info.param.test_name;
    });

TEST_F(PassageEmbedderDelegateTest, AveragesEmbeddings) {
  passage_embedder_.set_status(
      passage_embeddings::ComputeEmbeddingsStatus::kSuccess);

  std::string p1(500, 'a');
  std::string p2(500, 'b');
  std::string combined = p1 + p2;

  // Run separately to get individual embeddings
  // Note: TestEmbedder uses hashing of passage content.
  auto e1 = RunEmbedder(p1, /*passage_count=*/1);
  auto e2 = RunEmbedder(p2, /*passage_count=*/1);
  ASSERT_TRUE(e1.has_value());
  ASSERT_TRUE(e2.has_value());

  // Run combined with count 2
  auto e_avg = RunEmbedder(combined, /*passage_count=*/2);
  ASSERT_TRUE(e_avg.has_value());

  const std::vector<float>& v1 = *e1;
  const std::vector<float>& v2 = *e2;
  const std::vector<float>& v_avg = *e_avg;

  ASSERT_EQ(v1.size(), v_avg.size());
  ASSERT_EQ(v2.size(), v_avg.size());

  for (size_t i = 0; i < v1.size(); ++i) {
    float expected = (v1[i] + v2[i]) / 2.0f;
    EXPECT_NEAR(v_avg[i], expected, 1e-5f) << "Mismatch at index " << i;
  }
}

TEST_F(PassageEmbedderDelegateTest, ReturnsFallbackWhenEmbedderFails) {
  passage_embedder_.set_status(
      passage_embeddings::ComputeEmbeddingsStatus::kExecutionFailure);
  bool fallback_called = false;

  delegate_->CreatePassageEmbeddingsFromRenderedText(
      "text", /*passage_count=*/1,
      base::BindLambdaForTesting([&](std::vector<float> embedding) {
        FAIL() << "Should not be called";
      }),
      base::BindLambdaForTesting([&]() { fallback_called = true; }));

  // The mock reports the failure before ComputePassagesEmbeddings() returns,
  // so before the delegate stores the job.
  EXPECT_TRUE(fallback_called);
}

TEST_F(PassageEmbedderDelegateTest, ReturnsFallbackWhenEmbedderNotReady) {
  delegate_->SetEmbedder(nullptr);
  bool fallback_called = false;
  base::RunLoop run_loop;

  delegate_->CreatePassageEmbeddingsFromRenderedText(
      "text", /*passage_count=*/1,
      base::BindLambdaForTesting([&](std::vector<float> embedding) {
        FAIL() << "Should not be called";
      }),
      base::BindLambdaForTesting([&]() {
        fallback_called = true;
        run_loop.Quit();
      }));

  run_loop.Run();
  EXPECT_TRUE(fallback_called);
}

// A job dropped by Reset() can still report back, e.g. as canceled once the
// work it had started returns. Reset() dropped the callbacks, so the result
// must not run a null fallback.
TEST_F(PassageEmbedderDelegateStaleJobTest,
       CanceledResultAfterResetDoesNotCrash) {
  delegate_->CreatePassageEmbeddingsFromRenderedText(
      "text", /*passage_count=*/1, base::DoNothing(), base::DoNothing());
  delegate_->Reset();

  passage_embedder_.RunCallback(
      passage_embedder_.last_job_id(),
      passage_embeddings::ComputeEmbeddingsStatus::kCanceled);
}

// A job replaced by a newer request can still report back, e.g. as canceled.
// That result must leave the newer request's fallback, timeout and timeout
// metric alone.
TEST_F(PassageEmbedderDelegateStaleJobTest,
       CanceledResultOfReplacedJobIsIgnored) {
  base::HistogramTester histogram_tester;
  delegate_->CreatePassageEmbeddingsFromRenderedText(
      "text 1", /*passage_count=*/1, base::DoNothing(), base::DoNothing());
  const uint64_t first_job_id = passage_embedder_.last_job_id();

  int fallback_count = 0;
  delegate_->CreatePassageEmbeddingsFromRenderedText(
      "text 2", /*passage_count=*/1,
      base::BindLambdaForTesting([](std::vector<float> embedding) {
        FAIL() << "Should not be called";
      }),
      base::BindLambdaForTesting([&]() { ++fallback_count; }));

  passage_embedder_.RunCallback(
      first_job_id, passage_embeddings::ComputeEmbeddingsStatus::kCanceled);
  EXPECT_EQ(fallback_count, 0);
  histogram_tester.ExpectTotalCount(
      "Permissions.AIv4.PassageEmbeddingsComputationTimeout", 0);

  // The newer request still times out.
  task_environment()->FastForwardBy(
      base::Seconds(PassageEmbedderDelegate::kPassageEmbedderDelegateTimeout));
  EXPECT_EQ(fallback_count, 1);
  histogram_tester.ExpectUniqueSample(
      "Permissions.AIv4.PassageEmbeddingsComputationTimeout", true, 1);
}

// A replaced job can also report back after the newer request succeeded. That
// result must not run the newer request's fallback.
TEST_F(PassageEmbedderDelegateStaleJobTest,
       CanceledResultOfReplacedJobAfterNewerSuccessIsIgnored) {
  delegate_->CreatePassageEmbeddingsFromRenderedText(
      "text 1", /*passage_count=*/1, base::DoNothing(), base::DoNothing());
  const uint64_t first_job_id = passage_embedder_.last_job_id();

  int success_count = 0;
  int fallback_count = 0;
  delegate_->CreatePassageEmbeddingsFromRenderedText(
      "text 2", /*passage_count=*/1,
      base::BindLambdaForTesting(
          [&](std::vector<float> embedding) { ++success_count; }),
      base::BindLambdaForTesting([&]() { ++fallback_count; }));
  const uint64_t second_job_id = passage_embedder_.last_job_id();

  passage_embedder_.RunCallback(
      second_job_id, passage_embeddings::ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(success_count, 1);

  passage_embedder_.RunCallback(
      first_job_id, passage_embeddings::ComputeEmbeddingsStatus::kCanceled);
  EXPECT_EQ(fallback_count, 0);
}

// A job can fail after it timed out. The timeout already ran the fallback, so
// the failure must not run it again.
TEST_F(PassageEmbedderDelegateStaleJobTest, FailedResultAfterTimeoutIsIgnored) {
  int fallback_count = 0;
  delegate_->CreatePassageEmbeddingsFromRenderedText(
      "text", /*passage_count=*/1,
      base::BindLambdaForTesting([](std::vector<float> embedding) {
        FAIL() << "Should not be called";
      }),
      base::BindLambdaForTesting([&]() { ++fallback_count; }));
  const uint64_t job_id = passage_embedder_.last_job_id();

  task_environment()->FastForwardBy(
      base::Seconds(PassageEmbedderDelegate::kPassageEmbedderDelegateTimeout));
  EXPECT_EQ(fallback_count, 1);

  passage_embedder_.RunCallback(
      job_id, passage_embeddings::ComputeEmbeddingsStatus::kExecutionFailure);
  EXPECT_EQ(fallback_count, 1);
}

// A job can succeed after it timed out. The timeout already ran the fallback,
// so the result must not run the success callback as well.
TEST_F(PassageEmbedderDelegateStaleJobTest,
       SuccessfulResultAfterTimeoutIsIgnored) {
  int fallback_count = 0;
  delegate_->CreatePassageEmbeddingsFromRenderedText(
      "text", /*passage_count=*/1,
      base::BindLambdaForTesting([](std::vector<float> embedding) {
        FAIL() << "Should not be called";
      }),
      base::BindLambdaForTesting([&]() { ++fallback_count; }));
  const uint64_t job_id = passage_embedder_.last_job_id();

  task_environment()->FastForwardBy(
      base::Seconds(PassageEmbedderDelegate::kPassageEmbedderDelegateTimeout));
  EXPECT_EQ(fallback_count, 1);

  passage_embedder_.RunCallback(
      job_id, passage_embeddings::ComputeEmbeddingsStatus::kSuccess);
  EXPECT_EQ(fallback_count, 1);
}

}  // namespace permissions
