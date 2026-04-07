// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/pass_key.h"
#include "chrome/browser/password_manager/password_change/annotated_page_content_capturer_impl.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

using ::base::test::RunOnceCallback;
using ::testing::_;

class AnnotatedPageContentCapturerTest
    : public ChromeRenderViewHostTestHarness,
      public ::testing::WithParamInterface<bool> {
 public:
  AnnotatedPageContentCapturerTest() {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureStates(
        {{password_manager::features::kAwaitPageStabilityForPasswordChange,
          GetParam()}});
  }

  using MockGetAIPageContentFunction = base::MockCallback<
      AnnotatedPageContentCapturer::GetAIPageContentFunction>;

  std::unique_ptr<AnnotatedPageContentCapturerImpl> CreateCapturer(
      optimization_guide::OnAIPageContentDone callback) {
    return std::make_unique<AnnotatedPageContentCapturerImpl>(
        web_contents(), &stub_client_,
        blink::mojom::AIPageContentOptions::New(), std::move(callback),
        mock_get_page_content_.Get());
  }

 protected:
  MockGetAIPageContentFunction mock_get_page_content_;
  base::test::ScopedFeatureList feature_list_;
  password_manager::StubPasswordManagerClient stub_client_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AnnotatedPageContentCapturerTest,
                         testing::Bool());

TEST_P(AnnotatedPageContentCapturerTest, CaptureEmptyPageContent) {
  base::test::TestFuture<optimization_guide::AIPageContentResultOrError>
      completion_future;
  std::unique_ptr<AnnotatedPageContentCapturerImpl> capturer =
      CreateCapturer(completion_future.GetCallback());
  optimization_guide::AIPageContentResult result;
  const char kEmptyPageContentData[] = "\n\002B\000\020\002";
  result.proto.ParseFromArray(kEmptyPageContentData,
                              sizeof(kEmptyPageContentData) - 1);
  EXPECT_CALL(mock_get_page_content_, Run)
      .WillOnce(RunOnceCallback<1>(std::move(result)));
  content::WebContentsTester::For(web_contents())->TestSetIsLoading(false);
  capturer->OnPageStable();
  EXPECT_TRUE(completion_future.IsReady());
}

TEST_P(AnnotatedPageContentCapturerTest, CaptureSucceedsOnFirstLoad) {
  base::test::TestFuture<optimization_guide::AIPageContentResultOrError>
      completion_future;
  std::unique_ptr<AnnotatedPageContentCapturerImpl> capturer =
      CreateCapturer(completion_future.GetCallback());
  optimization_guide::AIPageContentResult page_content_result;
  page_content_result.proto.mutable_root_node()
      ->mutable_content_attributes()
      ->set_common_ancestor_dom_node_id(3);
  EXPECT_CALL(mock_get_page_content_, Run)
      .WillOnce(RunOnceCallback<1>(std::move(page_content_result)));
  content::WebContentsTester::For(web_contents())->TestSetIsLoading(false);
  capturer->OnPageStable();
  EXPECT_TRUE(completion_future.IsReady());
}

TEST_P(AnnotatedPageContentCapturerTest, NewLoadInvalidatesPreviousRequest) {
  base::test::TestFuture<optimization_guide::AIPageContentResultOrError>
      completion_future;
  std::unique_ptr<AnnotatedPageContentCapturerImpl> capturer =
      CreateCapturer(completion_future.GetCallback());

  optimization_guide::OnAIPageContentDone first_request_callback;
  optimization_guide::OnAIPageContentDone second_request_callback;

  EXPECT_CALL(mock_get_page_content_, Run)
      .WillOnce([&](auto, optimization_guide::OnAIPageContentDone callback) {
        first_request_callback = std::move(callback);
      })
      .WillOnce([&](auto, optimization_guide::OnAIPageContentDone callback) {
        second_request_callback = std::move(callback);
      });

  content::WebContentsTester::For(web_contents())->TestSetIsLoading(false);

  capturer->OnPageStable();
  ASSERT_TRUE(first_request_callback);

  capturer->OnPageStable();
  ASSERT_TRUE(second_request_callback);

  // The second `OnPageStable` should invalidate the
  // first callback from being executed.
  optimization_guide::AIPageContentResult first_page_content_result;
  first_page_content_result.proto.mutable_root_node()
      ->mutable_content_attributes()
      ->set_common_ancestor_dom_node_id(3);
  std::move(first_request_callback).Run(std::move(first_page_content_result));
  EXPECT_FALSE(completion_future.IsReady());

  optimization_guide::AIPageContentResult second_page_content_result;
  second_page_content_result.proto.mutable_root_node()
      ->mutable_content_attributes()
      ->set_common_ancestor_dom_node_id(5);
  std::move(second_request_callback).Run(std::move(second_page_content_result));
  ASSERT_TRUE(completion_future.Wait());
  EXPECT_TRUE(completion_future.Get().has_value());
}
