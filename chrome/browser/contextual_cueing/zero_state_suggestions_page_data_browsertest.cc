// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/zero_state_suggestions_page_data.h"

#include "base/test/test_future.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/zero_state_suggestions.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace contextual_cueing {

using ::testing::_;
using ::testing::WithArgs;

class ZeroStateSuggestionsPageDataBrowserTest : public InProcessBrowserTest {
 public:
  ZeroStateSuggestionsPageDataBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_cueing::kContextualCueing, {}},
         {contextual_cueing::kGlicZeroStateSuggestions, {}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ZeroStateSuggestionsPageDataBrowserTest, BasicFlow) {
  auto fake_optimization_guide_keyed_service =
      testing::NiceMock<MockOptimizationGuideKeyedService>();
  ON_CALL(fake_optimization_guide_keyed_service, ExecuteModel(_, _, _, _))
      .WillByDefault(WithArgs<1, 3>(
          [](const google::protobuf::MessageLite& request_metadata,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            const auto* request = reinterpret_cast<
                const optimization_guide::proto::ZeroStateSuggestionsRequest*>(
                &request_metadata);
            EXPECT_EQ("AB\n\np-tag\n\nCD",
                      request->page_context().inner_text());
            EXPECT_EQ("title", request->page_context().title());
            EXPECT_NE(std::string::npos,
                      request->page_context().url().find(
                          "/optimization_guide/zss_page.html"));

            optimization_guide::proto::ZeroStateSuggestionsResponse response;
            response.add_suggestions()->set_label("suggestion 1");
            response.add_suggestions()->set_label("suggestion 2");
            response.add_suggestions()->set_label("suggestion 3");
            std::string serialized_metadata;
            response.SerializeToString(&serialized_metadata);

            optimization_guide::proto::Any any_result;
            any_result.set_type_url(
                base::StrCat({"type.googleapis.com/", response.GetTypeName()}));
            any_result.set_value(serialized_metadata);

            std::move(callback).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(
                    any_result, nullptr),
                nullptr);
          }));

  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/optimization_guide/zss_page.html")));

  base::test::TestFuture<std::optional<std::vector<std::string>>> future;

  ZeroStateSuggestionsPageData::CreateForPage(
      web_contents->GetPrimaryPage(), web_contents,
      &fake_optimization_guide_keyed_service, /*is_fre=*/false,
      future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ("suggestion 1", future.Get().value()[0]);
  EXPECT_EQ("suggestion 2", future.Get().value()[1]);
  EXPECT_EQ("suggestion 3", future.Get().value()[2]);
}

}  // namespace contextual_cueing
