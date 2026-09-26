// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/selection/prompt_suggestion.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/selection/mojom/action.mojom.h"
#include "chrome/browser/selection/suggestion_service.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class SelectionOverlayBrowserTest : public GlicBrowserTest {
 public:
  SelectionOverlayBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(::features::kGlicCaptureRegion);
  }
  ~SelectionOverlayBrowserTest() override = default;

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
  controller->Show(/*options=*/nullptr);

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
}

namespace {

class TestSuggestedActionsListener
    : public selection::SuggestedActionsListener {
 public:
  TestSuggestedActionsListener() = default;
  ~TestSuggestedActionsListener() override = default;

  mojo::PendingRemote<selection::SuggestedActionsListener>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnSuggestedActionsAvailable(
      std::vector<selection::SuggestedActionPtr> actions) override {
    for (auto& action : actions) {
      actions_.push_back(std::move(action));
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
  mojo::Receiver<selection::SuggestedActionsListener> receiver_{this};
  std::vector<selection::SuggestedActionPtr> actions_;
  size_t batches_received_ = 0;
  size_t expected_batches_ = 0;
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SelectionOverlayBrowserTest,
                       SuggestedActionsDisabledByDefault) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr);

  TestSuggestedActionsListener listener;
  static_cast<selection::SelectionOverlayPageHandler*>(controller)
      ->GetSuggestedActions(listener.BindNewPipeAndPassRemote());
  listener.WaitForBatches(1);
  EXPECT_TRUE(listener.actions().empty());
}

class SelectionOverlayPromptBrowserTest : public GlicBrowserTest {
 public:
  SelectionOverlayPromptBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {::features::kGlicCaptureRegion,
         ::features::kGlicSelectionOverlayPrompt,
         ::features::kGlicSelectionOverlayPromptBox},
        {});
  }
  ~SelectionOverlayPromptBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

namespace {

class FakeStaticSelectionSuggestionTool
    : public ::selection::SuggestionTool {
 public:
  explicit FakeStaticSelectionSuggestionTool(tabs::TabInterface* tab)
      : tab_(tab) {}
  ~FakeStaticSelectionSuggestionTool() override = default;

  ToolId GetToolId() const override {
    return optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME;
  }

  void RequestSuggestions(const ::selection::AreaOfInterest& processed_area,
                          ::selection::SuggestionsCallback callback) override {
    std::vector<std::unique_ptr<::selection::Suggestion>> suggestions;
    suggestions.push_back(std::make_unique<PromptSuggestion>(
        *tab_, u"Explain", "Explain the selection in a few sentences."));
    suggestions.push_back(std::make_unique<PromptSuggestion>(
        *tab_, u"Summarize", "Summarize the selection in a few sentences."));
    suggestions.push_back(std::make_unique<PromptSuggestion>(
        *tab_, u"Create Image",
        "Create a cartoon styled image from the selection."));
    std::move(callback).Run(std::move(suggestions), /*complete=*/true);
  }

 private:
  raw_ptr<tabs::TabInterface> tab_;
};

class FakeSelectionSuggestionTool
    : public ::selection::SuggestionTool {
 public:
  explicit FakeSelectionSuggestionTool(tabs::TabInterface* tab)
      : tab_(tab) {}
  ~FakeSelectionSuggestionTool() override = default;

  ToolId GetToolId() const override {
    return optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME;
  }

  void RequestSuggestions(const ::selection::AreaOfInterest& processed_area,
                          ::selection::SuggestionsCallback callback) override {
    std::vector<std::unique_ptr<::selection::Suggestion>> suggestions;
    suggestions.push_back(std::make_unique<PromptSuggestion>(
        *tab_, u"Translate to Spanish",
        "Translate the selected text to Spanish."));
    suggestions.push_back(std::make_unique<PromptSuggestion>(
        *tab_, u"Fact check", "Fact check the claims in this section."));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(suggestions),
                                  /*complete=*/true));
  }

 private:
  raw_ptr<tabs::TabInterface> tab_;
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
  FakeStaticSelectionSuggestionTool static_tool(tab);
  ScopedToolRegistration registration(suggestion_service, &static_tool);

  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr);
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
  handler->GetSuggestedActions(listener.BindNewPipeAndPassRemote());
  listener.WaitForBatches(1);
  const auto& actions = listener.actions();
  ASSERT_EQ(actions.size(), 3u);
  EXPECT_FALSE(actions[0]->id.is_empty());
  EXPECT_EQ(actions[0]->title, "Explain");
  EXPECT_FALSE(actions[1]->id.is_empty());
  EXPECT_EQ(actions[1]->title, "Summarize");
  EXPECT_FALSE(actions[2]->id.is_empty());
  EXPECT_EQ(actions[2]->title, "Create Image");
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
  controller->Show(/*options=*/nullptr);
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
  handler->GetSuggestedActions(listener.BindNewPipeAndPassRemote());
  listener.WaitForBatches(1);
  const auto& actions = listener.actions();
  ASSERT_EQ(actions.size(), 3u);
  EXPECT_EQ(actions[0]->title, "Explain");
  EXPECT_EQ(actions[1]->title, "Summarize");
  EXPECT_EQ(actions[2]->title, "Create Image");
}

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
  controller->ShowWithSelection(selection_bounds);

  EXPECT_EQ(controller->GetSelectedRegionCount(), 1u);

  controller->Close();
  EXPECT_EQ(controller->GetSelectedRegionCount(), 0u);
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayPromptBrowserTest,
                       SubmitPromptWithSelectedRegion) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_TRUE(OpenGlicForActiveTab().has_value());
  content::WebContents* web_contents = tab->GetContents();
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);

  gfx::Rect view_bounds = web_contents->GetViewBounds();
  gfx::Rect selection_bounds(view_bounds.x() + 10, view_bounds.y() + 10, 100,
                             50);
  controller->ShowWithSelection(selection_bounds);

  EXPECT_EQ(controller->GetSelectedRegionCount(), 1u);

  static_cast<selection::SelectionOverlayPageHandler*>(controller)
      ->SubmitPrompt("Explain this selection");

  // The browser started this session, so it dismisses the overlay itself.
  EXPECT_EQ(controller->GetSelectedRegionCount(), 0u);

  controller->Close();
  EXPECT_EQ(controller->GetSelectedRegionCount(), 0u);
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayPromptBrowserTest,
                       SuggestedActionsFromMockTool) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();

  auto* suggestion_service = ::selection::SuggestionService::From(tab);
  ASSERT_TRUE(suggestion_service);
  FakeStaticSelectionSuggestionTool static_tool(tab);
  FakeSelectionSuggestionTool fake_tool(tab);
  ScopedToolRegistration static_registration(suggestion_service, &static_tool);
  ScopedToolRegistration fake_registration(suggestion_service, &fake_tool);

  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr);
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
  handler->GetSuggestedActions(listener.BindNewPipeAndPassRemote());
  listener.WaitForBatches(2);
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

class CountingSelectionSuggestionTool
    : public ::selection::SuggestionTool {
 public:
  explicit CountingSelectionSuggestionTool(tabs::TabInterface* tab)
      : tab_(tab) {}
  ~CountingSelectionSuggestionTool() override = default;

  ToolId GetToolId() const override {
    return optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME;
  }

  void RequestSuggestions(const ::selection::AreaOfInterest& processed_area,
                          ::selection::SuggestionsCallback callback) override {
    request_count_++;
    last_aoi_screenshot_ = processed_area.screenshot;
    last_aoi_apc_ = processed_area.apc;
    if (std::holds_alternative<gfx::Rect>(processed_area.bounds)) {
      last_rect_ = std::get<gfx::Rect>(processed_area.bounds);
    }
    std::vector<std::unique_ptr<::selection::Suggestion>> suggestions;
    std::u16string label = u"Action " + base::NumberToString16(request_count_);
    suggestions.push_back(
        std::make_unique<PromptSuggestion>(*tab_, label, "Prompt"));
    std::move(callback).Run(std::move(suggestions), /*complete=*/true);
  }

  int request_count() const { return request_count_; }
  const gfx::Rect& last_rect() const { return last_rect_; }
  const SkBitmap& last_aoi_screenshot() const { return last_aoi_screenshot_; }
  const optimization_guide::proto::AnnotatedPageContent& last_aoi_apc() const {
    return last_aoi_apc_;
  }

 private:
  raw_ptr<tabs::TabInterface> tab_;
  int request_count_ = 0;
  gfx::Rect last_rect_;
  SkBitmap last_aoi_screenshot_;
  optimization_guide::proto::AnnotatedPageContent last_aoi_apc_;
};

}  // namespace

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
  controller->Show(/*options=*/nullptr);
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
    handler->GetSuggestedActions(listener.BindNewPipeAndPassRemote());
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
    handler->GetSuggestedActions(listener.BindNewPipeAndPassRemote());
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
    handler->GetSuggestedActions(listener.BindNewPipeAndPassRemote());
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
    handler->GetSuggestedActions(listener.BindNewPipeAndPassRemote());
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
    handler->GetSuggestedActions(listener.BindNewPipeAndPassRemote());
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
    handler->GetSuggestedActions(listener.BindNewPipeAndPassRemote());
    listener.WaitForBatches(1);
    ASSERT_EQ(listener.actions().size(), 1u);
    EXPECT_EQ(listener.actions()[0]->title, "Action 3");
    EXPECT_NE(listener.actions()[0]->id, region1_action_id);
    EXPECT_EQ(counting_tool.request_count(), 3);
  }
}

namespace {

class FakePromptSuggestionTool : public ::selection::SuggestionTool {
 public:
  explicit FakePromptSuggestionTool(tabs::TabInterface* tab) : tab_(tab) {}
  ~FakePromptSuggestionTool() override = default;

  ToolId GetToolId() const override {
    return optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME;
  }

  void RequestSuggestions(const ::selection::AreaOfInterest& processed_area,
                          ::selection::SuggestionsCallback callback) override {
    std::vector<std::unique_ptr<::selection::Suggestion>> suggestions;
    suggestions.push_back(std::make_unique<PromptSuggestion>(
        *tab_, u"Explain", "Explain the selection in a few sentences."));
    std::move(callback).Run(std::move(suggestions), /*complete=*/true);
  }

 private:
  raw_ptr<tabs::TabInterface> tab_;
};

class InlineSuggestion : public ::selection::Suggestion {
 public:
  InlineSuggestion() = default;
  ~InlineSuggestion() override = default;

  // ::selection::Suggestion:
  const std::u16string& GetLabel() const override { return label_; }
  void OnSuggestionPresented() override {}
  void OnSuggestionExecuted() override {}
  ::selection::mojom::ActionPtr GetAction() const override {
    return ::selection::mojom::Action::NewInlineFulfillment(
        ::selection::mojom::InlineFulfillment::New("does_not_matter.js"));
  }

 private:
  std::u16string label_ = u"InlineSuggestion";
};

class FakeInlineSuggestionTool : public ::selection::SuggestionTool {
 public:
  FakeInlineSuggestionTool() = default;
  ~FakeInlineSuggestionTool() override = default;

  ToolId GetToolId() const override {
    return optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME;
  }

  void RequestSuggestions(const ::selection::AreaOfInterest& processed_area,
                          ::selection::SuggestionsCallback callback) override {
    std::vector<std::unique_ptr<::selection::Suggestion>> suggestions;
    suggestions.push_back(std::make_unique<InlineSuggestion>());
    std::move(callback).Run(std::move(suggestions), /*complete=*/true);
  }
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
  handler->GetSuggestedActions(listener.BindNewPipeAndPassRemote());
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
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_TRUE(OpenGlicForActiveTab().has_value());

  auto* suggestion_service = ::selection::SuggestionService::From(tab);
  ASSERT_TRUE(suggestion_service);
  FakePromptSuggestionTool tool(tab);
  ScopedToolRegistration registration(suggestion_service, &tool);

  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr);
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
      ->ExecuteSuggestedAction(action_id);
  EXPECT_EQ(controller->GetSelectedRegionCount(), 0u);
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayPromptBrowserTest,
                       InlineFulfillmentKeepsOverlay) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());

  auto* suggestion_service = ::selection::SuggestionService::From(tab);
  ASSERT_TRUE(suggestion_service);
  FakeInlineSuggestionTool tool;
  ScopedToolRegistration registration(suggestion_service, &tool);

  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  controller->Show(/*options=*/nullptr);
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

  static_cast<selection::SelectionOverlayPageHandler*>(controller)
      ->ExecuteSuggestedAction(action_id);
  EXPECT_EQ(controller->state(), SelectionOverlayController::State::kOverlay);
  EXPECT_EQ(controller->GetSelectedRegionCount(), 1u);
}

}  // namespace glic
