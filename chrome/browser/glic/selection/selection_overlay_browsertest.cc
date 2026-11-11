// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_selection_observer.h"
#include "chrome/browser/glic/glic_warming_checks.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/provider_features.h"
#include "chrome/browser/glic/selection/explain_suggestion.h"
#include "chrome/browser/glic/selection/explain_suggestion_fre.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/glic/selection/selection_overlay_untrusted_ui.h"
#include "chrome/browser/glic/selection/selection_suggestion.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/scoped_page_context_eligibility.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/selection/features.h"
#include "chrome/browser/selection/mojom/action.mojom.h"
#include "chrome/browser/selection/suggestion_service.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/mojom/echo.test-mojom.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/optimization_guide/content/browser/no_response_ai_page_content_agent.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_observer.h"
#include "components/optimization_guide/core/feature_registry/feature_keys.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/quick_answers.pb.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/quick_answers_metadata.pb.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/generic_pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

namespace glic {

using ::testing::NiceMock;

class SelectionOverlayBrowserTest : public GlicBrowserTest {
 public:
  SelectionOverlayBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(::features::kGlicCaptureRegion);
  }
  ~SelectionOverlayBrowserTest() override = default;

  void TearDown() override {
    GlicBrowserTest::TearDown();
    ForceConnectionTypeForTesting(std::nullopt);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SelectionOverlayBrowserTest,
                       SelectionUsedFromController) {
  base::HistogramTester histogram_tester;

  // 1. Navigate to a valid page.
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());

  // 2. Open Glic.
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());

  // 3. Show the selection overlay.
  content::WebContents* web_contents = tab->GetContents();
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);

  // 4. Wait until state is State::kOverlay.
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  // 5. Adjust region.
  static_cast<selection::SelectionOverlayPageHandler*>(controller)
      ->AdjustRegion(
          selection::SelectedRegion::New(
              base::UnguessableToken::Create(),
              selection::RegionShape::NewRect(gfx::RectF(10, 10, 10, 10))),
          /*is_using_keyboard=*/false);

  // 6. Submit user input and verify metrics.
  SimulateUserInputSubmitted(instance, mojom::WebClientMode::kText);
  histogram_tester.ExpectBucketCount(
      "Glic.Instance.InputSubmitted.SelectionCount", 1, 1);

  // Submit another input, should still log 1.
  SimulateUserInputSubmitted(instance, mojom::WebClientMode::kText);
  histogram_tester.ExpectBucketCount(
      "Glic.Instance.InputSubmitted.SelectionCount", 1, 2);

  // Close the overlay.
  controller->Close();

  // Submit another input, should log 0.
  SimulateUserInputSubmitted(instance, mojom::WebClientMode::kText);
  histogram_tester.ExpectBucketCount(
      "Glic.Instance.InputSubmitted.SelectionCount", 0, 1);
  histogram_tester.ExpectTotalCount(
      "Glic.Instance.InputSubmitted.SelectionCount", 3);
  histogram_tester.ExpectTotalCount(
      "Glic.Prewarming.ChecksResult.SelectionOverlay", 0);
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayBrowserTest,
                       WarmsWhenOpenedWithoutInstance) {
  ForceConnectionTypeForTesting(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
  base::HistogramTester histogram_tester;

  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  EXPECT_FALSE(coordinator()
                   .GetWebContentsWarmingPoolForTesting()
                   .HasWarmedContainerForTesting());

  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);

  EXPECT_TRUE(RunUntil(
      [this]() {
        return coordinator()
            .GetWebContentsWarmingPoolForTesting()
            .HasWarmedContainerForTesting();
      },
      "Wait for container to warm"));

  histogram_tester.ExpectBucketCount(
      "Glic.Prewarming.ChecksResult.SelectionOverlay",
      GlicPrewarmingChecksResult::kSuccess, 1);
  histogram_tester.ExpectBucketCount(
      "Glic.WarmingPool.ContainerCreationReason",
      GlicWebContentsWarmingPool::ContainerCreationReason::kSelectionOverlay,
      1);
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayBrowserTest, ClosesWhenLensOpened) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  std::optional<SelectionOverlayController::CloseReason> close_reason;
  base::CallbackListSubscription closed_subscription =
      controller->RegisterOverlayClosedCallback(base::BindLambdaForTesting(
          [&close_reason](SelectionOverlayController::CloseReason reason) {
            close_reason = reason;
          }));

  auto* lens_search_controller = LensSearchController::From(tab);
  ASSERT_TRUE(lens_search_controller);
  lens_search_controller->OpenLensOverlay(
      lens::LensOverlayInvocationSource::kToolbar);

  EXPECT_EQ(controller->state(), SelectionOverlayController::State::kOff);
  EXPECT_EQ(close_reason, SelectionOverlayController::CloseReason::kOther);
  EXPECT_FALSE(lens_search_controller->IsOff());
  ASSERT_OK(RunUntilEqual(
      [&]() {
        return lens_search_controller->lens_overlay_controller()->state();
      },
      LensOverlayController::State::kOverlay,
      "Timeout waiting for LensOverlayController state to be kOverlay"));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayBrowserTest, ClosesLensWhenOpened) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  auto* lens_search_controller = LensSearchController::From(tab);
  ASSERT_TRUE(lens_search_controller);
  lens_search_controller->OpenLensOverlay(
      lens::LensOverlayInvocationSource::kAppMenu);
  EXPECT_FALSE(lens_search_controller->IsOff());
  ASSERT_OK(RunUntilEqual(
      [&]() {
        return lens_search_controller->lens_overlay_controller()->state();
      },
      LensOverlayController::State::kOverlay,
      "Timeout waiting for LensOverlayController state to be kOverlay"));

  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);

  EXPECT_TRUE(lens_search_controller->IsOff());
  EXPECT_EQ(lens_search_controller->lens_overlay_controller()->state(),
            LensOverlayController::State::kOff);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));
}

namespace {

class TestSuggestedActionsListener {
 public:
  TestSuggestedActionsListener() = default;
  ~TestSuggestedActionsListener() = default;

  SelectionOverlayController::SuggestedActionsCallback GetCallback() {
    return base::BindRepeating(
        &TestSuggestedActionsListener::OnSuggestedActionsAvailable,
        weak_ptr_factory_.GetWeakPtr());
  }

  void OnSuggestedActionsAvailable(
      const std::vector<selection::SuggestedActionPtr>& actions) {
    for (const auto& action : actions) {
      actions_.push_back(action->Clone());
    }
    batches_received_++;
    if (run_loop_ && batches_received_ >= expected_batches_) {
      run_loop_->Quit();
    }
  }

  void WaitForBatches(size_t expected_batches) {
    if (batches_received_ >= expected_batches) {
      return;
    }
    expected_batches_ = expected_batches;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  const std::vector<selection::SuggestedActionPtr>& actions() const {
    return actions_;
  }

 private:
  std::vector<selection::SuggestedActionPtr> actions_;
  size_t batches_received_ = 0;
  size_t expected_batches_ = 0;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::WeakPtrFactory<TestSuggestedActionsListener> weak_ptr_factory_{this};
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SelectionOverlayBrowserTest,
                       SuggestedActionsDisabledByDefault) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);

  TestSuggestedActionsListener listener;
  controller->GetSuggestedActionsForTesting(listener.GetCallback());
  listener.WaitForBatches(1);
  EXPECT_TRUE(listener.actions().empty());
}

namespace {

class SelectionOverlayEligibilityBrowserTest
    : public SelectionOverlayBrowserTest {
 public:
  void SetUpOnMainThread() override {
    SelectionOverlayBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  tabs::TabInterface* CreateTabWithEligibility(
      const GURL& url,
      optimization_guide::PageContextEligibilityStatus expected) {
    tabs::TabInterface* tab = CreateAndActivateTab(url);
    CHECK(content::ExecJs(
        tab->GetContents(),
        content::JsReplace("const f = document.createElement('iframe');"
                           "f.src = $1;"
                           "document.body.appendChild(f);",
                           url)));
    CHECK(content::WaitForLoadStop(tab->GetContents()));
    auto* helper = tabs::PageContextEligibilityHelper::From(tab);
    CHECK(helper);
    CHECK(base::test::RunUntil([helper, expected]() {
      return helper->IsPageContextEligible() == expected;
    }));
    return tab;
  }

  // Updates the subframe URL to an ineligible path and appends a child frame so
  // `PageContextEligibilityObserver::RenderFrameCreated` re-evaluates
  // eligibility immediately without navigating the primary main frame.
  void MakePageIneligible(tabs::TabInterface* tab) {
    content::ExecuteScriptAsync(
        tab->GetContents(),
        "window[0].history.replaceState(null, '', '/ineligible');"
        "document.body.appendChild(document.createElement('iframe'));");
  }

 private:
  ScopedPageContextEligibilityForTesting scoped_eligibility_;
};

IN_PROC_BROWSER_TEST_F(SelectionOverlayEligibilityBrowserTest,
                       ShowRejectedOnIneligiblePage) {
  tabs::TabInterface* tab = CreateTabWithEligibility(
      embedded_test_server()->GetURL("ineligible.example.com", "/title1.html"),
      optimization_guide::PageContextEligibilityStatus::kNotEligible);
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);

  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);

  EXPECT_EQ(controller->state(), SelectionOverlayController::State::kOff);
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayEligibilityBrowserTest,
                       ClosesWhenEligibilityLostDuringFetch) {
  tabs::TabInterface* tab = CreateTabWithEligibility(
      GetSimpleTestUrl(),
      optimization_guide::PageContextEligibilityStatus::kEligible);
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);

  optimization_guide::NoResponseAIPageContentAgent slow_apc_agent(
      tab->GetContents()->GetPrimaryMainFrame());
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
  ASSERT_EQ(controller->state(),
            SelectionOverlayController::State::kScreenshot);
  ASSERT_FALSE(controller->HasPageContextForTesting());

  MakePageIneligible(tab);

  EXPECT_TRUE(base::test::RunUntil([controller]() {
    return controller->state() == SelectionOverlayController::State::kOff;
  }));
  EXPECT_FALSE(controller->HasPageContextForTesting());
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayEligibilityBrowserTest,
                       ClosesWhenEligibilityLostAfterFetch) {
  tabs::TabInterface* tab = CreateTabWithEligibility(
      GetSimpleTestUrl(),
      optimization_guide::PageContextEligibilityStatus::kEligible);
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);

  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
  ASSERT_TRUE(base::test::RunUntil([controller]() {
    return controller->state() == SelectionOverlayController::State::kOverlay &&
           controller->HasPageContextForTesting();
  }));

  MakePageIneligible(tab);

  EXPECT_TRUE(base::test::RunUntil([controller]() {
    return controller->state() == SelectionOverlayController::State::kOff;
  }));
}

// The helper stops observing a backgrounded tab, so a page that becomes
// ineligible in the background is caught when the tab is activated again.
IN_PROC_BROWSER_TEST_F(SelectionOverlayEligibilityBrowserTest,
                       ClosesWhenIneligibleOnReturnFromBackground) {
  tabs::TabInterface* tab = CreateTabWithEligibility(
      GetSimpleTestUrl(),
      optimization_guide::PageContextEligibilityStatus::kEligible);
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);

  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
  ASSERT_TRUE(base::test::RunUntil([controller]() {
    return controller->state() == SelectionOverlayController::State::kOverlay &&
           controller->HasPageContextForTesting();
  }));

  CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_EQ(controller->state(),
            SelectionOverlayController::State::kBackground);
  ASSERT_TRUE(content::ExecJs(
      tab->GetContents(),
      "window[0].history.replaceState(null, '', '/ineligible');"));
  ASSERT_EQ(controller->state(),
            SelectionOverlayController::State::kBackground);

  GetTabListInterface()->ActivateTab(tab->GetHandle());

  EXPECT_TRUE(base::test::RunUntil([controller]() {
    return controller->state() == SelectionOverlayController::State::kOff;
  }));
}

}  // namespace

class SelectionOverlayPromptBrowserTest : public GlicBrowserTest {
 public:
  SelectionOverlayPromptBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {::features::kGlicCaptureRegion,
         ::features::kGlicSelectionOverlayPromptBox,
         ::selection::kSmartSelectionServerSuggestions},
        {});
  }
  ~SelectionOverlayPromptBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

namespace {

void AddServerSuggestionsForTesting(
    tabs::TabInterface* tab,
    std::initializer_list<std::pair<::selection::SuggestionTool::ToolId,
                                    std::string>> suggestions) {
  optimization_guide::proto::SmartSelectionSuggestionsResponse response;
  for (const auto& [tool, label] : suggestions) {
    auto* s = response.add_suggestions();
    s->set_tool(tool);
    s->set_label(label);
  }
  optimization_guide::proto::Any any;
  any.set_value(response.SerializeAsString());
  any.set_type_url(
      base::StrCat({"type.googleapis.com/", response.GetTypeName()}));
  OptimizationGuideKeyedServiceFactory::GetForProfile(tab->GetProfile())
      ->AddExecutionResultForTesting(
          optimization_guide::ModelBasedCapabilityKey::
              kSmartSelectionSuggestions,
          optimization_guide::OptimizationGuideModelExecutionResult(
              std::move(any), nullptr));
}

class FakeSelectionSuggestionTool : public ::selection::SuggestionTool {
 public:
  explicit FakeSelectionSuggestionTool(
      tabs::TabInterface* tab,
      ToolId tool_id =
          optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME)
      : tab_(tab), tool_id_(tool_id) {}
  ~FakeSelectionSuggestionTool() override = default;

  ToolId GetToolId() const override { return tool_id_; }

  std::unique_ptr<::selection::Suggestion> CreateSuggestion(
      const ::selection::AreaOfInterest& processed_area,
      const optimization_guide::proto::SmartSelectionSuggestion&
          server_suggestion) override {
    return std::make_unique<SelectionSuggestion>(
        *tab_, base::UTF8ToUTF16(server_suggestion.label()),
        server_suggestion.label());
  }

 private:
  raw_ptr<tabs::TabInterface> tab_;
  ToolId tool_id_;
};

class ScopedToolRegistration {
 public:
  ScopedToolRegistration(::selection::SuggestionService* service,
                         ::selection::SuggestionTool* tool)
      : service_(service), tool_(tool) {
    service_->RegisterTool(tool_);
  }
  ScopedToolRegistration(const ScopedToolRegistration&) = delete;
  ScopedToolRegistration& operator=(const ScopedToolRegistration&) = delete;
  ~ScopedToolRegistration() { service_->UnregisterTool(tool_); }

 private:
  const raw_ptr<::selection::SuggestionService> service_;
  const raw_ptr<::selection::SuggestionTool> tool_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SelectionOverlayPromptBrowserTest,
                       SuggestedActionsWhenEnabled) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();

  auto* suggestion_service = ::selection::SuggestionService::From(tab);
  ASSERT_TRUE(suggestion_service);
  FakeSelectionSuggestionTool tool(tab);
  ScopedToolRegistration registration(suggestion_service, &tool);
  AddServerSuggestionsForTesting(
      tab, {{optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME,
             "Explain"},
            {optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME,
             "Summarize"},
            {optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME,
             "Create Image"}});

  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  auto* handler =
      static_cast<selection::SelectionOverlayPageHandler*>(controller);
  handler->AdjustRegion(
      selection::SelectedRegion::New(
          base::UnguessableToken::Create(),
          selection::RegionShape::NewRect(gfx::RectF(0.5f, 0.5f, 0.2f, 0.2f))),
      /*is_using_keyboard=*/false);

  TestSuggestedActionsListener listener;
  controller->GetSuggestedActionsForTesting(listener.GetCallback());
  listener.WaitForBatches(1);
  const auto& actions = listener.actions();
  ASSERT_EQ(actions.size(), 3u);
  EXPECT_FALSE(actions[0]->id.is_empty());
  EXPECT_EQ(actions[0]->title, "Explain");
  EXPECT_EQ(actions[0]->icon, selection::SuggestedActionIcon::kNone);
  EXPECT_FALSE(actions[1]->id.is_empty());
  EXPECT_EQ(actions[1]->title, "Summarize");
  EXPECT_EQ(actions[1]->icon, selection::SuggestedActionIcon::kNone);
  EXPECT_FALSE(actions[2]->id.is_empty());
  EXPECT_EQ(actions[2]->title, "Create Image");
  EXPECT_EQ(actions[2]->icon, selection::SuggestedActionIcon::kNone);
  EXPECT_NE(actions[0]->id, actions[1]->id);
  EXPECT_NE(actions[1]->id, actions[2]->id);
}

class SelectionOverlayStaticSuggestionsBrowserTest : public GlicBrowserTest {
 public:
  SelectionOverlayStaticSuggestionsBrowserTest() {
    scoped_feature_list_.InitFromCommandLine(
        "GlicCaptureRegion,GlicSelectionOverlayPrompt,"
        "StaticSelectionSuggestions",
        "");
  }
  ~SelectionOverlayStaticSuggestionsBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SelectionOverlayStaticSuggestionsBrowserTest,
                       StaticSuggestionsInjectedWhenFeatureEnabled) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();

  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  auto* handler =
      static_cast<selection::SelectionOverlayPageHandler*>(controller);
  handler->AdjustRegion(
      selection::SelectedRegion::New(
          base::UnguessableToken::Create(),
          selection::RegionShape::NewRect(gfx::RectF(0.5f, 0.5f, 0.2f, 0.2f))),
      /*is_using_keyboard=*/false);

  TestSuggestedActionsListener listener;
  controller->GetSuggestedActionsForTesting(listener.GetCallback());
  listener.WaitForBatches(1);
  const auto& actions = listener.actions();
  ASSERT_EQ(actions.size(), 1u);
  EXPECT_EQ(actions[0]->title, "Ask Gemini");
  EXPECT_EQ(actions[0]->icon, selection::SuggestedActionIcon::kSpark);
  EXPECT_TRUE(actions[0]->action->is_handoff());
}

// Suggestions are asked for right away, so the surrounding text arrives before
// the redacted screenshot. Requesting them on the text alone hit the CHECK in
// `RequestNewSuggestions()`; they must wait for the screenshot too, and no
// empty list is sent before they're requested.
IN_PROC_BROWSER_TEST_F(SelectionOverlayStaticSuggestionsBrowserTest,
                       RequestsSuggestionsOnceInputsAreReady) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  gfx::Rect bounds = tab->GetContents()->GetViewBounds();
  bounds.Inset(20);
  controller->ShowWithSelection(
      tab->GetContents()->GetPrimaryMainFrame(), bounds,
      selection::InteractionOptions::New(/*hide_handles=*/true,
                                         /*skip_intro_animation=*/true),
      ::selection::EntryPoint::kInlineCue);

  TestSuggestedActionsListener listener;
  controller->GetSuggestedActionsForTesting(listener.GetCallback());
  listener.WaitForBatches(1);
  ASSERT_FALSE(listener.actions().empty());
  EXPECT_EQ(listener.actions()[0]->title, "Ask Gemini");
}

class SelectionOverlayStaticSuggestionsWithPromptBoxBrowserTest
    : public GlicBrowserTest {
 public:
  SelectionOverlayStaticSuggestionsWithPromptBoxBrowserTest() {
    scoped_feature_list_.InitFromCommandLine(
        "GlicCaptureRegion,GlicSelectionOverlayPrompt,"
        "GlicSelectionOverlayPromptBox,StaticSelectionSuggestions",
        "");
  }
  ~SelectionOverlayStaticSuggestionsWithPromptBoxBrowserTest() override =
      default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SelectionOverlayStaticSuggestionsWithPromptBoxBrowserTest,
    AskGeminiNotInjectedWhenPromptBoxEnabled) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();

  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  auto* handler =
      static_cast<selection::SelectionOverlayPageHandler*>(controller);
  handler->AdjustRegion(
      selection::SelectedRegion::New(
          base::UnguessableToken::Create(),
          selection::RegionShape::NewRect(gfx::RectF(0.5f, 0.5f, 0.2f, 0.2f))),
      /*is_using_keyboard=*/false);

  TestSuggestedActionsListener listener;
  controller->GetSuggestedActionsForTesting(listener.GetCallback());
  listener.WaitForBatches(1);
  EXPECT_TRUE(listener.actions().empty());
}

class SelectionOverlayQuickAnswersSuggestionsBrowserTest
    : public GlicBrowserTest {
 public:
  SelectionOverlayQuickAnswersSuggestionsBrowserTest() {
    scoped_feature_list_.InitFromCommandLine(
        "GlicCaptureRegion,GlicSelectionOverlayPrompt,"
        "StaticSelectionSuggestions,QuickAnswersSelectionSuggestions",
        "");
  }
  ~SelectionOverlayQuickAnswersSuggestionsBrowserTest() override = default;

 protected:
  void AddQuickAnswersHint(const GURL& url, bool is_eligible) {
    optimization_guide::proto::QuickAnswersMetadata qa_metadata;
    qa_metadata.set_is_eligible(is_eligible);
    optimization_guide::OptimizationMetadata metadata;
    metadata.set_any_metadata(optimization_guide::AnyWrapProto(qa_metadata));
    OptimizationGuideKeyedServiceFactory::GetForProfile(GetProfile())
        ->AddHintForTesting(
            url, optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
            metadata);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SelectionOverlayQuickAnswersSuggestionsBrowserTest,
                       QuickAnswersSuggestionsInjectedWhenFeatureEnabled) {
  AddQuickAnswersHint(GetSimpleTestUrl(), /*is_eligible=*/true);
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();

  ASSERT_TRUE(content::ExecJs(web_contents, R"(
    document.body.innerText = 'Before text Selected target After text';
    const textNode = document.body.firstChild;
    const range = document.createRange();
    range.setStart(textNode, 12);
    range.setEnd(textNode, 27);
    const selection = window.getSelection();
    selection.removeAllRanges();
    selection.addRange(range);
  )"));

  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);
  gfx::Rect view_bounds = web_contents->GetViewBounds();
  gfx::Rect selection_bounds(view_bounds.x() + 10, view_bounds.y() + 10, 100,
                             50);
  controller->ShowWithSelection(web_contents->GetPrimaryMainFrame(),
                                selection_bounds,
                                selection::InteractionOptions::New(),
                                ::selection::EntryPoint::kContextMenuText);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  auto* handler =
      static_cast<selection::SelectionOverlayPageHandler*>(controller);
  TestSuggestedActionsListener listener;
  controller->GetSuggestedActionsForTesting(listener.GetCallback());
  listener.WaitForBatches(1);
  const auto& actions = listener.actions();
  ASSERT_EQ(actions.size(), 2u);
  EXPECT_EQ(actions[0]->title, "Ask Gemini");
  EXPECT_EQ(actions[0]->icon, selection::SuggestedActionIcon::kSpark);
  EXPECT_TRUE(actions[0]->action->is_handoff());
  EXPECT_EQ(actions[1]->title, "Explain");
  EXPECT_EQ(actions[1]->icon, selection::SuggestedActionIcon::kExplain);
  ASSERT_TRUE(actions[1]->action->is_inline_fulfillment());
  EXPECT_EQ(actions[1]->action->get_inline_fulfillment()->resource_name,
            "explain_fulfillment.js");

  // Adjusting the region without text selection should not present the Explain
  // suggestion.
  handler->AdjustRegion(
      selection::SelectedRegion::New(
          base::UnguessableToken::Create(),
          selection::RegionShape::NewRect(gfx::RectF(0.5f, 0.5f, 0.2f, 0.2f))),
      /*is_using_keyboard=*/false);
  TestSuggestedActionsListener adjusted_listener;
  controller->GetSuggestedActionsForTesting(adjusted_listener.GetCallback());
  adjusted_listener.WaitForBatches(1);
  ASSERT_EQ(adjusted_listener.actions().size(), 1u);
  EXPECT_EQ(adjusted_listener.actions()[0]->title, "Ask Gemini");
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayQuickAnswersSuggestionsBrowserTest,
                       QuickAnswersSuggestionsNotInjectedWhenIneligible) {
  AddQuickAnswersHint(GetSimpleTestUrl(), /*is_eligible=*/false);
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();

  ASSERT_TRUE(content::ExecJs(web_contents, R"(
    document.body.innerText = 'Before text Selected target After text';
    const textNode = document.body.firstChild;
    const range = document.createRange();
    range.setStart(textNode, 12);
    range.setEnd(textNode, 27);
    const selection = window.getSelection();
    selection.removeAllRanges();
    selection.addRange(range);
  )"));

  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);
  gfx::Rect view_bounds = web_contents->GetViewBounds();
  gfx::Rect selection_bounds(view_bounds.x() + 10, view_bounds.y() + 10, 100,
                             50);
  controller->ShowWithSelection(web_contents->GetPrimaryMainFrame(),
                                selection_bounds,
                                selection::InteractionOptions::New(),
                                ::selection::EntryPoint::kContextMenuText);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  TestSuggestedActionsListener listener;
  controller->GetSuggestedActionsForTesting(listener.GetCallback());
  listener.WaitForBatches(1);
  const auto& actions = listener.actions();
  ASSERT_EQ(actions.size(), 1u);
  EXPECT_EQ(actions[0]->title, "Ask Gemini");
}

class SelectionOverlayQuickAnswersServerSuggestionsBrowserTest
    : public SelectionOverlayQuickAnswersSuggestionsBrowserTest {
 public:
  SelectionOverlayQuickAnswersServerSuggestionsBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ::selection::kSmartSelectionServerSuggestions);
  }
  ~SelectionOverlayQuickAnswersServerSuggestionsBrowserTest() override =
      default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SelectionOverlayQuickAnswersServerSuggestionsBrowserTest,
    QuickAnswersToolOnlyRegisteredWithSuggestionServiceWhenEligible) {
  AddQuickAnswersHint(GetSimpleTestUrl(), /*is_eligible=*/false);
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();

  ASSERT_TRUE(content::ExecJs(web_contents, R"(
    document.body.innerText = 'Before text Selected target After text';
    const textNode = document.body.firstChild;
    const range = document.createRange();
    range.setStart(textNode, 12);
    range.setEnd(textNode, 27);
    const selection = window.getSelection();
    selection.removeAllRanges();
    selection.addRange(range);
  )"));

  AddServerSuggestionsForTesting(
      tab, {{optimization_guide::proto::SMART_SELECTION_TOOL_QUICK_ANSWERS,
             "Explain"}});

  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);
  gfx::Rect view_bounds = web_contents->GetViewBounds();
  gfx::Rect selection_bounds(view_bounds.x() + 10, view_bounds.y() + 10, 100,
                             50);
  controller->ShowWithSelection(web_contents->GetPrimaryMainFrame(),
                                selection_bounds,
                                selection::InteractionOptions::New(),
                                ::selection::EntryPoint::kContextMenuText);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  // Since Quick Answers is ineligible, the QuickAnswersTool is not registered
  // with SuggestionService, so only the static "Ask Gemini" action is returned.
  TestSuggestedActionsListener ineligible_listener;
  controller->GetSuggestedActionsForTesting(ineligible_listener.GetCallback());
  ineligible_listener.WaitForBatches(1);
  ASSERT_EQ(ineligible_listener.actions().size(), 1u);
  EXPECT_EQ(ineligible_listener.actions()[0]->title, "Ask Gemini");

  controller->Close();
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOff,
      "Timeout waiting for SelectionOverlayController state to be kOff"));

  // Now mark the URL as eligible and navigate so QuickAnswersTool updates
  // eligibility and registers itself with SuggestionService.
  const GURL eligible_url = GetTestUrl("page.html");
  AddQuickAnswersHint(eligible_url, /*is_eligible=*/true);
  ASSERT_TRUE(content::NavigateToURL(web_contents, eligible_url));

  ASSERT_TRUE(content::ExecJs(web_contents, R"(
    document.body.innerText = 'Before text Selected target After text';
    const textNode = document.body.firstChild;
    const range = document.createRange();
    range.setStart(textNode, 12);
    range.setEnd(textNode, 27);
    const selection = window.getSelection();
    selection.removeAllRanges();
    selection.addRange(range);
  )"));

  AddServerSuggestionsForTesting(
      tab, {{optimization_guide::proto::SMART_SELECTION_TOOL_QUICK_ANSWERS,
             "Explain"}});

  controller->ShowWithSelection(web_contents->GetPrimaryMainFrame(),
                                selection_bounds,
                                selection::InteractionOptions::New(),
                                ::selection::EntryPoint::kContextMenuText);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  TestSuggestedActionsListener eligible_listener;
  controller->GetSuggestedActionsForTesting(eligible_listener.GetCallback());
  eligible_listener.WaitForBatches(2);
  ASSERT_EQ(eligible_listener.actions().size(), 2u);
  EXPECT_EQ(eligible_listener.actions()[0]->title, "Ask Gemini");
  EXPECT_EQ(eligible_listener.actions()[1]->title, "Explain");
  EXPECT_EQ(eligible_listener.actions()[1]->icon,
            selection::SuggestedActionIcon::kExplain);
}

class SelectionOverlayQuickAnswersPrewarmBrowserTest
    : public SelectionOverlayQuickAnswersSuggestionsBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  SelectionOverlayQuickAnswersPrewarmBrowserTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          ::selection::kSmartSelectionServerSuggestions);
    }
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    SelectionOverlayQuickAnswersSuggestionsBrowserTest::
        SetUpBrowserContextKeyedServices(context);
    OptimizationGuideKeyedServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              NiceMock<MockOptimizationGuideKeyedService>>();
        }));
  }

  NiceMock<MockOptimizationGuideKeyedService>& mock_opt_guide() {
    return *static_cast<NiceMock<MockOptimizationGuideKeyedService>*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(GetProfile()));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(SelectionOverlayQuickAnswersPrewarmBrowserTest,
                       PrewarmsQuickAnswersOnlyWhenServerSuggestionsDisabled) {
  const bool server_suggestions_enabled = GetParam();
  EXPECT_CALL(mock_opt_guide(),
              PrewarmModelExecution(
                  optimization_guide::ModelBasedCapabilityKey::kQuickAnswers))
      .Times(server_suggestions_enabled ? 0 : 1);

  tabs::TabInterface* const tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* const web_contents = tab->GetContents();

  SelectionOverlayController* const controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);

  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SelectionOverlayQuickAnswersPrewarmBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_F(SelectionOverlayPromptBrowserTest,
                       ShowWithSelectionPopulatesSelection) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);

  gfx::Rect view_bounds = web_contents->GetViewBounds();
  gfx::Rect selection_bounds(view_bounds.x() + 10, view_bounds.y() + 10, 100,
                             50);
  controller->ShowWithSelection(web_contents->GetPrimaryMainFrame(),
                                selection_bounds,
                                selection::InteractionOptions::New(),
                                ::selection::EntryPoint::kContextMenuText);

  EXPECT_EQ(controller->GetSelectedRegionCount(), 1u);

  controller->Close();
  EXPECT_EQ(controller->GetSelectedRegionCount(), 0u);
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayPromptBrowserTest,
                       ShowWithSelectionHiddenOnTabSwitchWithoutGlic) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);

  gfx::Rect view_bounds = web_contents->GetViewBounds();
  gfx::Rect selection_bounds(view_bounds.x() + 10, view_bounds.y() + 10, 100,
                             50);
  controller->ShowWithSelection(web_contents->GetPrimaryMainFrame(),
                                selection_bounds,
                                selection::InteractionOptions::New(),
                                ::selection::EntryPoint::kContextMenuText);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  CreateAndActivateTab(GetSimpleTestUrl());
  EXPECT_EQ(controller->state(),
            SelectionOverlayController::State::kBackground);
  EXPECT_EQ(controller->GetSelectedRegionCount(), 1u);

  ActivateTab(tab);
  EXPECT_EQ(controller->state(), SelectionOverlayController::State::kOverlay);
  EXPECT_EQ(controller->GetSelectedRegionCount(), 1u);
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayPromptBrowserTest,
                       ShowWithSelectionIgnoredWhileShowing) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);

  gfx::Rect view_bounds = web_contents->GetViewBounds();
  gfx::Rect selection_bounds(view_bounds.x() + 10, view_bounds.y() + 10, 100,
                             50);
  controller->ShowWithSelection(web_contents->GetPrimaryMainFrame(),
                                selection_bounds,
                                selection::InteractionOptions::New(),
                                ::selection::EntryPoint::kContextMenuText);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));
  static_cast<selection::SelectionOverlayPageHandler*>(controller)
      ->AdjustRegion(selection::SelectedRegion::New(
                         base::UnguessableToken::Create(),
                         selection::RegionShape::NewRect(
                             gfx::RectF(0.5f, 0.5f, 0.2f, 0.2f))),
                     /*is_using_keyboard=*/false);
  ASSERT_EQ(controller->GetSelectedRegionCount(), 2u);

  EXPECT_FALSE(controller->CanStartSession());
  controller->ShowWithSelection(web_contents->GetPrimaryMainFrame(),
                                selection_bounds,
                                selection::InteractionOptions::New(),
                                ::selection::EntryPoint::kContextMenuText);
  EXPECT_EQ(controller->GetSelectedRegionCount(), 2u);
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayPromptBrowserTest,
                       ShowWithSelectionIgnoredOnBackgroundTab) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);
  gfx::Rect view_bounds = web_contents->GetViewBounds();
  gfx::Rect selection_bounds(view_bounds.x() + 10, view_bounds.y() + 10, 100,
                             50);

  CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_FALSE(tab->IsActivated());

  EXPECT_FALSE(controller->CanStartSession());
  controller->ShowWithSelection(web_contents->GetPrimaryMainFrame(),
                                selection_bounds,
                                selection::InteractionOptions::New(),
                                ::selection::EntryPoint::kContextMenuText);
  EXPECT_EQ(controller->state(), SelectionOverlayController::State::kOff);
  EXPECT_EQ(controller->GetSelectedRegionCount(), 0u);
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayPromptBrowserTest,
                       SubmitPromptWithSelectedRegion) {
  // The panel stays closed: a page session started with the panel open is
  // handed to the panel and then outlives a prompt.
  base::HistogramTester histogram_tester;
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);

  gfx::Rect view_bounds = web_contents->GetViewBounds();
  gfx::Rect selection_bounds(view_bounds.x() + 10, view_bounds.y() + 10, 100,
                             50);
  controller->ShowWithSelection(web_contents->GetPrimaryMainFrame(),
                                selection_bounds,
                                selection::InteractionOptions::New(),
                                ::selection::EntryPoint::kContextMenuText);

  EXPECT_EQ(controller->GetSelectedRegionCount(), 1u);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->HasPageContextForTesting(); }));

  static_cast<selection::SelectionOverlayPageHandler*>(controller)
      ->SubmitPrompt("Explain this selection");

  // The browser started this session, so it dismisses the overlay itself.
  EXPECT_EQ(controller->state(), SelectionOverlayController::State::kOff);
  EXPECT_EQ(controller->GetSelectedRegionCount(), 0u);
  histogram_tester.ExpectUniqueSample("SmartSelection.Invocation.ByEntryPoint",
                                      ::selection::EntryPoint::kContextMenuText,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "SmartSelection.Invocation.ByDocumentMimeType",
      ::selection::DocumentMimeType::kHtml, 1);
  histogram_tester.ExpectUniqueSample(
      "SmartSelection.Selection.Outcome",
      ::selection::SelectionOutcome::kPreselectedText, 1);
  histogram_tester.ExpectUniqueSample(
      "SmartSelection.Submission.Outcome",
      ::selection::SubmissionOutcome::kPromptSubmitted, 1);
}

// The overlay doesn't allow a prompt before the page context is in. One that
// comes anyway is a bad message, and nothing is sent.
IN_PROC_BROWSER_TEST_F(SelectionOverlayPromptBrowserTest,
                       SubmitPromptBeforePageContextIsBadMessage) {
  base::HistogramTester histogram_tester;
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);
  // Holds back the page content, so the overlay shows without it.
  optimization_guide::NoResponseAIPageContentAgent apc_agent(
      web_contents->GetPrimaryMainFrame());

  gfx::Rect view_bounds = web_contents->GetViewBounds();
  gfx::Rect selection_bounds(view_bounds.x() + 10, view_bounds.y() + 10, 100,
                             50);
  controller->ShowWithSelection(web_contents->GetPrimaryMainFrame(),
                                selection_bounds,
                                selection::InteractionOptions::New(),
                                ::selection::EntryPoint::kContextMenuImage);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));
  apc_agent.WaitForRequest();
  ASSERT_FALSE(controller->HasPageContextForTesting());

  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  static_cast<selection::SelectionOverlayPageHandler*>(controller)
      ->SubmitPrompt("Explain this selection");
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Prompt submitted before the page context.");

  controller->Close();
  ASSERT_EQ(controller->state(), SelectionOverlayController::State::kOff);
  histogram_tester.ExpectBucketCount(
      "SmartSelection.Submission.Outcome",
      ::selection::SubmissionOutcome::kPromptSubmitted, 0);
}

class SelectionOverlayTextSelectionBrowserTest : public GlicBrowserTest {
 public:
  SelectionOverlayTextSelectionBrowserTest() {
    // The inline cue widget is not needed to pre-select the text selection.
    scoped_feature_list_.InitWithFeatures(
        {::features::kGlicCaptureRegion,
         ::features::kGlicSelectionOverlayPrompt,
         ::features::kGlicSelectionSmallChip},
        {::features::kGlicSelectionPrompt});
  }
  ~SelectionOverlayTextSelectionBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Same as `SelectionOverlayTextSelectionBrowserTest`, but with the small chip
// disabled.
class SelectionOverlayTextSelectionNoSmallChipBrowserTest
    : public GlicBrowserTest {
 public:
  SelectionOverlayTextSelectionNoSmallChipBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {::features::kGlicCaptureRegion,
         ::features::kGlicSelectionOverlayPrompt},
        {::features::kGlicSelectionSmallChip,
         ::features::kGlicSelectionPrompt});
  }
  ~SelectionOverlayTextSelectionNoSmallChipBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

namespace {

// Starts a capture session the way the web client does. The observer's
// receiver is dropped because these tests only check browser state.
void StartCaptureRegion(tabs::TabInterface* tab) {
  mojo::PendingRemote<mojom::CaptureRegionObserver> observer;
  std::ignore = observer.InitWithNewPipeAndPassReceiver();
  SelectionOverlayController::CaptureRegion(
      tab,
      GlicKeyedService::Get(tab->GetProfile())
          ->active_instance_sharing_manager(),
      std::move(observer), /*options=*/nullptr);
}

// Selects all text on the page and waits for the renderer to report the
// selection bounds, which arrive asynchronously.
[[nodiscard]] bool SelectAllAndWaitForBounds(
    content::WebContents* web_contents) {
  web_contents->SelectAll();
  return base::test::RunUntil([&]() {
    std::optional<gfx::Rect> bounds = web_contents->GetTextSelectionBounds(
        web_contents->GetPrimaryMainFrame());
    return bounds.has_value() && !bounds->IsEmpty();
  });
}

}  // namespace

IN_PROC_BROWSER_TEST_F(SelectionOverlayTextSelectionBrowserTest,
                       CaptureRegionPreselectsTextSelection) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);
  auto* selection_observer = GlicSelectionObserver::From(tab);
  ASSERT_TRUE(selection_observer);

  ASSERT_TRUE(SelectAllAndWaitForBounds(web_contents));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return selection_observer->GetCurrentSelectionBounds().has_value();
  }));
  ASSERT_TRUE(OpenGlicForActiveTab().has_value());

  StartCaptureRegion(tab);

  EXPECT_EQ(controller->GetSelectedRegionCount(), 1u);

  controller->Close();
  EXPECT_EQ(controller->GetSelectedRegionCount(), 0u);
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayTextSelectionNoSmallChipBrowserTest,
                       CaptureRegionIgnoresSelectionWithoutSmallChip) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);

  // A text selection exists, but the small chip is disabled.
  ASSERT_TRUE(SelectAllAndWaitForBounds(web_contents));
  ASSERT_TRUE(OpenGlicForActiveTab().has_value());

  StartCaptureRegion(tab);

  EXPECT_EQ(controller->GetSelectedRegionCount(), 0u);

  controller->Close();
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayTextSelectionBrowserTest,
                       CaptureRegionIgnoresClearedSelection) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);
  auto* selection_observer = GlicSelectionObserver::From(tab);
  ASSERT_TRUE(selection_observer);

  // Select text so that the observer records the selected frame.
  ASSERT_TRUE(SelectAllAndWaitForBounds(web_contents));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return selection_observer->GetCurrentSelectionBounds().has_value();
  }));

  // Clear the selection. Starting a capture session afterwards must not reuse
  // the previous selection.
  ASSERT_TRUE(content::ExecJs(web_contents,
                              "window.getSelection().removeAllRanges();"));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !selection_observer->GetCurrentSelectionBounds().has_value();
  }));
  ASSERT_TRUE(OpenGlicForActiveTab().has_value());

  StartCaptureRegion(tab);

  EXPECT_EQ(controller->GetSelectedRegionCount(), 0u);

  controller->Close();
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayTextSelectionBrowserTest,
                       CaptureRegionIgnoredWhileTabModalShowing) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);
  auto* selection_observer = GlicSelectionObserver::From(tab);
  ASSERT_TRUE(selection_observer);

  ASSERT_TRUE(SelectAllAndWaitForBounds(web_contents));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return selection_observer->GetCurrentSelectionBounds().has_value();
  }));
  ASSERT_TRUE(OpenGlicForActiveTab().has_value());

  std::unique_ptr<tabs::ScopedTabModalUI> modal_ui = tab->ShowModalUI();
  StartCaptureRegion(tab);

  EXPECT_EQ(controller->state(), SelectionOverlayController::State::kOff);
  EXPECT_EQ(controller->GetSelectedRegionCount(), 0u);
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayPromptBrowserTest,
                       SuggestedActionsFromMockTool) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();

  auto* suggestion_service = ::selection::SuggestionService::From(tab);
  ASSERT_TRUE(suggestion_service);
  FakeSelectionSuggestionTool gemini_tool(
      tab, optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  FakeSelectionSuggestionTool lens_tool(
      tab, optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_LENS);
  ScopedToolRegistration gemini_registration(suggestion_service, &gemini_tool);
  ScopedToolRegistration lens_registration(suggestion_service, &lens_tool);
  AddServerSuggestionsForTesting(
      tab, {{optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME,
             "Explain"},
            {optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME,
             "Summarize"},
            {optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME,
             "Create Image"},
            {optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_LENS,
             "Translate to Spanish"},
            {optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_LENS,
             "Fact check"}});

  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  auto* handler =
      static_cast<selection::SelectionOverlayPageHandler*>(controller);
  handler->AdjustRegion(
      selection::SelectedRegion::New(
          base::UnguessableToken::Create(),
          selection::RegionShape::NewRect(gfx::RectF(0.5f, 0.5f, 0.2f, 0.2f))),
      /*is_using_keyboard=*/false);

  TestSuggestedActionsListener listener;
  controller->GetSuggestedActionsForTesting(listener.GetCallback());
  listener.WaitForBatches(1);
  const auto& actions = listener.actions();
  ASSERT_EQ(actions.size(), 5u);
  EXPECT_EQ(actions[0]->title, "Explain");
  EXPECT_EQ(actions[1]->title, "Summarize");
  EXPECT_EQ(actions[2]->title, "Create Image");
  EXPECT_FALSE(actions[3]->id.is_empty());
  EXPECT_EQ(actions[3]->title, "Translate to Spanish");
  EXPECT_FALSE(actions[4]->id.is_empty());
  EXPECT_EQ(actions[4]->title, "Fact check");
  EXPECT_NE(actions[3]->id, actions[4]->id);
}

namespace {

class CountingSelectionSuggestionTool : public ::selection::SuggestionTool {
 public:
  explicit CountingSelectionSuggestionTool(tabs::TabInterface* tab)
      : tab_(tab) {
    QueueNextResponse();
  }
  ~CountingSelectionSuggestionTool() override = default;

  ToolId GetToolId() const override {
    return optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME;
  }

  std::unique_ptr<::selection::Suggestion> CreateSuggestion(
      const ::selection::AreaOfInterest& processed_area,
      const optimization_guide::proto::SmartSelectionSuggestion&
          server_suggestion) override {
    request_count_++;
    last_aoi_screenshot_ = processed_area.screenshot;
    last_aoi_apc_ = processed_area.apc;
    last_selected_text_ = processed_area.selected_text;
    last_text_surrounding_selection_ =
        processed_area.text_surrounding_selection;
    if (std::holds_alternative<gfx::Rect>(processed_area.bounds)) {
      last_rect_ = std::get<gfx::Rect>(processed_area.bounds);
    }
    QueueNextResponse();
    std::u16string label = u"Action " + base::NumberToString16(request_count_);
    return std::make_unique<SelectionSuggestion>(*tab_, label, "Prompt");
  }

  int request_count() const { return request_count_; }
  const gfx::Rect& last_rect() const { return last_rect_; }
  const SkBitmap& last_aoi_screenshot() const { return last_aoi_screenshot_; }
  const optimization_guide::proto::AnnotatedPageContent& last_aoi_apc() const {
    return last_aoi_apc_;
  }
  const std::optional<std::u16string>& last_selected_text() const {
    return last_selected_text_;
  }
  const std::optional<std::u16string>& last_text_surrounding_selection() const {
    return last_text_surrounding_selection_;
  }

 private:
  void QueueNextResponse() {
    AddServerSuggestionsForTesting(
        tab_,
        {{optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME,
          "Action"}});
  }

  raw_ptr<tabs::TabInterface> tab_;
  int request_count_ = 0;
  gfx::Rect last_rect_;
  SkBitmap last_aoi_screenshot_;
  optimization_guide::proto::AnnotatedPageContent last_aoi_apc_;
  std::optional<std::u16string> last_selected_text_;
  std::optional<std::u16string> last_text_surrounding_selection_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SelectionOverlayPromptBrowserTest,
                       ShowWithSelectionPopulatesSurroundingText) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();

  auto* suggestion_service = ::selection::SuggestionService::From(tab);
  ASSERT_TRUE(suggestion_service);
  CountingSelectionSuggestionTool counting_tool(tab);
  ScopedToolRegistration registration(suggestion_service, &counting_tool);

  ASSERT_TRUE(content::ExecJs(web_contents, R"(
    document.body.innerText = 'Before text Selected target After text';
    const textNode = document.body.firstChild;
    const range = document.createRange();
    range.setStart(textNode, 12);
    range.setEnd(textNode, 27);
    const selection = window.getSelection();
    selection.removeAllRanges();
    selection.addRange(range);
  )"));

  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);

  gfx::Rect view_bounds = web_contents->GetViewBounds();
  gfx::Rect selection_bounds(view_bounds.x() + 10, view_bounds.y() + 10, 100,
                             50);
  controller->ShowWithSelection(web_contents->GetPrimaryMainFrame(),
                                selection_bounds,
                                selection::InteractionOptions::New(),
                                ::selection::EntryPoint::kContextMenuText);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  TestSuggestedActionsListener listener;
  controller->GetSuggestedActionsForTesting(listener.GetCallback());
  listener.WaitForBatches(1);

  EXPECT_EQ(counting_tool.request_count(), 1);
  EXPECT_EQ(counting_tool.last_selected_text(), u"Selected target");
  EXPECT_EQ(counting_tool.last_text_surrounding_selection(),
            u"Before text Selected target After text");
}

IN_PROC_BROWSER_TEST_F(
    SelectionOverlayPromptBrowserTest,
    ShowWithSelectionForImageSkipsPaddingAndSurroundingText) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();

  auto* suggestion_service = ::selection::SuggestionService::From(tab);
  ASSERT_TRUE(suggestion_service);
  CountingSelectionSuggestionTool counting_tool(tab);
  ScopedToolRegistration registration(suggestion_service, &counting_tool);

  ASSERT_TRUE(content::ExecJs(web_contents, R"(
    document.body.innerText = 'Before text Selected target After text';
    const textNode = document.body.firstChild;
    const range = document.createRange();
    range.setStart(textNode, 12);
    range.setEnd(textNode, 27);
    const selection = window.getSelection();
    selection.removeAllRanges();
    selection.addRange(range);
  )"));

  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);

  gfx::Rect view_bounds = web_contents->GetViewBounds();
  gfx::Rect selection_bounds(view_bounds.x() + 10, view_bounds.y() + 10, 100,
                             50);
  controller->ShowWithSelection(web_contents->GetPrimaryMainFrame(),
                                selection_bounds,
                                selection::InteractionOptions::New(),
                                ::selection::EntryPoint::kContextMenuImage);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  TestSuggestedActionsListener listener;
  controller->GetSuggestedActionsForTesting(listener.GetCallback());
  listener.WaitForBatches(1);

  EXPECT_EQ(counting_tool.request_count(), 1);
  EXPECT_FALSE(counting_tool.last_selected_text().has_value());
  EXPECT_FALSE(counting_tool.last_text_surrounding_selection().has_value());
  float scale = web_contents->GetRenderWidgetHostView()->GetDeviceScaleFactor();
  gfx::Rect expected_rect =
      gfx::ScaleToEnclosedRect(gfx::Rect(10, 10, 100, 50), scale);
  EXPECT_NEAR(counting_tool.last_rect().x(), expected_rect.x(), 1);
  EXPECT_NEAR(counting_tool.last_rect().y(), expected_rect.y(), 1);
  EXPECT_NEAR(counting_tool.last_rect().width(), expected_rect.width(), 2);
  EXPECT_NEAR(counting_tool.last_rect().height(), expected_rect.height(), 2);
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayPromptBrowserTest,
                       SuggestionsCachedPerRegionAndRefetchedOnAdjust) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();

  auto* suggestion_service = ::selection::SuggestionService::From(tab);
  ASSERT_TRUE(suggestion_service);
  CountingSelectionSuggestionTool counting_tool(tab);
  ScopedToolRegistration registration(suggestion_service, &counting_tool);

  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  auto* handler =
      static_cast<selection::SelectionOverlayPageHandler*>(controller);

  // 1. No active region yet: GetSuggestedActions returns empty without
  // fetching.
  {
    TestSuggestedActionsListener listener;
    controller->GetSuggestedActionsForTesting(listener.GetCallback());
    listener.WaitForBatches(1);
    EXPECT_TRUE(listener.actions().empty());
    EXPECT_EQ(counting_tool.request_count(), 0);
  }

  // 2. Select Region 1 and request suggestions -> fetches once ("Action 1").
  const auto region1_id = base::UnguessableToken::Create();
  const gfx::RectF region1_rect(0.2f, 0.2f, 0.2f, 0.2f);
  handler->AdjustRegion(
      selection::SelectedRegion::New(
          region1_id, selection::RegionShape::NewRect(region1_rect)),
      /*is_using_keyboard=*/false);
  base::UnguessableToken region1_action_id;
  {
    TestSuggestedActionsListener listener;
    controller->GetSuggestedActionsForTesting(listener.GetCallback());
    listener.WaitForBatches(1);
    ASSERT_EQ(listener.actions().size(), 1u);
    EXPECT_EQ(listener.actions()[0]->title, "Action 1");
    region1_action_id = listener.actions()[0]->id;
    EXPECT_EQ(counting_tool.request_count(), 1);
    EXPECT_FALSE(counting_tool.last_aoi_screenshot().empty());
    EXPECT_TRUE(counting_tool.last_aoi_apc().has_main_frame_data());
  }

  // 3. Re-request for Region 1 without changing bounds -> returns stored
  // suggestions without refetching.
  {
    TestSuggestedActionsListener listener;
    controller->GetSuggestedActionsForTesting(listener.GetCallback());
    listener.WaitForBatches(1);
    ASSERT_EQ(listener.actions().size(), 1u);
    EXPECT_EQ(listener.actions()[0]->title, "Action 1");
    EXPECT_EQ(listener.actions()[0]->id, region1_action_id);
    EXPECT_EQ(counting_tool.request_count(), 1);
  }

  // 4. Select Region 2 -> Region 2 becomes active; requesting suggestions
  // fetches for Region 2 ("Action 2").
  const auto region2_id = base::UnguessableToken::Create();
  const gfx::RectF region2_rect(0.5f, 0.5f, 0.2f, 0.2f);
  handler->AdjustRegion(
      selection::SelectedRegion::New(
          region2_id, selection::RegionShape::NewRect(region2_rect)),
      /*is_using_keyboard=*/false);
  {
    TestSuggestedActionsListener listener;
    controller->GetSuggestedActionsForTesting(listener.GetCallback());
    listener.WaitForBatches(1);
    ASSERT_EQ(listener.actions().size(), 1u);
    EXPECT_EQ(listener.actions()[0]->title, "Action 2");
    EXPECT_EQ(counting_tool.request_count(), 2);
  }

  // 5. Delete Region 2 so Region 1 becomes active again -> returns stored
  // "Action 1" without refetching.
  handler->DeleteRegion(region2_id, /*is_using_keyboard=*/false);
  {
    TestSuggestedActionsListener listener;
    controller->GetSuggestedActionsForTesting(listener.GetCallback());
    listener.WaitForBatches(1);
    ASSERT_EQ(listener.actions().size(), 1u);
    EXPECT_EQ(listener.actions()[0]->title, "Action 1");
    EXPECT_EQ(listener.actions()[0]->id, region1_action_id);
    EXPECT_EQ(counting_tool.request_count(), 2);
  }

  // 6. Adjust Region 1's bounds -> invalidates stored suggestions and refetches
  // ("Action 3").
  const gfx::RectF region1_adjusted_rect(0.3f, 0.3f, 0.2f, 0.2f);
  handler->AdjustRegion(
      selection::SelectedRegion::New(
          region1_id, selection::RegionShape::NewRect(region1_adjusted_rect)),
      /*is_using_keyboard=*/false);
  {
    TestSuggestedActionsListener listener;
    controller->GetSuggestedActionsForTesting(listener.GetCallback());
    listener.WaitForBatches(1);
    ASSERT_EQ(listener.actions().size(), 1u);
    EXPECT_EQ(listener.actions()[0]->title, "Action 3");
    EXPECT_NE(listener.actions()[0]->id, region1_action_id);
    EXPECT_EQ(counting_tool.request_count(), 3);
  }
}

namespace {

class FakeSuggestionService : public ::selection::SuggestionService {
 public:
  explicit FakeSuggestionService(tabs::TabInterface* tab)
      : ::selection::SuggestionService(tab,
                                       /*remote_model_executor=*/nullptr,
                                       /*google_groups_manager=*/nullptr) {}
  ~FakeSuggestionService() override = default;

  void RequestSuggestions(::selection::AreaOfInterest processed_area,
                          ::selection::SuggestionsCallback callback) override {
    pending_callbacks_.push_back(std::move(callback));
  }

  size_t pending_request_count() const { return pending_callbacks_.size(); }

  void CompleteRequest(
      size_t index,
      std::vector<std::unique_ptr<::selection::Suggestion>> suggestions) {
    std::move(pending_callbacks_[index])
        .Run(std::move(suggestions), ::selection::FeedbackZipBuilder());
  }

 private:
  std::vector<::selection::SuggestionsCallback> pending_callbacks_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SelectionOverlayPromptBrowserTest,
                       SuggestionsShownMetricNotRecordedForInactiveRegion) {
  base::HistogramTester histogram_tester;
  FakeSuggestionService* fake_service = nullptr;
  ui::UserDataFactory::ScopedOverride override_subscription =
      tabs::TabFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
          base::BindLambdaForTesting([&](tabs::TabInterface& tab) {
            auto service = std::make_unique<FakeSuggestionService>(&tab);
            fake_service = service.get();
            return service;
          }));

  tabs::TabInterface* const tab = CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_TRUE(fake_service);

  SelectionOverlayController* const controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  selection::SelectionOverlayPageHandler* const handler =
      static_cast<selection::SelectionOverlayPageHandler*>(controller);

  // Select Region 1 and wait for the overlay WebUI to request suggestions;
  // FakeSuggestionService holds the request in flight.
  const base::UnguessableToken region1_id = base::UnguessableToken::Create();
  handler->AdjustRegion(
      selection::SelectedRegion::New(
          region1_id,
          selection::RegionShape::NewRect(gfx::RectF(0.2f, 0.2f, 0.2f, 0.2f))),
      /*is_using_keyboard=*/false);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_service->pending_request_count() == 1u; }));

  // Select a different Region 2 before Region 1's suggestions arrive.
  const base::UnguessableToken region2_id = base::UnguessableToken::Create();
  handler->AdjustRegion(
      selection::SelectedRegion::New(
          region2_id,
          selection::RegionShape::NewRect(gfx::RectF(0.5f, 0.5f, 0.2f, 0.2f))),
      /*is_using_keyboard=*/false);

  // Now Region 1's suggestions arrive while Region 2 is active.
  std::vector<std::unique_ptr<::selection::Suggestion>> suggestions;
  suggestions.push_back(
      std::make_unique<SelectionSuggestion>(*tab, u"Action 1", "Prompt"));
  fake_service->CompleteRequest(0, std::move(suggestions));

  histogram_tester.ExpectTotalCount(
      "SmartSelection.Suggestions.TotalShownCount", 0);
  histogram_tester.ExpectTotalCount(
      "SmartSelection.Suggestions.GeminiInChromeShownCount", 0);
}

namespace {

class FakePromptSuggestionTool : public ::selection::SuggestionTool {
 public:
  explicit FakePromptSuggestionTool(tabs::TabInterface* tab) : tab_(tab) {
    AddServerSuggestionsForTesting(
        tab_,
        {{optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME,
          "Explain"}});
  }
  ~FakePromptSuggestionTool() override = default;

  ToolId GetToolId() const override {
    return optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME;
  }

  std::unique_ptr<::selection::Suggestion> CreateSuggestion(
      const ::selection::AreaOfInterest& processed_area,
      const optimization_guide::proto::SmartSelectionSuggestion&
          server_suggestion) override {
    return std::make_unique<SelectionSuggestion>(
        *tab_, u"Explain", "Explain the selection in a few sentences.");
  }

 private:
  raw_ptr<tabs::TabInterface> tab_;
};

class InlineSuggestion : public ::selection::Suggestion,
                         public ::test::mojom::Echo {
 public:
  InlineSuggestion() {
    SetInterface<::test::mojom::Echo>(base::BindRepeating(
        &InlineSuggestion::BindEcho, base::Unretained(this)));
  }
  ~InlineSuggestion() override = default;

  // ::selection::Suggestion:
  const std::u16string& GetLabel() const override { return label_; }
  ToolId GetToolId() const override {
    return optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME;
  }
  void OnSuggestionPresented() override {}
  void OnSuggestionExecuted() override {}
  ::selection::mojom::ActionPtr GetAction() const override {
    return ::selection::mojom::Action::NewInlineFulfillment(
        ::selection::mojom::InlineFulfillment::New("does_not_matter.js"));
  }

  // ::test::mojom::Echo:
  void EchoString(const std::string& input,
                  EchoStringCallback callback) override {
    std::move(callback).Run(input);
  }

  bool channel_bound() const { return receiver_.is_bound(); }

 private:
  void BindEcho(mojo::PendingAssociatedReceiver<::test::mojom::Echo> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  std::u16string label_ = u"InlineSuggestion";
  mojo::AssociatedReceiver<::test::mojom::Echo> receiver_{this};
};

class FakeInlineSuggestionTool : public ::selection::SuggestionTool {
 public:
  explicit FakeInlineSuggestionTool(tabs::TabInterface* tab) {
    AddServerSuggestionsForTesting(
        tab, {{optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME,
               "InlineSuggestion"}});
  }
  ~FakeInlineSuggestionTool() override = default;

  ToolId GetToolId() const override {
    return optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME;
  }

  std::unique_ptr<::selection::Suggestion> CreateSuggestion(
      const ::selection::AreaOfInterest& processed_area,
      const optimization_guide::proto::SmartSelectionSuggestion&
          server_suggestion) override {
    auto suggestion = std::make_unique<InlineSuggestion>();
    last_suggestion_ = suggestion.get();
    return suggestion;
  }

  InlineSuggestion* last_suggestion() { return last_suggestion_; }

 private:
  raw_ptr<InlineSuggestion> last_suggestion_ = nullptr;
};

::selection::mojom::ActionPtr GetActionFromRegion(
    SelectionOverlayController* controller,
    base::UnguessableToken* action_id) {
  auto* handler =
      static_cast<selection::SelectionOverlayPageHandler*>(controller);
  handler->AdjustRegion(
      selection::SelectedRegion::New(
          base::UnguessableToken::Create(),
          selection::RegionShape::NewRect(gfx::RectF(0.5f, 0.5f, 0.2f, 0.2f))),
      /*is_using_keyboard=*/false);

  TestSuggestedActionsListener listener;
  controller->GetSuggestedActionsForTesting(listener.GetCallback());
  listener.WaitForBatches(1);
  if (listener.actions().empty()) {
    return nullptr;
  }
  *action_id = listener.actions()[0]->id;
  return listener.actions()[0]->action.Clone();
}

}  // namespace

IN_PROC_BROWSER_TEST_F(SelectionOverlayPromptBrowserTest,
                       HandoffDismissesOverlay) {
  // The panel stays closed: a page session started with the panel open is
  // handed to the panel and then outlives a hand-off.
  base::HistogramTester histogram_tester;
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());

  auto* suggestion_service = ::selection::SuggestionService::From(tab);
  ASSERT_TRUE(suggestion_service);
  FakePromptSuggestionTool tool(tab);
  ScopedToolRegistration registration(suggestion_service, &tool);

  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  base::UnguessableToken action_id;
  ::selection::mojom::ActionPtr action =
      GetActionFromRegion(controller, &action_id);
  ASSERT_TRUE(action);
  EXPECT_TRUE(action->is_handoff());

  static_cast<selection::SelectionOverlayPageHandler*>(controller)
      ->ExecuteSuggestedAction(action_id,
                               mojo::GenericPendingAssociatedReceiver());
  EXPECT_EQ(controller->GetSelectedRegionCount(), 0u);
  histogram_tester.ExpectUniqueSample(
      "SmartSelection.Selection.Outcome",
      ::selection::SelectionOutcome::kSpatialSelectionCompleted, 1);
  histogram_tester.ExpectUniqueSample(
      "SmartSelection.Suggestions.TotalShownCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "SmartSelection.Suggestions.GeminiInChromeShownCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "SmartSelection.Suggestions.AcceptedSuggestionType",
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME, 1);
  histogram_tester.ExpectUniqueSample(
      "SmartSelection.Submission.Outcome",
      ::selection::SubmissionOutcome::kSuggestionAccepted, 1);
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayPromptBrowserTest,
                       InlineFulfillmentBindsChannel) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());

  auto* suggestion_service = ::selection::SuggestionService::From(tab);
  ASSERT_TRUE(suggestion_service);
  FakeInlineSuggestionTool tool(tab);
  ScopedToolRegistration registration(suggestion_service, &tool);

  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  base::UnguessableToken action_id;
  ::selection::mojom::ActionPtr action =
      GetActionFromRegion(controller, &action_id);
  ASSERT_TRUE(action);
  ASSERT_TRUE(action->is_inline_fulfillment());
  EXPECT_EQ(action->get_inline_fulfillment()->resource_name,
            "does_not_matter.js");

  mojo::AssociatedRemote<::test::mojom::Echo> channel;
  static_cast<selection::SelectionOverlayPageHandler*>(controller)
      ->ExecuteSuggestedAction(
          action_id, mojo::GenericPendingAssociatedReceiver(
                         channel.BindNewEndpointAndPassDedicatedReceiver()));
  EXPECT_EQ(controller->state(), SelectionOverlayController::State::kOverlay);
  EXPECT_EQ(controller->GetSelectedRegionCount(), 1u);

  ASSERT_TRUE(tool.last_suggestion());
  EXPECT_TRUE(tool.last_suggestion()->channel_bound());

  // The endpoint name matched what the suggestion registered, so real calls
  // work.
  base::test::TestFuture<const std::string&> echoed;
  channel->EchoString("hello", echoed.GetCallback());
  EXPECT_EQ(echoed.Get(), "hello");
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayPromptBrowserTest,
                       InlineFulfillmentRejectsMismatchedInterface) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());

  auto* suggestion_service = ::selection::SuggestionService::From(tab);
  ASSERT_TRUE(suggestion_service);
  FakeInlineSuggestionTool tool(tab);
  ScopedToolRegistration registration(suggestion_service, &tool);

  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  base::UnguessableToken action_id;
  ASSERT_TRUE(GetActionFromRegion(controller, &action_id));

  mojo::AssociatedRemote<::test::mojom::Echo> channel;
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  static_cast<selection::SelectionOverlayPageHandler*>(controller)
      ->ExecuteSuggestedAction(
          action_id,
          mojo::GenericPendingAssociatedReceiver(
              "test.mojom.NotEcho",
              channel.BindNewEndpointAndPassDedicatedReceiver().PassHandle()));
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Channel interface does not match.");

  ASSERT_TRUE(tool.last_suggestion());
  EXPECT_FALSE(tool.last_suggestion()->channel_bound());
}

namespace {

void Connect(ExplainSuggestion& suggestion,
             mojo::AssociatedRemote<selection::ExplainFulfillment>& remote) {
  suggestion.Execute(mojo::GenericPendingAssociatedReceiver(
      remote.BindNewEndpointAndPassDedicatedReceiver()));
}

}  // namespace

using ExplainSuggestionBrowserTest = SelectionOverlayBrowserTest;

IN_PROC_BROWSER_TEST_F(ExplainSuggestionBrowserTest, GetExplanation) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  static constexpr char kExpectedExplanation[] =
      "An explanation of your selection.";
  optimization_guide::proto::QuickAnswersResponse response_msg;
  response_msg.set_answer(kExpectedExplanation);
  optimization_guide::proto::Any any;
  any.set_value(response_msg.SerializeAsString());
  any.set_type_url(
      base::StrCat({"type.googleapis.com/", response_msg.GetTypeName()}));
  OptimizationGuideKeyedServiceFactory::GetForProfile(tab->GetProfile())
      ->AddExecutionResultForTesting(
          optimization_guide::ModelBasedCapabilityKey::kQuickAnswers,
          optimization_guide::OptimizationGuideModelExecutionResult(
              std::move(any), nullptr));
  tab->GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicQuickAnswersFreAccepted,
                                            true);

  ExplainSuggestion suggestion(*tab, /*request=*/{});
  mojo::AssociatedRemote<selection::ExplainFulfillment> remote;
  Connect(suggestion, remote);

  base::test::TestFuture<const std::string&> explanation;
  remote->GetExplanation(explanation.GetCallback());
  EXPECT_EQ(explanation.Get(), kExpectedExplanation);
}

namespace {

// Makes `search_url` the default search engine.
void SetTestDefaultSearchEngine(Profile* profile, const GURL& search_url) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
  TemplateURLData data;
  data.SetShortName(u"Test");
  data.SetKeyword(u"test");
  data.SetURL(search_url.spec());
  template_url_service->SetUserSelectedDefaultSearchProvider(
      template_url_service->Add(std::make_unique<TemplateURL>(data)));
}

// Shows the overlay on `tab` and returns the overlay WebUI's main frame.
content::RenderFrameHost* ShowOverlay(tabs::TabInterface* tab) {
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  CHECK(controller);
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
  EXPECT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));
  return controller->GetOverlayMainFrame();
}

}  // namespace

IN_PROC_BROWSER_TEST_F(ExplainSuggestionBrowserTest,
                       OpenTabForSearchUsesDefaultSearchEngine) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  SetTestDefaultSearchEngine(tab->GetProfile(),
                             GetTestUrl("page.html?q={searchTerms}"));
  content::RenderFrameHost* overlay = ShowOverlay(tab);
  ASSERT_TRUE(overlay);

  ExplainSuggestion suggestion(*tab, /*request=*/{});
  mojo::AssociatedRemote<selection::ExplainFulfillment> remote;
  Connect(suggestion, remote);
  auto* tabs = GetTabListInterface();
  const int tab_count = tabs->GetTabCount();

  // Gives the overlay a user activation, like the click on the card.
  ASSERT_TRUE(content::ExecJs(overlay, "true"));
  remote->OpenTabForSearch("hello");
  remote.FlushForTesting();

  // The search opens in a new foreground tab.
  EXPECT_EQ(tabs->GetTabCount(), tab_count + 1);
  EXPECT_EQ(tabs->GetActiveTab()->GetContents()->GetVisibleURL(),
            GetTestUrl("page.html?q=hello"));
}

IN_PROC_BROWSER_TEST_F(ExplainSuggestionBrowserTest,
                       OpenTabForSearchNeedsUserActivation) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  SetTestDefaultSearchEngine(tab->GetProfile(),
                             GetTestUrl("page.html?q={searchTerms}"));
  ASSERT_TRUE(ShowOverlay(tab));

  ExplainSuggestion suggestion(*tab, /*request=*/{});
  mojo::AssociatedRemote<selection::ExplainFulfillment> remote;
  Connect(suggestion, remote);
  auto* tabs = GetTabListInterface();
  const int tab_count = tabs->GetTabCount();

  remote->OpenTabForSearch("hello");
  remote.FlushForTesting();

  EXPECT_EQ(tabs->GetTabCount(), tab_count);
}

IN_PROC_BROWSER_TEST_F(ExplainSuggestionBrowserTest,
                       AcceptingFreRecordsConsentAndUnblocksExplanation) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_TRUE(ShowOverlay(tab));
  PrefService* prefs = tab->GetProfile()->GetPrefs();
  ASSERT_FALSE(prefs->GetBoolean(prefs::kGlicQuickAnswersFreAccepted));
  static constexpr char kExpectedExplanation[] = "An explanation.";
  optimization_guide::proto::QuickAnswersResponse response_msg;
  response_msg.set_answer(kExpectedExplanation);
  optimization_guide::proto::Any any;
  any.set_value(response_msg.SerializeAsString());
  any.set_type_url(
      base::StrCat({"type.googleapis.com/", response_msg.GetTypeName()}));
  OptimizationGuideKeyedServiceFactory::GetForProfile(tab->GetProfile())
      ->AddExecutionResultForTesting(
          optimization_guide::ModelBasedCapabilityKey::kQuickAnswers,
          optimization_guide::OptimizationGuideModelExecutionResult(
              std::move(any), nullptr));

  ExplainSuggestion suggestion(*tab, /*request=*/{});
  mojo::AssociatedRemote<selection::ExplainFulfillment> remote;
  Connect(suggestion, remote);

  // Until the FRE is accepted, the model isn't asked.
  base::test::TestFuture<const std::string&> explanation;
  remote->GetExplanation(explanation.GetCallback());
  EXPECT_EQ(explanation.Take(),
            l10n_util::GetStringUTF8(IDS_GLIC_ERROR_NOTICE));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       kQuickAnswersFreDialogName);
  base::test::TestFuture<bool> consent;
  remote->RequestConsent(consent.GetCallback());
  views::Widget* dialog = waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(dialog);
  EXPECT_EQ(dialog->widget_delegate()->GetModalType(),
            ui::mojom::ModalType::kWindow);
  EXPECT_FALSE(consent.IsReady());

  dialog->widget_delegate()->AsDialogDelegate()->AcceptDialog();
  EXPECT_TRUE(consent.Take());
  EXPECT_TRUE(prefs->GetBoolean(prefs::kGlicQuickAnswersFreAccepted));

  remote->GetExplanation(explanation.GetCallback());
  EXPECT_EQ(explanation.Take(), kExpectedExplanation);

  // Once accepted, consent is immediate.
  remote->RequestConsent(consent.GetCallback());
  EXPECT_TRUE(consent.Take());
}

IN_PROC_BROWSER_TEST_F(ExplainSuggestionBrowserTest, DecliningFreKeepsOverlay) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_TRUE(ShowOverlay(tab));
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());

  ExplainSuggestion suggestion(*tab, /*request=*/{});
  mojo::AssociatedRemote<selection::ExplainFulfillment> remote;
  Connect(suggestion, remote);

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       kQuickAnswersFreDialogName);
  base::test::TestFuture<bool> consent;
  remote->RequestConsent(consent.GetCallback());
  views::Widget* dialog = waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(dialog);

  dialog->widget_delegate()->AsDialogDelegate()->CancelDialog();
  EXPECT_FALSE(consent.Take());
  EXPECT_FALSE(tab->GetProfile()->GetPrefs()->GetBoolean(
      prefs::kGlicQuickAnswersFreAccepted));
  EXPECT_EQ(controller->state(), SelectionOverlayController::State::kOverlay);
}

IN_PROC_BROWSER_TEST_F(ExplainSuggestionBrowserTest, AskGeminiOpensGlic) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_TRUE(ShowOverlay(tab));
  ExplainSuggestion suggestion(*tab, /*request=*/{});
  mojo::AssociatedRemote<selection::ExplainFulfillment> remote;
  Connect(suggestion, remote);

  remote->AskGemini();
  EXPECT_TRUE(WaitForGlicOpen(tab).has_value());
  EXPECT_EQ(SelectionOverlayController::FromTabWebContents(tab->GetContents())
                ->state(),
            SelectionOverlayController::State::kOff);
}

class SelectionOverlayDogfoodFeedbackBrowserTest
    : public SelectionOverlayBrowserTest {
 public:
  SelectionOverlayDogfoodFeedbackBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicSelectionOverlayDogfoodFeedback,
        {{features::kGlicSelectionOverlayDogfoodFeedbackUrl.name,
          "https://example.com/feedback"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SelectionOverlayDogfoodFeedbackBrowserTest,
                       OpenDogfoodFeedbackOpensUrlInNewTab) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  DownloadPrefs::FromBrowserContext(tab->GetProfile())
      ->SetDownloadPath(temp_dir.GetPath());
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  auto* tabs = GetTabListInterface();
  const int initial_tab_count = tabs->GetTabCount();

  static_cast<selection::SelectionOverlayPageHandler*>(controller)
      ->OpenDogfoodFeedback();

  EXPECT_EQ(controller->state(), SelectionOverlayController::State::kOff);
  ASSERT_OK(RunUntilEqual([&]() { return tabs->GetTabCount(); },
                          initial_tab_count + 1,
                          "Timeout waiting for feedback tab to open"));
  EXPECT_EQ(tabs->GetActiveTab()->GetContents()->GetVisibleURL(),
            GURL("https://example.com/feedback"));
  EXPECT_TRUE(base::PathExists(
      temp_dir.GetPath().AppendASCII("smart_selection_feedback.zip")));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayBrowserTest,
                       OpenDogfoodFeedbackIgnoredWhenDisabled) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  auto* tabs = GetTabListInterface();
  const int initial_tab_count = tabs->GetTabCount();

  static_cast<selection::SelectionOverlayPageHandler*>(controller)
      ->OpenDogfoodFeedback();

  EXPECT_EQ(controller->state(), SelectionOverlayController::State::kOverlay);
  EXPECT_EQ(tabs->GetTabCount(), initial_tab_count);
}

// Backgrounding the tab while the overlay WebUI is starting must not crash
// when the WebUI binds, and the overlay initialized in the background (with its
// blur) must show again when the tab comes back.
IN_PROC_BROWSER_TEST_F(SelectionOverlayBrowserTest,
                       BackgroundedWhileStartingWebUI) {
  tabs::TabInterface* other_tab = CreateAndActivateTab(GetSimpleTestUrl());
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);

  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
  ASSERT_OK(RunUntilEqual([&]() { return controller->state(); },
                          SelectionOverlayController::State::kStartingWebUI,
                          "Timeout waiting for kStartingWebUI"));

  // The WebUI binds and initializes while the tab is in the background.
  GetTabListInterface()->ActivateTab(other_tab->GetHandle());
  ASSERT_OK(RunUntilEqual([&]() { return controller->state(); },
                          SelectionOverlayController::State::kBackground,
                          "Timeout waiting for kBackground"));
  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->backgrounded_state_for_testing(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for the overlay to initialize in the background"));

  GetTabListInterface()->ActivateTab(tab->GetHandle());
  ASSERT_OK(RunUntilEqual([&]() { return controller->state(); },
                          SelectionOverlayController::State::kOverlay,
                          "Timeout waiting for kOverlay"));
  ASSERT_TRUE(controller->GetOverlayViewForTesting());
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());
  EXPECT_TRUE(controller->GetLensOverlayBlurLayerDelegateForTesting());
}

// The page handler factory serves one request per page; another request is a
// bad message instead of a second binding.
IN_PROC_BROWSER_TEST_F(SelectionOverlayBrowserTest,
                       SecondCreatePageHandlerIsBadMessage) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
  ASSERT_OK(RunUntilEqual([controller]() { return controller->state(); },
                          SelectionOverlayController::State::kOverlay,
                          "Timeout waiting for kOverlay"));
  auto* untrusted_ui = controller->GetOverlayWebViewForTesting()
                           ->GetWebContents()
                           ->GetWebUI()
                           ->GetController()
                           ->GetAs<SelectionOverlayUntrustedUI>();
  ASSERT_TRUE(untrusted_ui);

  mojo::Remote<selection::SelectionOverlayPageHandlerFactory> factory;
  untrusted_ui->BindInterface(factory.BindNewPipeAndPassReceiver());
  mojo::Remote<selection::SelectionOverlayPageHandler> handler;
  mojo::PendingReceiver<selection::SelectionOverlayPage> page;
  mojo::test::BadMessageObserver bad_message_observer;
  factory->CreatePageHandler(handler.BindNewPipeAndPassReceiver(),
                             page.InitWithNewPipeAndPassRemote());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "CreatePageHandler called more than once");
}

// A handler request that can't bind and isn't from the prewarmed overlay is a
// bad message instead of being held. Reloading the overlay mid-session makes a
// new page ask for a handler while the session can't take it.
IN_PROC_BROWSER_TEST_F(SelectionOverlayBrowserTest,
                       UnexpectedCreatePageHandlerIsBadMessage) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
  ASSERT_OK(RunUntilEqual([controller]() { return controller->state(); },
                          SelectionOverlayController::State::kOverlay,
                          "Timeout waiting for kOverlay"));

  content::WebContents* overlay_contents =
      controller->GetOverlayWebViewForTesting()->GetWebContents();
  content::RenderProcessHostBadMojoMessageWaiter bad_message_waiter(
      overlay_contents->GetPrimaryMainFrame()->GetProcess());
  overlay_contents->GetController().Reload(content::ReloadType::NORMAL,
                                           /*check_for_repost=*/false);
  EXPECT_EQ(bad_message_waiter.Wait(),
            "Received bad user message: CreatePageHandler called when the "
            "overlay cannot bind");
}

class SelectionOverlayPrewarmBrowserTest : public GlicBrowserTest {
 public:
  SelectionOverlayPrewarmBrowserTest() {
    scoped_feature_list_.InitFromCommandLine(
        "GlicCaptureRegion,GlicSelectionOverlayPrompt,"
        "StaticSelectionSuggestions,GlicSelectionOverlayHoverPrewarm",
        "");
  }

 protected:
  // Prewarms the overlay and returns its WebContents, caught as it is created.
  base::WeakPtr<content::WebContents> Prewarm(
      SelectionOverlayController* controller,
      const selection::InteractionOptionsPtr& options) {
    base::WeakPtr<content::WebContents> prewarmed;
    base::CallbackListSubscription subscription =
        content::RegisterWebContentsCreationCallback(base::BindLambdaForTesting(
            [&prewarmed](content::WebContents* contents) {
              prewarmed = contents->GetWeakPtr();
            }));
    controller->MaybePrewarmOverlay(options.Clone());
    return prewarmed;
  }

  // Prewarms the overlay and waits until its page is loaded.
  base::WeakPtr<content::WebContents> PrewarmAndWait(
      SelectionOverlayController* controller,
      const selection::InteractionOptionsPtr& options) {
    base::WeakPtr<content::WebContents> contents = Prewarm(controller, options);
    if (!contents || !content::WaitForLoadStop(contents.get())) {
      return nullptr;
    }
    return contents;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SelectionOverlayPrewarmBrowserTest,
                       AdoptsPrewarmedOverlayAndCancelsOnTabSwitch) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  auto options = selection::InteractionOptions::New(
      /*hide_handles=*/true, /*skip_intro_animation=*/true);

  // Switching tabs while idle drops the unused prewarm.
  controller->MaybePrewarmOverlay(options.Clone());
  ASSERT_TRUE(controller->GetPrewarmInteractionOptions());
  CreateAndActivateTab(GetSimpleTestUrl());
  EXPECT_FALSE(controller->GetPrewarmInteractionOptions());

  GetTabListInterface()->ActivateTab(tab->GetHandle());
  auto* eligibility_helper = tabs::PageContextEligibilityHelper::From(tab);
  ASSERT_TRUE(eligibility_helper);
  ASSERT_TRUE(base::test::RunUntil([eligibility_helper]() {
    return eligibility_helper->IsPageContextEligible() ==
           optimization_guide::PageContextEligibilityStatus::kEligible;
  }));
  base::WeakPtr<content::WebContents> prewarmed =
      PrewarmAndWait(controller, options);
  ASSERT_TRUE(prewarmed);
  // The prewarmed page's handler request waits for a session.
  auto* untrusted_ui = prewarmed->GetWebUI()
                           ->GetController()
                           ->GetAs<SelectionOverlayUntrustedUI>();
  ASSERT_TRUE(untrusted_ui);
  ASSERT_TRUE(base::test::RunUntil([untrusted_ui]() {
    return untrusted_ui->HasPendingPageHandlerForTesting();
  }));

  gfx::Rect bounds = tab->GetContents()->GetViewBounds();
  bounds.Inset(20);
  controller->ShowWithSelection(tab->GetContents()->GetPrimaryMainFrame(),
                                bounds, std::move(options),
                                ::selection::EntryPoint::kInlineCue);
  // The session shows the prewarmed WebContents instead of a new one.
  EXPECT_FALSE(controller->GetPrewarmInteractionOptions());
  ASSERT_TRUE(prewarmed);
  ASSERT_EQ(controller->GetOverlayWebViewForTesting()->GetWebContents(),
            prewarmed.get());

  // Like the adopted page, ask for suggestions before the redacted screenshot
  // is here. They must wait for it instead of being requested early.
  TestSuggestedActionsListener listener;
  controller->GetSuggestedActionsForTesting(listener.GetCallback());
  ASSERT_TRUE(base::test::RunUntil(
      [&listener]() { return !listener.actions().empty(); }));
  EXPECT_EQ(listener.actions()[0]->title, "Ask Gemini");

  // The adopted page binds its waiting handler and the overlay initializes.
  ASSERT_OK(
      RunUntilEqual([controller]() { return controller->state(); },
                    SelectionOverlayController::State::kOverlay,
                    "Timeout waiting for adopted overlay to reach kOverlay"));
  EXPECT_FALSE(untrusted_ui->HasPendingPageHandlerForTesting());
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());
}

// The adopted page can bind while its tab is in the background; the overlay
// must show when the tab comes back.
IN_PROC_BROWSER_TEST_F(SelectionOverlayPrewarmBrowserTest,
                       ShowsAdoptedOverlayAfterTabSwitch) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  auto options = selection::InteractionOptions::New(
      /*hide_handles=*/true, /*skip_intro_animation=*/true);
  base::WeakPtr<content::WebContents> prewarmed =
      PrewarmAndWait(controller, options);
  ASSERT_TRUE(prewarmed);

  gfx::Rect bounds = tab->GetContents()->GetViewBounds();
  bounds.Inset(20);
  controller->ShowWithSelection(tab->GetContents()->GetPrimaryMainFrame(),
                                bounds, std::move(options),
                                ::selection::EntryPoint::kInlineCue);
  ASSERT_EQ(controller->GetOverlayWebViewForTesting()->GetWebContents(),
            prewarmed.get());

  CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_EQ(controller->state(),
            SelectionOverlayController::State::kBackground);

  GetTabListInterface()->ActivateTab(tab->GetHandle());
  ASSERT_OK(RunUntilEqual([controller]() { return controller->state(); },
                          SelectionOverlayController::State::kOverlay,
                          "Timeout waiting for overlay to return to kOverlay"));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());
  // Initialization that landed while backgrounded still set up the blur.
  EXPECT_TRUE(controller->GetLensOverlayBlurLayerDelegateForTesting());
}

// Esc closes an adopted overlay that is shown while the screenshot is still
// being captured.
IN_PROC_BROWSER_TEST_F(SelectionOverlayPrewarmBrowserTest,
                       EscClosesAdoptedOverlayWhileStarting) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  auto options = selection::InteractionOptions::New(
      /*hide_handles=*/true, /*skip_intro_animation=*/true);
  base::WeakPtr<content::WebContents> prewarmed =
      PrewarmAndWait(controller, options);
  ASSERT_TRUE(prewarmed);
  std::optional<SelectionOverlayController::CloseReason> close_reason;
  base::CallbackListSubscription closed_subscription =
      controller->RegisterOverlayClosedCallback(base::BindLambdaForTesting(
          [&close_reason](SelectionOverlayController::CloseReason reason) {
            close_reason = reason;
          }));

  gfx::Rect bounds = tab->GetContents()->GetViewBounds();
  bounds.Inset(20);
  controller->ShowWithSelection(tab->GetContents()->GetPrimaryMainFrame(),
                                bounds, std::move(options),
                                ::selection::EntryPoint::kInlineCue);
  ASSERT_EQ(controller->state(),
            SelectionOverlayController::State::kStartingWebUI);

  // The overlay handles Esc before its renderer sees the key.
  input::NativeWebKeyboardEvent escape(blink::WebInputEvent::Type::kRawKeyDown,
                                       blink::WebInputEvent::kNoModifiers,
                                       base::TimeTicks::Now());
  escape.windows_key_code = ui::VKEY_ESCAPE;
  EXPECT_EQ(
      prewarmed->GetDelegate()->PreHandleKeyboardEvent(prewarmed.get(), escape),
      content::KeyboardEventProcessingResult::HANDLED);
  EXPECT_EQ(controller->state(), SelectionOverlayController::State::kOff);
  EXPECT_EQ(close_reason,
            SelectionOverlayController::CloseReason::kEscapeKeyPress);
}

// Closing drops an unused prewarm too, e.g. when the session fails to start.
IN_PROC_BROWSER_TEST_F(SelectionOverlayPrewarmBrowserTest, CloseDropsPrewarm) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  controller->MaybePrewarmOverlay(selection::InteractionOptions::New(
      /*hide_handles=*/true, /*skip_intro_animation=*/true));
  ASSERT_TRUE(controller->GetPrewarmInteractionOptions());

  controller->Close();
  EXPECT_FALSE(controller->GetPrewarmInteractionOptions());
}

// A prewarm for other interaction options is dropped, and the session loads a
// new overlay instead.
IN_PROC_BROWSER_TEST_F(SelectionOverlayPrewarmBrowserTest,
                       DropsPrewarmWithOtherOptions) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  base::WeakPtr<content::WebContents> prewarmed = PrewarmAndWait(
      controller, selection::InteractionOptions::New(
                      /*hide_handles=*/true, /*skip_intro_animation=*/true));
  ASSERT_TRUE(prewarmed);

  gfx::Rect bounds = tab->GetContents()->GetViewBounds();
  bounds.Inset(20);
  controller->ShowWithSelection(
      tab->GetContents()->GetPrimaryMainFrame(), bounds,
      selection::InteractionOptions::New(/*hide_handles=*/false,
                                         /*skip_intro_animation=*/true),
      ::selection::EntryPoint::kInlineCue);
  EXPECT_FALSE(prewarmed);
  EXPECT_FALSE(controller->GetPrewarmInteractionOptions());

  ASSERT_OK(RunUntilEqual([controller]() { return controller->state(); },
                          SelectionOverlayController::State::kOverlay,
                          "Timeout waiting for overlay to reach kOverlay"));
  EXPECT_TRUE(controller->GetOverlayWebViewForTesting()->GetWebContents());
}

// A prewarm adopted before its page loads binds the page's handler as soon as
// it asks, without holding it.
IN_PROC_BROWSER_TEST_F(SelectionOverlayPrewarmBrowserTest,
                       AdoptsPrewarmBeforeItsPageBinds) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  auto options = selection::InteractionOptions::New(
      /*hide_handles=*/true, /*skip_intro_animation=*/true);
  base::WeakPtr<content::WebContents> prewarmed = Prewarm(controller, options);
  ASSERT_TRUE(prewarmed);

  // In the same task, so the page can't have asked for its handler yet.
  gfx::Rect bounds = tab->GetContents()->GetViewBounds();
  bounds.Inset(20);
  controller->ShowWithSelection(tab->GetContents()->GetPrimaryMainFrame(),
                                bounds, std::move(options),
                                ::selection::EntryPoint::kInlineCue);
  ASSERT_TRUE(prewarmed);
  ASSERT_EQ(controller->GetOverlayWebViewForTesting()->GetWebContents(),
            prewarmed.get());

  ASSERT_OK(
      RunUntilEqual([controller]() { return controller->state(); },
                    SelectionOverlayController::State::kOverlay,
                    "Timeout waiting for adopted overlay to reach kOverlay"));
  auto* untrusted_ui = prewarmed->GetWebUI()
                           ->GetController()
                           ->GetAs<SelectionOverlayUntrustedUI>();
  ASSERT_TRUE(untrusted_ui);
  EXPECT_FALSE(untrusted_ui->HasPendingPageHandlerForTesting());
}

}  // namespace glic
