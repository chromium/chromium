// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"

#include <optional>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_cueing/prefs.h"
#include "chrome/browser/multistep_filter/core/multistep_filter_service_factory.h"
#include "chrome/browser/multistep_filter/ui/filter_ui_controller_test_api.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/action_ids.h"
#include "chrome/browser/ui/page_action/test_support/mock_page_action_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/multistep_filter/content/filter_initiated_navigation_marker.h"
#include "components/multistep_filter/core/annotation_index/mock_annotation_index_client.h"
#include "components/multistep_filter/core/data_models/suggestion_user_decision.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/logging/multistep_filter_metrics.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/gfx/image/image.h"
#include "ui/strings/grit/ui_strings.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;

constexpr int64_t kTestNavigationId = 0;

class MockFilterUiController : public FilterUiController {
 public:
  explicit MockFilterUiController(tabs::TabInterface& tab)
      : FilterUiController(tab) {}
  ~MockFilterUiController() override = default;

  MOCK_METHOD(void,
              NavigateTo,
              (const UrlFilterSuggestion& suggestion),
              (override));
};

class TestFilterUiController : public FilterUiController {
 public:
  explicit TestFilterUiController(tabs::TabInterface& tab)
      : FilterUiController(tab) {}
  ~TestFilterUiController() override = default;

  // Expose protected methods for testing
  using FilterUiController::NavigateTo;
  using FilterUiController::OnSuggestionGenerated;
};

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MOCK_METHOD(content::WebContents*,
              OpenURLFromTab,
              (content::WebContents * source,
               const content::OpenURLParams& params,
               base::OnceCallback<void(content::NavigationHandle&)>
                   navigation_handle_callback),
              (override));
};

std::vector<FilterAttributeUiLabel> DefaultAttributes() {
  return {FilterAttributeUiLabel(
              FilterSuggestionCandidateAttribute("color", u"Color"),
              FilterAttribute("color", "red")),
          FilterAttributeUiLabel(
              FilterSuggestionCandidateAttribute("size", u"Size"),
              FilterAttribute("size", "large"))};
}

UrlFilterSuggestion CreateDummySuggestion(
    const GURL& url,
    std::vector<FilterAttributeUiLabel> attribute_ui_labels = {}) {
  return UrlFilterSuggestion(
      {.navigation_url = url,
       .source_host = base::UTF8ToUTF16(url.GetHost()),
       .extraction_timestamp = base::Time::Now(),
       .attribute_ui_labels = std::move(attribute_ui_labels),
       .triggering_navigation_id = kTestNavigationId,
       .triggering_host = url.GetHost(),
       .task_type = "task1",
       .suggestion_message = u"Test Message"});
}

page_actions::PageActionState ActionState(bool showing = false) {
  page_actions::PageActionState state;
  state.action_id = kActionMultistepFilter;
  state.showing = showing;
  return state;
}

class FilterUiControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL("about:blank"));
    mock_tab_ = std::make_unique<tabs::MockTabInterface>();
    ON_CALL(*mock_tab_, GetContents()).WillByDefault(Return(web_contents()));
    ON_CALL(*mock_tab_, GetProfile()).WillByDefault(Return(profile()));
    ON_CALL(*mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(unowned_user_data_host_));
    controller_ =
        std::make_unique<testing::NiceMock<MockFilterUiController>>(*mock_tab_);

    mock_page_action_controller_ = std::make_unique<
        testing::NiceMock<page_actions::MockPageActionController>>();
    test_api(*controller_)
        .set_page_action_controller(mock_page_action_controller_.get());

    mock_favicon_service_ =
        std::make_unique<testing::NiceMock<favicon::MockFaviconService>>();
    ON_CALL(*mock_favicon_service_, GetFaviconImageForPageURL(_, _, _))
        .WillByDefault([](const GURL&,
                          favicon_base::FaviconImageCallback callback,
                          base::CancelableTaskTracker*) {
          std::move(callback).Run(favicon_base::FaviconImageResult());
          return base::CancelableTaskTracker::TaskId();
        });
    test_api(*controller_).set_favicon_service(mock_favicon_service_.get());

  }

  void TearDown() override {
    if (controller_) {
      test_api(*controller_).set_favicon_service(nullptr);
    }
    mock_favicon_service_.reset();
    controller_.reset();
    mock_tab_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<tabs::MockTabInterface> CreateMockTab(
      content::WebContents* contents,
      ui::UnownedUserDataHost& host) {
    auto tab = std::make_unique<tabs::MockTabInterface>();
    ON_CALL(*tab, GetContents()).WillByDefault(Return(contents));
    ON_CALL(*tab, GetProfile()).WillByDefault(Return(profile()));
    ON_CALL(*tab, GetUnownedUserDataHost()).WillByDefault(ReturnRef(host));
    return tab;
  }

 protected:
  base::test::ScopedFeatureList feature_list_{kMultistepFilter};
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<tabs::MockTabInterface> mock_tab_;
  std::unique_ptr<testing::NiceMock<MockFilterUiController>> controller_;
  std::unique_ptr<testing::NiceMock<page_actions::MockPageActionController>>
      mock_page_action_controller_;
  std::unique_ptr<favicon::MockFaviconService> mock_favicon_service_;
};

// === Group 1: Lifecycle & Instance ===

TEST_F(FilterUiControllerTest, FromReturnsInstance) {
  EXPECT_EQ(FilterUiController::From(mock_tab_.get()), controller_.get());
}

TEST_F(FilterUiControllerTest, FromReturnsNullIfNotFound) {
  controller_.reset();
  EXPECT_EQ(FilterUiController::From(mock_tab_.get()), nullptr);
}

// === Group 2: Suggestion Generation (OnSuggestionGenerated) ===

TEST_F(FilterUiControllerTest, OnSuggestionGeneratedShowsCue) {
  EXPECT_CALL(*mock_page_action_controller_, Show(kActionMultistepFilter))
      .Times(1);
  EXPECT_CALL(*mock_page_action_controller_,
              ShowAnchoredMessage(kActionMultistepFilter, _))
      .Times(1);

  GURL url("https://example.com");
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(url, DefaultAttributes());
  suggestion.suggestion_message = u"Test Message";

  controller_->OnSuggestionGenerated(suggestion, {});

  const std::optional<FilterUiController::SuggestionState>& state =
      test_api(*controller_).suggestion_state();
  ASSERT_TRUE(state.has_value());
  EXPECT_EQ(state->suggestion, suggestion);
}

TEST_F(FilterUiControllerTest,
       SuggestionCallbackDoesNothingIfFaviconServiceNull) {
  test_api(*controller_).set_favicon_service(nullptr);

  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());

  controller_->OnSuggestionGenerated(suggestion, {});
  EXPECT_FALSE(test_api(*controller_).suggestion_state().has_value());
}

TEST_F(FilterUiControllerTest, SuggestionCallbackIgnoresNullopt) {
  // Also verify that direct calls with nullopt are ignored.
  controller_->OnSuggestionGenerated(std::nullopt, {});
  EXPECT_FALSE(test_api(*controller_).suggestion_state().has_value());
}

TEST_F(FilterUiControllerTest,
       OnSuggestionGeneratedWithNullPageActionController) {
  test_api(*controller_).set_page_action_controller(nullptr);

  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  suggestion.suggestion_message = u"Test Message";

  controller_->OnSuggestionGenerated(suggestion, {});
  EXPECT_FALSE(test_api(*controller_).suggestion_state().has_value());
}


TEST_F(FilterUiControllerTest, OnSuggestionGeneratedWithNullPrefService) {
  test_api(*controller_).set_pref_service(nullptr);

  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  suggestion.suggestion_message = u"Test Message";

  controller_->OnSuggestionGenerated(suggestion, {});
  EXPECT_FALSE(test_api(*controller_).suggestion_state().has_value());
}

TEST_F(FilterUiControllerTest, OnSuggestionGeneratedWhenSettingDisabled) {
  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::kContextualCueing),
      std::to_underlying(
          optimization_guide::prefs::FeatureOptInState::kDisabled));

  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  suggestion.suggestion_message = u"Test Message";

  controller_->OnSuggestionGenerated(suggestion, {});
  EXPECT_FALSE(test_api(*controller_).suggestion_state().has_value());
}

TEST_F(FilterUiControllerTest, OnSuggestionGeneratedWhenSettingEnabled) {
  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::kContextualCueing),
      std::to_underlying(optimization_guide::prefs::FeatureOptInState::kEnabled));

  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  suggestion.suggestion_message = u"Test Message";

  controller_->OnSuggestionGenerated(suggestion, {});
  EXPECT_TRUE(test_api(*controller_).suggestion_state().has_value());
}

TEST_F(FilterUiControllerTest,
       OnSuggestionGeneratedWhenEnterprisePolicyDisabled) {
  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kChromeSuggestionsSettings,
      std::to_underlying(
          contextual_cueing::ChromeSuggestionsSettingsValue::kDisabled));

  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  suggestion.suggestion_message = u"Test Message";

  controller_->OnSuggestionGenerated(suggestion, {});
  EXPECT_FALSE(test_api(*controller_).suggestion_state().has_value());
}

TEST_F(FilterUiControllerTest, OnFaviconAvailableWithValidIcon) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  bitmap.eraseColor(SK_ColorRED);
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(bitmap);

  favicon_base::FaviconImageResult favicon_result;
  favicon_result.image = image;
  favicon_result.icon_url = GURL("https://example.com/favicon.ico");

  EXPECT_CALL(*mock_favicon_service_, GetFaviconImageForPageURL(_, _, _))
      .WillOnce([&](const GURL&, favicon_base::FaviconImageCallback callback,
                    base::CancelableTaskTracker*) {
        std::move(callback).Run(favicon_result);
        return base::CancelableTaskTracker::TaskId();
      });

  GURL url("https://example.com");
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(url, DefaultAttributes());

  EXPECT_CALL(*mock_page_action_controller_,
              SetAnchoredMessageExpandableContent(kActionMultistepFilter, _))
      .WillOnce(
          [&](actions::ActionId,
              std::optional<page_actions::AnchoredMessageExpandableContent>
                  content) {
            ASSERT_TRUE(content.has_value());
            ASSERT_EQ(content->items.size(), 1u);
            EXPECT_EQ(content->items[0].text, u"example.com");
            EXPECT_FALSE(content->items[0].icon->IsEmpty());
            EXPECT_TRUE(content->items[0].icon->IsImage());
          });

  controller_->OnSuggestionGenerated(suggestion, {});
}

TEST_F(FilterUiControllerTest, OnFaviconAvailableWithEmptyIcon) {
  favicon_base::FaviconImageResult favicon_result;

  EXPECT_CALL(*mock_favicon_service_, GetFaviconImageForPageURL(_, _, _))
      .WillOnce([&](const GURL&, favicon_base::FaviconImageCallback callback,
                    base::CancelableTaskTracker*) {
        std::move(callback).Run(favicon_result);
        return base::CancelableTaskTracker::TaskId();
      });

  GURL url("https://example.com");
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(url, DefaultAttributes());

  EXPECT_CALL(*mock_page_action_controller_,
              SetAnchoredMessageExpandableContent(kActionMultistepFilter, _))
      .WillOnce(
          [&](actions::ActionId,
              std::optional<page_actions::AnchoredMessageExpandableContent>
                  content) {
            ASSERT_TRUE(content.has_value());
            ASSERT_EQ(content->items.size(), 1u);
            EXPECT_EQ(content->items[0].text, u"example.com");
            EXPECT_FALSE(content->items[0].icon->IsEmpty());
            EXPECT_TRUE(content->items[0].icon->IsVectorIcon());
          });

  controller_->OnSuggestionGenerated(suggestion, {});
}

// === Group 3: Clear & Dismissal ===

TEST_F(FilterUiControllerTest, ClearSuggestionResetsCachedSuggestion) {
  GURL url("https://example.com");
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(url, DefaultAttributes());

  controller_->OnSuggestionGenerated(suggestion, {});

  controller_->ClearSuggestion(SuggestionUserDecision::kIgnored);

  // Verify that the current suggestion is reset.
  EXPECT_FALSE(test_api(*controller_).suggestion_state().has_value());
}

TEST_F(FilterUiControllerTest, ClearSuggestionHidesPageAction) {
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  suggestion.suggestion_message = u"Test Message";

  // Generate suggestion to set up state and show cue.
  EXPECT_CALL(*mock_page_action_controller_, Show(kActionMultistepFilter))
      .Times(1);
  controller_->OnSuggestionGenerated(suggestion, {});

  // Now clear suggestion and verify it hides the cue.
  EXPECT_CALL(*mock_page_action_controller_, Hide(kActionMultistepFilter))
      .Times(1);
  EXPECT_CALL(*mock_page_action_controller_,
              HideAnchoredMessage(kActionMultistepFilter))
      .Times(1);

  controller_->ClearSuggestion(SuggestionUserDecision::kIgnored);
}

TEST_F(FilterUiControllerTest, ClearSuggestionResetsViewState) {
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  controller_->OnSuggestionGenerated(suggestion, {});

  test_api(*controller_).OnPageActionAnchoredMessageShown(ActionState());
  EXPECT_EQ(test_api(*controller_).suggestion_state()->view_state,
            FilterUiController::SuggestionViewState::kShowingInitialCue);

  controller_->ClearSuggestion(SuggestionUserDecision::kIgnored);
  EXPECT_FALSE(test_api(*controller_).suggestion_state().has_value());
}

TEST_F(FilterUiControllerTest, CallbackNotifiedOnClearSuggestion) {
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  base::MockCallback<base::OnceCallback<void(SuggestionUserDecision)>>
      on_decision;
  controller_->OnSuggestionGenerated(
      suggestion, MultistepFilterUiDelegate::SuggestionUiCallbacks{
                      .on_user_interaction = on_decision.Get(),
                  });
  test_api(*controller_).OnPageActionAnchoredMessageShown(ActionState());

  EXPECT_CALL(on_decision, Run(SuggestionUserDecision::kAccepted)).Times(1);
  controller_->ClearSuggestion(SuggestionUserDecision::kAccepted);
}

// === Group 4: Apply Suggestion & Navigation ===

TEST_F(FilterUiControllerTest, ApplySuggestion) {
  // Should do nothing if there's no suggestion.
  EXPECT_CALL(*controller_, NavigateTo(_)).Times(0);
  controller_->ApplySuggestion();

  // Should do nothing if the URL is empty.
  UrlFilterSuggestion empty_url_suggestion =
      CreateDummySuggestion(GURL(), DefaultAttributes());
  controller_->OnSuggestionGenerated(empty_url_suggestion, {});
  controller_->ApplySuggestion();

  // Should navigate to the suggestion URL.
  GURL url("https://example.com");
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(url, DefaultAttributes());
  controller_->OnSuggestionGenerated(suggestion, {});

  EXPECT_CALL(*controller_, NavigateTo(suggestion));
  controller_->ApplySuggestion();
}

TEST_F(FilterUiControllerTest, NavigateToWithNullWebContentsDoesNotCrash) {
  ui::UnownedUserDataHost null_contents_host;
  auto mock_tab_null_contents = CreateMockTab(nullptr, null_contents_host);

  // Instantiating FilterUiController with a null WebContents should not crash.
  auto controller_null_contents =
      std::make_unique<TestFilterUiController>(*mock_tab_null_contents);

  // Should not crash.
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  controller_null_contents->NavigateTo(suggestion);
}

TEST_F(FilterUiControllerTest, NavigateToWithWebContents) {
  // Destroy the controller created in SetUp() so we can create a new one
  // on the same tab without hitting a reinsertion check.
  controller_.reset();

  auto controller = std::make_unique<TestFilterUiController>(*mock_tab_);

  MockWebContentsDelegate delegate;
  web_contents()->SetDelegate(&delegate);

  GURL url("https://example.com");
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(url, DefaultAttributes());

  EXPECT_CALL(
      delegate,
      OpenURLFromTab(web_contents(),
                     testing::Field(&content::OpenURLParams::url, url), _))
      .WillOnce(
          [&](content::WebContents* source,
              const content::OpenURLParams& params,
              base::OnceCallback<void(content::NavigationHandle&)> callback) {
            EXPECT_FALSE(callback.is_null());
            content::MockNavigationHandle handle;
            std::move(callback).Run(handle);
            EXPECT_NE(multistep_filter::FilterInitiatedNavigationMarker::
                          GetForNavigationHandle(handle),
                      nullptr);
            return web_contents();
          });

  controller->NavigateTo(suggestion);
}

// === Group 5: Menu Delegate Methods (IsCommandIdChecked, IsCommandIdEnabled,
// ExecuteCommand) ===

TEST_F(FilterUiControllerTest, IsCommandIdCheckedReturnsFalse) {
  EXPECT_FALSE(
      test_api(*controller_).IsCommandIdChecked(internal::kDismissCommand));
  EXPECT_FALSE(
      test_api(*controller_).IsCommandIdChecked(internal::kSettingsCommand));
  EXPECT_FALSE(test_api(*controller_)
                   .IsCommandIdChecked(internal::kSendFeedbackCommand));
}

TEST_F(FilterUiControllerTest, IsCommandIdEnabledReturnsTrue) {
  EXPECT_TRUE(
      test_api(*controller_).IsCommandIdEnabled(internal::kDismissCommand));
  EXPECT_TRUE(
      test_api(*controller_).IsCommandIdEnabled(internal::kSettingsCommand));
  EXPECT_TRUE(test_api(*controller_)
                  .IsCommandIdEnabled(internal::kSendFeedbackCommand));
}

TEST_F(FilterUiControllerTest, ExecuteCommandDismissClearsSuggestion) {
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  controller_->OnSuggestionGenerated(suggestion, {});
  EXPECT_NE(test_api(*controller_).suggestion_state(), std::nullopt);

  test_api(*controller_).ExecuteCommand(internal::kDismissCommand, 0);
  EXPECT_EQ(test_api(*controller_).suggestion_state(), std::nullopt);
}

TEST_F(FilterUiControllerTest,
       ExecuteCommandSendFeedbackDoesNotClearSuggestion) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kMultistepFilterSendFeedback,
      {{"MultistepFilterSendFeedbackUrl", "https://feedback.google.com"}});

  MockWebContentsDelegate delegate;
  web_contents()->SetDelegate(&delegate);

  EXPECT_CALL(delegate, OpenURLFromTab(
                            web_contents(),
                            testing::Field(&content::OpenURLParams::url,
                                           GURL("https://feedback.google.com")),
                            _))
      .WillOnce(testing::Return(web_contents()));

  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  controller_->OnSuggestionGenerated(suggestion, {});
  EXPECT_NE(test_api(*controller_).suggestion_state(), std::nullopt);

  test_api(*controller_).ExecuteCommand(internal::kSendFeedbackCommand, 0);
  EXPECT_NE(test_api(*controller_).suggestion_state(), std::nullopt);
}

// === Group 6: Action Invocation (OnActionInvoked) ===

TEST_F(FilterUiControllerTest, OnActionInvokedAppliesSuggestionWhenShowing) {
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  controller_->OnSuggestionGenerated(suggestion, {});

  // Simulate bubble showing
  test_api(*controller_).OnPageActionAnchoredMessageShown(ActionState());
  EXPECT_EQ(test_api(*controller_).suggestion_state()->view_state,
            FilterUiController::SuggestionViewState::kShowingInitialCue);

  EXPECT_CALL(*controller_, NavigateTo(_)).Times(1);
  controller_->OnActionInvoked();
}

TEST_F(FilterUiControllerTest, OnActionInvokedReopensBubbleWhenCollapsed) {
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  controller_->OnSuggestionGenerated(suggestion, {});

  // Simulate cue shown then collapsed
  test_api(*controller_).OnPageActionAnchoredMessageShown(ActionState());
  test_api(*controller_).OnPageActionAnchoredMessageHidden(ActionState());
  EXPECT_EQ(test_api(*controller_).suggestion_state()->view_state,
            FilterUiController::SuggestionViewState::kCollapsedInOmnibox);

  // Invoking action when collapsed should reopen cue (no navigation)
  EXPECT_CALL(*controller_, NavigateTo(_)).Times(0);
  controller_->OnActionInvoked();

  // Simulate bubble shown again (reopened)
  test_api(*controller_).OnPageActionAnchoredMessageShown(ActionState());
  EXPECT_EQ(test_api(*controller_).suggestion_state()->view_state,
            FilterUiController::SuggestionViewState::kReopenedFromOmnibox);
}

TEST_F(FilterUiControllerTest, ReopenCueUsesCachedFaviconWithoutFetchingAgain) {
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());

  // 1. Initial suggestion generation fetches favicon exactly ONCE.
  EXPECT_CALL(*mock_favicon_service_, GetFaviconImageForPageURL(_, _, _))
      .Times(1)
      .WillOnce([&](const GURL&,
                    favicon_base::FaviconImageCallback callback,
                    base::CancelableTaskTracker*) {
        std::move(callback).Run(favicon_base::FaviconImageResult());
        return base::CancelableTaskTracker::TaskId();
      });

  controller_->OnSuggestionGenerated(suggestion, {});

  // Simulate showing and collapsing
  test_api(*controller_).OnPageActionAnchoredMessageShown(ActionState());
  test_api(*controller_).OnPageActionAnchoredMessageHidden(ActionState());
  ASSERT_EQ(test_api(*controller_).suggestion_state()->view_state,
            FilterUiController::SuggestionViewState::kCollapsedInOmnibox);

  // 2. Reopening the cue should NOT call GetFaviconImageForPageURL again,
  // but it should call SetAnchoredMessageExpandableContent.
  EXPECT_CALL(*mock_favicon_service_, GetFaviconImageForPageURL(_, _, _))
      .Times(0);
  EXPECT_CALL(*mock_page_action_controller_,
              SetAnchoredMessageExpandableContent(kActionMultistepFilter, _))
      .Times(1);

  controller_->OnActionInvoked();
}

TEST_F(FilterUiControllerTest,
       OnPageActionAnchoredMessageShownInitialCueCallbackNotified) {
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  base::MockCallback<base::OnceClosure> on_shown;
  controller_->OnSuggestionGenerated(
      suggestion, MultistepFilterUiDelegate::SuggestionUiCallbacks{
                      .on_suggestion_shown = on_shown.Get(),
                  });

  EXPECT_CALL(on_shown, Run()).Times(1);
  test_api(*controller_).OnPageActionAnchoredMessageShown(ActionState());
}

TEST_F(FilterUiControllerTest,
       OnPageActionAnchoredMessageShownReopenedCueCallbackNotified) {
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  base::MockCallback<base::OnceClosure> on_reopened;
  controller_->OnSuggestionGenerated(
      suggestion, MultistepFilterUiDelegate::SuggestionUiCallbacks{
                      .on_suggestion_reopened = on_reopened.Get(),
                  });
  test_api(*controller_).OnPageActionAnchoredMessageShown(ActionState());

  // Collapse into Omnibox
  test_api(*controller_).OnPageActionAnchoredMessageHidden(ActionState());

  EXPECT_CALL(on_reopened, Run()).Times(1);
  // Reopen
  controller_->OnActionInvoked();
  test_api(*controller_).OnPageActionAnchoredMessageShown(ActionState());
}

// === Group 7: Observer Lifecycle & State Transitions ===

TEST_F(FilterUiControllerTest,
       OnPageActionAnchoredMessageShownUpdatesViewState) {
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  controller_->OnSuggestionGenerated(suggestion, {});
  EXPECT_EQ(test_api(*controller_).suggestion_state()->view_state,
            FilterUiController::SuggestionViewState::kInactive);

  test_api(*controller_).OnPageActionAnchoredMessageShown(ActionState());
  EXPECT_EQ(test_api(*controller_).suggestion_state()->view_state,
            FilterUiController::SuggestionViewState::kShowingInitialCue);
}

TEST_F(FilterUiControllerTest,
       OnPageActionAnchoredMessageHiddenUpdatesViewState) {
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  controller_->OnSuggestionGenerated(suggestion, {});

  test_api(*controller_).OnPageActionAnchoredMessageShown(ActionState());
  test_api(*controller_).OnPageActionAnchoredMessageHidden(ActionState());
  EXPECT_EQ(test_api(*controller_).suggestion_state()->view_state,
            FilterUiController::SuggestionViewState::kCollapsedInOmnibox);
  EXPECT_TRUE(test_api(*controller_).suggestion_state().has_value());
}


TEST_F(
    FilterUiControllerTest,
    OnPageActionAnchoredMessageHiddenFromReopenedStateCollapsesToOmniboxAfterReopen) {
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  controller_->OnSuggestionGenerated(suggestion, {});

  // Transition: Inactive -> ShowingInitialCue -> CollapsedInOmnibox ->
  // ReopenedFromOmnibox
  test_api(*controller_).OnPageActionAnchoredMessageShown(ActionState());
  test_api(*controller_).OnPageActionAnchoredMessageHidden(ActionState());
  controller_->OnActionInvoked();
  test_api(*controller_).OnPageActionAnchoredMessageShown(ActionState());
  EXPECT_EQ(test_api(*controller_).suggestion_state()->view_state,
            FilterUiController::SuggestionViewState::kReopenedFromOmnibox);

  // Transition: ReopenedFromOmnibox -> CollapsedInOmniboxAfterReopen (on
  // hidden)
  test_api(*controller_).OnPageActionAnchoredMessageHidden(ActionState());
  EXPECT_EQ(
      test_api(*controller_).suggestion_state()->view_state,
      FilterUiController::SuggestionViewState::kCollapsedInOmniboxAfterReopen);
  EXPECT_TRUE(test_api(*controller_).suggestion_state().has_value());
}



}  // namespace

}  // namespace multistep_filter
