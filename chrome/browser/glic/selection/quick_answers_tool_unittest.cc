// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/quick_answers_tool.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/selection/explain_fulfillment.mojom.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/selection/mojom/action.mojom.h"
#include "chrome/browser/selection/suggestion.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/proto/features/quick_answers.pb.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/generic_pending_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace glic {
namespace {

class QuickAnswersToolTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ON_CALL(mock_tab_, GetContents())
        .WillByDefault(testing::Return(web_contents()));
    ON_CALL(mock_tab_, GetProfile()).WillByDefault(testing::Return(profile()));
  }

  MockOptimizationGuideKeyedService* SetupMockOptimizationGuide() {
    return static_cast<testing::NiceMock<MockOptimizationGuideKeyedService>*>(
        OptimizationGuideKeyedServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile(),
                base::BindRepeating([](content::BrowserContext* context)
                                        -> std::unique_ptr<KeyedService> {
                  return std::make_unique<
                      testing::NiceMock<MockOptimizationGuideKeyedService>>();
                })));
  }

  tabs::MockTabInterface mock_tab_;
};

TEST_F(QuickAnswersToolTest, ExecutesQuickAnswersModelWithExpectedRequest) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/article"));
  content::WebContentsTester::For(web_contents())->SetTitle(u"Example Article");

  auto* mock_opt_guide = SetupMockOptimizationGuide();

  ::selection::AreaOfInterest aoi;
  aoi.selected_text = u"quantum computing";
  aoi.text_surrounding_selection =
      u"Learn about quantum computing and its applications.";

  QuickAnswersTool tool(mock_tab_);
  std::vector<std::unique_ptr<::selection::Suggestion>> received_suggestions;
  bool received_complete = false;

  tool.RequestSuggestions(
      aoi,
      base::BindLambdaForTesting(
          [&](std::vector<std::unique_ptr<::selection::Suggestion>> suggestions,
              bool complete) {
            received_suggestions = std::move(suggestions);
            received_complete = complete;
          }));

  EXPECT_TRUE(received_complete);
  ASSERT_EQ(received_suggestions.size(), 1u);
  EXPECT_EQ(received_suggestions[0]->GetLabel(), u"Explain");
  ASSERT_TRUE(received_suggestions[0]->GetAction()->is_inline_fulfillment());
  EXPECT_EQ(received_suggestions[0]
                ->GetAction()
                ->get_inline_fulfillment()
                ->resource_name,
            "explain_fulfillment.js");

  bool model_executed = false;
  EXPECT_CALL(
      *mock_opt_guide,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kQuickAnswers,
                   testing::_, testing::_, testing::_))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey capability,
              const google::protobuf::MessageLite& request,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            model_executed = true;
            const auto* qa_req = static_cast<
                const optimization_guide::proto::QuickAnswersRequest*>(
                &request);
            EXPECT_EQ(qa_req->selected_text(), "quantum computing");
            EXPECT_EQ(qa_req->surrounding_text(),
                      "Learn about quantum computing and its applications.");
            EXPECT_EQ(qa_req->page_title(), "Example Article");
            EXPECT_EQ(qa_req->page_url(), "https://example.com/article");
            EXPECT_EQ(qa_req->user_locale(),
                      g_browser_process->GetApplicationLocale());
            EXPECT_FALSE(qa_req->has_annotated_page_content());

            optimization_guide::proto::QuickAnswersResponse response_msg;
            response_msg.set_answer(
                "Quantum computing processes information using qubits.");
            optimization_guide::proto::Any any;
            any.set_value(response_msg.SerializeAsString());
            any.set_type_url(base::StrCat(
                {"type.googleapis.com/", response_msg.GetTypeName()}));

            std::move(callback).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(
                    std::move(any), nullptr),
                nullptr);
          });

  mojo::AssociatedRemote<selection::ExplainFulfillment> fulfillment;
  received_suggestions[0]->Execute(mojo::GenericPendingAssociatedReceiver(
      fulfillment.BindNewEndpointAndPassDedicatedReceiver()));

  base::test::TestFuture<const std::string&> explanation;
  fulfillment->GetExplanation(explanation.GetCallback());

  EXPECT_EQ(explanation.Get(),
            "Quantum computing processes information using qubits.");
  EXPECT_TRUE(model_executed);
}

TEST_F(QuickAnswersToolTest, HandlesModelExecutionError) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/outer"));
  content::WebContentsTester::For(web_contents())->SetTitle(u"Outer Page");

  auto* mock_opt_guide = SetupMockOptimizationGuide();

  ::selection::AreaOfInterest aoi;
  aoi.selected_text = u"selected term";
  aoi.text_surrounding_selection = u"Surrounding context for selected term.";

  QuickAnswersTool tool(mock_tab_);
  std::vector<std::unique_ptr<::selection::Suggestion>> received_suggestions;
  tool.RequestSuggestions(
      aoi,
      base::BindLambdaForTesting(
          [&](std::vector<std::unique_ptr<::selection::Suggestion>> suggestions,
              bool complete) {
            received_suggestions = std::move(suggestions);
          }));

  ASSERT_EQ(received_suggestions.size(), 1u);

  bool model_executed = false;
  EXPECT_CALL(
      *mock_opt_guide,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kQuickAnswers,
                   testing::_, testing::_, testing::_))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey capability,
              const google::protobuf::MessageLite& request,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            model_executed = true;
            const auto* qa_req = static_cast<
                const optimization_guide::proto::QuickAnswersRequest*>(
                &request);
            EXPECT_EQ(qa_req->selected_text(), "selected term");
            EXPECT_EQ(qa_req->surrounding_text(),
                      "Surrounding context for selected term.");
            EXPECT_FALSE(qa_req->has_annotated_page_content());

            auto error = optimization_guide::
                OptimizationGuideModelExecutionError::FromModelExecutionError(
                    optimization_guide::OptimizationGuideModelExecutionError::
                        ModelExecutionError::kGenericFailure);
            std::move(callback).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(
                    base::unexpected(error), nullptr),
                nullptr);
          });

  mojo::AssociatedRemote<selection::ExplainFulfillment> fulfillment;
  received_suggestions[0]->Execute(mojo::GenericPendingAssociatedReceiver(
      fulfillment.BindNewEndpointAndPassDedicatedReceiver()));

  base::test::TestFuture<const std::string&> explanation;
  fulfillment->GetExplanation(explanation.GetCallback());

  EXPECT_EQ(explanation.Get(), l10n_util::GetStringUTF8(IDS_GLIC_ERROR_NOTICE));
  EXPECT_TRUE(model_executed);
}

TEST_F(QuickAnswersToolTest,
       DoesNotPresentSuggestionWhenSelectedOrSurroundingTextIsEmpty) {
  QuickAnswersTool tool(mock_tab_);

  auto request_suggestions = [&](const ::selection::AreaOfInterest& aoi) {
    std::vector<std::unique_ptr<::selection::Suggestion>> received_suggestions;
    bool received_complete = false;
    tool.RequestSuggestions(
        aoi, base::BindLambdaForTesting(
                 [&](std::vector<std::unique_ptr<::selection::Suggestion>>
                         suggestions,
                     bool complete) {
                   received_suggestions = std::move(suggestions);
                   received_complete = complete;
                 }));
    EXPECT_TRUE(received_complete);
    return received_suggestions;
  };

  // Both unset.
  {
    ::selection::AreaOfInterest aoi;
    EXPECT_TRUE(request_suggestions(aoi).empty());
  }

  // Missing selected_text.
  {
    ::selection::AreaOfInterest aoi;
    aoi.text_surrounding_selection = u"Surrounding text";
    EXPECT_TRUE(request_suggestions(aoi).empty());
  }

  // Empty selected_text.
  {
    ::selection::AreaOfInterest aoi;
    aoi.selected_text = u"";
    aoi.text_surrounding_selection = u"Surrounding text";
    EXPECT_TRUE(request_suggestions(aoi).empty());
  }

  // Missing text_surrounding_selection.
  {
    ::selection::AreaOfInterest aoi;
    aoi.selected_text = u"Selected";
    EXPECT_TRUE(request_suggestions(aoi).empty());
  }

  // Empty text_surrounding_selection.
  {
    ::selection::AreaOfInterest aoi;
    aoi.selected_text = u"Selected";
    aoi.text_surrounding_selection = u"";
    EXPECT_TRUE(request_suggestions(aoi).empty());
  }
}

}  // namespace
}  // namespace glic
