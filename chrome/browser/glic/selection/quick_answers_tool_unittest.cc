// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/quick_answers_tool.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/strcat.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/selection/explain_fulfillment.mojom.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/selection/mojom/action.mojom.h"
#include "chrome/browser/selection/suggestion.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/feature_registry/feature_keys.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/quick_answers.pb.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/quick_answers_metadata.pb.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/generic_pending_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "url/gurl.h"

namespace glic {
namespace {

using ::base::test::RunOnceCallback;
using ::base::test::RunOnceCallbackRepeatedly;
using ::testing::_;
using ::testing::An;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

optimization_guide::OptimizationMetadata MakeQuickAnswersMetadata(
    bool is_eligible) {
  optimization_guide::proto::QuickAnswersMetadata quick_answers_metadata;
  quick_answers_metadata.set_is_eligible(is_eligible);
  optimization_guide::OptimizationMetadata metadata;
  metadata.set_any_metadata(
      optimization_guide::AnyWrapProto(quick_answers_metadata));
  return metadata;
}

class QuickAnswersToolTest : public ChromeRenderViewHostTestHarness {
 protected:
  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        OptimizationGuideKeyedServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              NiceMock<MockOptimizationGuideKeyedService>>();
        })}};
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ON_CALL(mock_tab_, GetContents()).WillByDefault(Return(web_contents()));
    ON_CALL(mock_tab_, GetProfile()).WillByDefault(Return(profile()));
    ON_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
  }

  MockOptimizationGuideKeyedService* SetupMockOptimizationGuide(
      bool default_eligible = true) {
    auto* service = static_cast<NiceMock<MockOptimizationGuideKeyedService>*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile()));
    if (default_eligible) {
      ON_CALL(*service,
              CanApplyOptimization(
                  _, optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
          .WillByDefault(RunOnceCallbackRepeatedly<2>(
              optimization_guide::OptimizationGuideDecision::kTrue,
              MakeQuickAnswersMetadata(/*is_eligible=*/true)));
    }
    return service;
  }

  void AcceptFre() {
    profile()->GetPrefs()->SetBoolean(prefs::kGlicQuickAnswersFreAccepted,
                                      true);
  }

  ui::UnownedUserDataHost user_data_host_;
  tabs::MockTabInterface mock_tab_;
};

class QuickAnswersToolNoServiceTest : public QuickAnswersToolTest {
 protected:
  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {};
  }
};

TEST_F(QuickAnswersToolTest, ExecutesQuickAnswersModelWithExpectedRequest) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/article"));
  content::WebContentsTester::For(web_contents())->SetTitle(u"Example Article");
  AcceptFre();

  auto* mock_opt_guide = SetupMockOptimizationGuide();

  ::selection::AreaOfInterest aoi;
  aoi.selected_text = u"quantum computing";
  aoi.text_surrounding_selection =
      u"Learn about quantum computing and its applications.";

  QuickAnswersTool tool(mock_tab_);
  optimization_guide::proto::SmartSelectionSuggestion server_suggestion;
  std::unique_ptr<::selection::Suggestion> suggestion =
      tool.CreateSuggestion(aoi, server_suggestion);

  ASSERT_NE(suggestion, nullptr);
  EXPECT_EQ(suggestion->GetLabel(), u"Explain");
  EXPECT_EQ(suggestion->GetToolId(),
            optimization_guide::proto::SMART_SELECTION_TOOL_QUICK_ANSWERS);
  ASSERT_TRUE(suggestion->GetAction()->is_inline_fulfillment());
  EXPECT_EQ(suggestion->GetAction()->get_inline_fulfillment()->resource_name,
            "explain_fulfillment.js");

  bool model_executed = false;
  EXPECT_CALL(
      *mock_opt_guide,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kQuickAnswers,
                   _, _, _))
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
  suggestion->Execute(mojo::GenericPendingAssociatedReceiver(
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
  AcceptFre();

  auto* mock_opt_guide = SetupMockOptimizationGuide();

  ::selection::AreaOfInterest aoi;
  aoi.selected_text = u"selected term";
  aoi.text_surrounding_selection = u"Surrounding context for selected term.";

  QuickAnswersTool tool(mock_tab_);
  std::unique_ptr<::selection::Suggestion> suggestion =
      tool.CreateSuggestion(aoi);

  ASSERT_NE(suggestion, nullptr);

  bool model_executed = false;
  EXPECT_CALL(
      *mock_opt_guide,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kQuickAnswers,
                   _, _, _))
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
  suggestion->Execute(mojo::GenericPendingAssociatedReceiver(
      fulfillment.BindNewEndpointAndPassDedicatedReceiver()));

  base::test::TestFuture<const std::string&> explanation;
  fulfillment->GetExplanation(explanation.GetCallback());

  EXPECT_EQ(explanation.Get(), l10n_util::GetStringUTF8(IDS_GLIC_ERROR_NOTICE));
  EXPECT_TRUE(model_executed);
}

TEST_F(QuickAnswersToolTest, GetExplanationNeedsAcceptedFre) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/outer"));
  auto* mock_opt_guide = SetupMockOptimizationGuide();
  EXPECT_CALL(*mock_opt_guide, ExecuteModel).Times(0);

  ::selection::AreaOfInterest aoi;
  aoi.selected_text = u"selected term";
  aoi.text_surrounding_selection = u"Surrounding context for selected term.";
  QuickAnswersTool tool(mock_tab_);
  std::unique_ptr<::selection::Suggestion> suggestion =
      tool.CreateSuggestion(aoi);
  ASSERT_NE(suggestion, nullptr);

  mojo::AssociatedRemote<selection::ExplainFulfillment> fulfillment;
  suggestion->Execute(mojo::GenericPendingAssociatedReceiver(
      fulfillment.BindNewEndpointAndPassDedicatedReceiver()));

  // Nothing goes to the model before the FRE is accepted. The mock tab has
  // no browser window, so the FRE can't show and consent is declined.
  base::test::TestFuture<const std::string&> explanation;
  fulfillment->GetExplanation(explanation.GetCallback());
  EXPECT_EQ(explanation.Get(), l10n_util::GetStringUTF8(IDS_GLIC_ERROR_NOTICE));
  base::test::TestFuture<bool> consent;
  fulfillment->RequestConsent(consent.GetCallback());
  EXPECT_FALSE(consent.Get());
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kGlicQuickAnswersFreAccepted));
}

TEST_F(QuickAnswersToolTest,
       DoesNotPresentSuggestionWhenSelectedOrSurroundingTextIsEmpty) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/article"));
  SetupMockOptimizationGuide();
  QuickAnswersTool tool(mock_tab_);

  // Both unset.
  {
    ::selection::AreaOfInterest aoi;
    EXPECT_EQ(tool.CreateSuggestion(aoi), nullptr);
  }

  // Missing selected_text.
  {
    ::selection::AreaOfInterest aoi;
    aoi.text_surrounding_selection = u"Surrounding text";
    EXPECT_EQ(tool.CreateSuggestion(aoi), nullptr);
  }

  // Empty selected_text.
  {
    ::selection::AreaOfInterest aoi;
    aoi.selected_text = u"";
    aoi.text_surrounding_selection = u"Surrounding text";
    EXPECT_EQ(tool.CreateSuggestion(aoi), nullptr);
  }

  // Missing text_surrounding_selection.
  {
    ::selection::AreaOfInterest aoi;
    aoi.selected_text = u"Selected";
    EXPECT_EQ(tool.CreateSuggestion(aoi), nullptr);
  }

  // Empty text_surrounding_selection.
  {
    ::selection::AreaOfInterest aoi;
    aoi.selected_text = u"Selected";
    aoi.text_surrounding_selection = u"";
    EXPECT_EQ(tool.CreateSuggestion(aoi), nullptr);
  }
}

TEST_F(QuickAnswersToolTest, ChecksEligibilityOnCreationAndNavigation) {
  auto* mock_opt_guide = SetupMockOptimizationGuide(/*default_eligible=*/false);

  const GURL url1("https://example.com/eligible");
  const GURL url2("https://example.com/ineligible");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url1);

  optimization_guide::OptimizationGuideDecisionCallback url1_callback;
  EXPECT_CALL(*mock_opt_guide,
              RegisterOptimizationTypes(ElementsAre(
                  optimization_guide::proto::OptimizationType::QUICK_ANSWERS)));
  EXPECT_CALL(
      *mock_opt_guide,
      CanApplyOptimization(
          url1, optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
          An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(MoveArg<2>(&url1_callback));

  QuickAnswersTool tool(mock_tab_);
  EXPECT_EQ(QuickAnswersTool::From(&mock_tab_), &tool);
  EXPECT_FALSE(tool.IsEligible());

  ::selection::AreaOfInterest aoi;
  aoi.selected_text = u"Selected";
  aoi.text_surrounding_selection = u"Surrounding Selected text.";
  EXPECT_EQ(tool.CreateSuggestion(aoi), nullptr);

  ASSERT_TRUE(url1_callback);
  std::move(url1_callback)
      .Run(optimization_guide::OptimizationGuideDecision::kTrue,
           MakeQuickAnswersMetadata(/*is_eligible=*/true));
  EXPECT_TRUE(tool.IsEligible());
  EXPECT_NE(tool.CreateSuggestion(aoi), nullptr);

  // Navigate to an ineligible URL.
  EXPECT_CALL(
      *mock_opt_guide,
      CanApplyOptimization(
          url2, optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
          An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          MakeQuickAnswersMetadata(/*is_eligible=*/false)));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url2);
  EXPECT_FALSE(tool.IsEligible());
  EXPECT_EQ(tool.CreateSuggestion(aoi), nullptr);
}

TEST_F(QuickAnswersToolNoServiceTest,
       FailsClosedWithoutOptimizationGuideService) {
  ::selection::AreaOfInterest aoi;
  aoi.selected_text = u"Selected";
  aoi.text_surrounding_selection = u"Surrounding Selected text.";

  ASSERT_EQ(OptimizationGuideKeyedServiceFactory::GetForProfile(profile()),
            nullptr);
  QuickAnswersTool tool_no_service(mock_tab_);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/no-service"));
  EXPECT_FALSE(tool_no_service.IsEligible());
  EXPECT_EQ(tool_no_service.CreateSuggestion(aoi), nullptr);
}

TEST_F(QuickAnswersToolTest, FailsClosedOnFalseUnknownOrMissingMetadata) {
  ::selection::AreaOfInterest aoi;
  aoi.selected_text = u"Selected";
  aoi.text_surrounding_selection = u"Surrounding Selected text.";

  auto* mock_opt_guide = SetupMockOptimizationGuide(/*default_eligible=*/false);
  QuickAnswersTool tool(mock_tab_);

  // kFalse -> ineligible.
  const GURL url_false("https://example.com/false");
  EXPECT_CALL(
      *mock_opt_guide,
      CanApplyOptimization(
          url_false, optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
          An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kFalse,
          optimization_guide::OptimizationMetadata()));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url_false);
  EXPECT_FALSE(tool.IsEligible());
  EXPECT_EQ(tool.CreateSuggestion(aoi), nullptr);

  // kUnknown -> ineligible.
  const GURL url_unknown("https://example.com/unknown");
  EXPECT_CALL(*mock_opt_guide,
              CanApplyOptimization(
                  url_unknown,
                  optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kUnknown,
          optimization_guide::OptimizationMetadata()));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url_unknown);
  EXPECT_FALSE(tool.IsEligible());
  EXPECT_EQ(tool.CreateSuggestion(aoi), nullptr);

  // kTrue with missing metadata -> ineligible.
  const GURL url_missing("https://example.com/missing");
  EXPECT_CALL(*mock_opt_guide,
              CanApplyOptimization(
                  url_missing,
                  optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          optimization_guide::OptimizationMetadata()));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url_missing);
  EXPECT_FALSE(tool.IsEligible());
  EXPECT_EQ(tool.CreateSuggestion(aoi), nullptr);
}

TEST_F(QuickAnswersToolTest,
       SkipsFragmentNavigationAndIgnoresStaleCallbackAfterNavigation) {
  auto* mock_opt_guide = SetupMockOptimizationGuide(/*default_eligible=*/false);
  QuickAnswersTool tool(mock_tab_);

  const GURL url1("https://example.com/page1");
  const GURL url2("https://example.com/page2");
  optimization_guide::OptimizationGuideDecisionCallback stale_callback;
  optimization_guide::OptimizationGuideDecisionCallback fresh_callback;

  EXPECT_CALL(
      *mock_opt_guide,
      CanApplyOptimization(
          url1, optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
          An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(MoveArg<2>(&stale_callback));
  EXPECT_CALL(
      *mock_opt_guide,
      CanApplyOptimization(
          url2, optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
          An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(MoveArg<2>(&fresh_callback));

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url1);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url2);
  ASSERT_TRUE(stale_callback);
  ASSERT_TRUE(fresh_callback);

  std::move(fresh_callback)
      .Run(optimization_guide::OptimizationGuideDecision::kTrue,
           MakeQuickAnswersMetadata(/*is_eligible=*/true));
  std::move(stale_callback)
      .Run(optimization_guide::OptimizationGuideDecision::kTrue,
           MakeQuickAnswersMetadata(/*is_eligible=*/false));
  EXPECT_TRUE(tool.IsEligible());

  // Fragment-only same-document navigation should not re-query.
  EXPECT_CALL(*mock_opt_guide,
              CanApplyOptimization(
                  _, optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);
  auto fragment_nav = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com/page2#section"), main_rfh());
  fragment_nav->CommitSameDocument();
  EXPECT_TRUE(tool.IsEligible());
}

}  // namespace
}  // namespace glic
