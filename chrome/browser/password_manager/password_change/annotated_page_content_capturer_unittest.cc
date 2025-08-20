// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/annotated_page_content_capturer.h"

#include "base/functional/callback.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/pass_key.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Invoke;

class AnnotatedPageContentCapturerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  using MockGetAIPageContentFunction = base::MockCallback<
      AnnotatedPageContentCapturer::GetAIPageContentFunction>;

  std::unique_ptr<AnnotatedPageContentCapturer> CreateCapturer(
      optimization_guide::OnAIPageContentDone callback) {
    return std::make_unique<AnnotatedPageContentCapturer>(
        base::PassKey<class AnnotatedPageContentCapturerTest>(), web_contents(),
        blink::mojom::AIPageContentOptions::New(), std::move(callback),
        mock_get_page_content_.Get());
  }

 protected:
  MockGetAIPageContentFunction mock_get_page_content_;
};

TEST_F(AnnotatedPageContentCapturerTest, CaptureSucceedsOnFirstLoad) {
  base::test::TestFuture<std::optional<optimization_guide::AIPageContentResult>>
      completion_future;
  auto capturer = CreateCapturer(completion_future.GetCallback());

  EXPECT_CALL(mock_get_page_content_, Run)
      .WillOnce(RunOnceCallback<1>(optimization_guide::AIPageContentResult()));
  capturer->DidStopLoading();
  EXPECT_TRUE(completion_future.Get().has_value());
}

TEST_F(AnnotatedPageContentCapturerTest, NewLoadInvalidatesPreviousRequest) {
  base::test::TestFuture<std::optional<optimization_guide::AIPageContentResult>>
      completion_future;
  auto capturer = CreateCapturer(completion_future.GetCallback());

  optimization_guide::OnAIPageContentDone first_request_callback;
  optimization_guide::OnAIPageContentDone second_request_callback;

  EXPECT_CALL(mock_get_page_content_, Run)
      .WillOnce(
          Invoke([&](auto, optimization_guide::OnAIPageContentDone callback) {
            first_request_callback = std::move(callback);
          }))
      .WillOnce(
          Invoke([&](auto, optimization_guide::OnAIPageContentDone callback) {
            second_request_callback = std::move(callback);
          }));

  capturer->DidStopLoading();
  ASSERT_TRUE(first_request_callback);

  capturer->DidStopLoading();
  ASSERT_TRUE(second_request_callback);

  // The second `DidStopLoading` should invalidate the
  // first callback from being executed.
  std::move(first_request_callback)
      .Run(optimization_guide::AIPageContentResult());
  EXPECT_FALSE(completion_future.IsReady());

  std::move(second_request_callback)
      .Run(optimization_guide::AIPageContentResult());
  EXPECT_TRUE(completion_future.Get().has_value());
}
