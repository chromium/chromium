// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"

#include <optional>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/multistep_filter/ui/filter_ui_controller_test_api.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/action_ids.h"
#include "chrome/browser/ui/page_action/test_support/mock_page_action_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/multistep_filter/content/filter_initiated_navigation_marker.h"
#include "components/multistep_filter/core/annotation_index/mock_annotation_index_client.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/multistep_filter/core/storage/filter_store.h"
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

  MOCK_METHOD(void, NavigateTo, (const GURL& url), (override));
  MOCK_METHOD(void, ClearSuggestion, (), (override));
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

class MockMultistepFilterService : public MultistepFilterService {
 public:
  MockMultistepFilterService(
      std::unique_ptr<AnnotationIndexClient> extraction_client,
      std::unique_ptr<FilterStore> store)
      : MultistepFilterService([&]() {
          MultistepFilterService::Params params;
          params.annotation_index_client = std::move(extraction_client);
          params.filter_store = std::move(store);
          params.identity_manager = nullptr;
          params.consent_helper = nullptr;
          params.log_router = nullptr;
          return params;
        }()) {}
  ~MockMultistepFilterService() override = default;

  MOCK_METHOD(void,
              DeleteAnnotationsForTask,
              (std::string_view task_type,
               int64_t navigation_id,
               std::string_view domain),
              (override));
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
       .source_domain = base::UTF8ToUTF16(GetEtldPlusOne(url)),
       .extraction_timestamp = base::Time::Now(),
       .attribute_ui_labels = std::move(attribute_ui_labels),
       .triggering_navigation_id = kTestNavigationId,
       .triggering_domain = GetEtldPlusOne(url),
       .task_type = "task1",
       .suggestion_message = u"Test Message"});
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
    test_api(*controller_).set_favicon_service(&mock_favicon_service_);

    mock_service_ =
        std::make_unique<testing::NiceMock<MockMultistepFilterService>>(
            std::make_unique<testing::NiceMock<MockAnnotationIndexClient>>(),
            std::make_unique<FilterStore>());
    test_api(*controller_).set_service(mock_service_.get());
  }

  void TearDown() override {
    if (controller_) {
      test_api(*controller_).set_service(nullptr);
    }
    mock_service_.reset();
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
  testing::NiceMock<favicon::MockFaviconService> mock_favicon_service_;
  std::unique_ptr<MockMultistepFilterService> mock_service_;
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

TEST_F(FilterUiControllerTest, SuggestionCallbackGeneratesSuggestion) {
  testing::NiceMock<page_actions::MockPageActionController> mock_controller;
  test_api(*controller_).set_page_action_controller(&mock_controller);

  EXPECT_CALL(mock_controller, Show(kActionMultistepFilter)).Times(1);
  EXPECT_CALL(mock_controller, ShowAnchoredMessage(kActionMultistepFilter, _))
      .Times(1);

  GURL url("https://example.com");
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(url, DefaultAttributes());
  suggestion.suggestion_message = u"Test Message";

  controller_->OnSuggestionGenerated(suggestion);

  favicon_base::FaviconImageResult result;
  // Manually invoke the callback to simulate successful async favicon returns
  // in a synthetic test environment.
  test_api(*controller_).OnFaviconAvailable(suggestion, result);

  const std::optional<UrlFilterSuggestion>& cached_suggestion =
      test_api(*controller_).current_url_filter_suggestion();
  ASSERT_TRUE(cached_suggestion.has_value());
  EXPECT_EQ(*cached_suggestion, suggestion);
}

TEST_F(FilterUiControllerTest, SuggestionCallbackTriggersService) {
  GURL url("https://example.com");
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(url, DefaultAttributes());

  EXPECT_CALL(*mock_service_,
              DeleteAnnotationsForTask(testing::Eq("task1"),
                                       testing::Eq(kTestNavigationId),
                                       testing::Eq("example.com")));

  controller_->OnSuggestionGenerated(suggestion);
}

TEST_F(FilterUiControllerTest, SuggestionCallbackDoesNothingIfServiceNull) {
  GURL url("https://example.com");
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(url, DefaultAttributes());

  // service_ is null by default.
  test_api(*controller_).set_service(nullptr);

  testing::NiceMock<page_actions::MockPageActionController> mock_controller;
  test_api(*controller_).set_page_action_controller(&mock_controller);

  EXPECT_CALL(mock_controller, Show(_)).Times(0);
  EXPECT_CALL(mock_controller, ShowAnchoredMessage(_, _)).Times(0);

  controller_->OnSuggestionGenerated(suggestion);
  EXPECT_FALSE(
      test_api(*controller_).current_url_filter_suggestion().has_value());
}

TEST_F(FilterUiControllerTest, SuggestionCallbackIgnoresNullopt) {
  // Also verify that direct calls with nullopt are ignored.
  testing::NiceMock<page_actions::MockPageActionController> mock_controller;
  test_api(*controller_).set_page_action_controller(&mock_controller);

  EXPECT_CALL(mock_controller, Show(_)).Times(0);
  EXPECT_CALL(mock_controller, ShowAnchoredMessage(_, _)).Times(0);

  controller_->OnSuggestionGenerated(std::nullopt);
  EXPECT_FALSE(
      test_api(*controller_).current_url_filter_suggestion().has_value());
}

TEST_F(FilterUiControllerTest,
       OnSuggestionGeneratedWithNullPageActionController) {
  test_api(*controller_).set_page_action_controller(nullptr);

  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  suggestion.suggestion_message = u"Test Message";

  controller_->OnSuggestionGenerated(suggestion);
  EXPECT_FALSE(
      test_api(*controller_).current_url_filter_suggestion().has_value());
}

TEST_F(FilterUiControllerTest, OnSuggestionGeneratedWithNullFaviconService) {
  test_api(*controller_).set_favicon_service(nullptr);

  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  suggestion.suggestion_message = u"Test Message";

  controller_->OnSuggestionGenerated(suggestion);
  EXPECT_FALSE(
      test_api(*controller_).current_url_filter_suggestion().has_value());
}

// === Group 3: Clear & Dismissal ===

TEST_F(FilterUiControllerTest, ClearSuggestion) {
  GURL url("https://example.com");
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(url, DefaultAttributes());

  controller_->OnSuggestionGenerated(suggestion);

  controller_->FilterUiController::ClearSuggestion();

  // Verify that the current suggestion is reset.
  EXPECT_CALL(*controller_, NavigateTo(_)).Times(0);
  controller_->ApplySuggestion();
}

TEST_F(FilterUiControllerTest, ClearSuggestionHidesPageAction) {
  testing::NiceMock<page_actions::MockPageActionController> mock_controller;
  test_api(*controller_).set_page_action_controller(&mock_controller);

  EXPECT_CALL(mock_favicon_service_, GetFaviconImageForPageURL(_, _, _))
      .WillOnce(Return(base::CancelableTaskTracker::TaskId()));

  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  suggestion.suggestion_message = u"Test Message";

  // Generate suggestion to set up state and show cue.
  EXPECT_CALL(mock_controller, Show(kActionMultistepFilter)).Times(1);
  controller_->OnSuggestionGenerated(suggestion);

  favicon_base::FaviconImageResult result;
  // Manually invoke the callback to simulate successful async favicon returns
  // in a synthetic test environment.
  test_api(*controller_).OnFaviconAvailable(suggestion, result);

  // Now clear suggestion and verify it hides the cue.
  EXPECT_CALL(mock_controller, Hide(kActionMultistepFilter)).Times(1);
  EXPECT_CALL(mock_controller, HideAnchoredMessage(kActionMultistepFilter))
      .Times(1);

  controller_->FilterUiController::ClearSuggestion();
}

// === Group 4: Apply Suggestion & Navigation ===

TEST_F(FilterUiControllerTest, ApplySuggestion) {
  // Should do nothing if there's no suggestion.
  EXPECT_CALL(*controller_, NavigateTo(_)).Times(0);
  controller_->ApplySuggestion();

  // Should do nothing if the URL is empty.
  UrlFilterSuggestion empty_url_suggestion =
      CreateDummySuggestion(GURL(), DefaultAttributes());
  controller_->OnSuggestionGenerated(empty_url_suggestion);
  controller_->ApplySuggestion();

  // Should navigate to the suggestion URL.
  GURL url("https://example.com");
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(url, DefaultAttributes());
  controller_->OnSuggestionGenerated(suggestion);

  EXPECT_CALL(*controller_, NavigateTo(url));
  controller_->ApplySuggestion();
}

TEST_F(FilterUiControllerTest, NavigateToWithNullWebContentsDoesNotCrash) {
  ui::UnownedUserDataHost null_contents_host;
  auto mock_tab_null_contents = CreateMockTab(nullptr, null_contents_host);

  // Instantiating FilterUiController with a null WebContents should not crash.
  auto controller_null_contents =
      std::make_unique<TestFilterUiController>(*mock_tab_null_contents);

  // Should not crash.
  controller_null_contents->NavigateTo(GURL("https://example.com"));
}

TEST_F(FilterUiControllerTest, NavigateToWithWebContents) {
  // Destroy the controller created in SetUp() so we can create a new one
  // on the same tab without hitting a reinsertion check.
  controller_.reset();

  auto controller = std::make_unique<TestFilterUiController>(*mock_tab_);

  MockWebContentsDelegate delegate;
  web_contents()->SetDelegate(&delegate);

  GURL url("https://example.com");

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

  controller->NavigateTo(url);
}

// === Group 5: Commands (ExecuteCommand) ===

TEST_F(FilterUiControllerTest, ExecuteCommandDismissClearsSuggestion) {
  EXPECT_CALL(*controller_, ClearSuggestion()).Times(1);
  controller_->ExecuteCommand(internal::kDismissCommand, 0);
}

TEST_F(FilterUiControllerTest, ExecuteCommandSettingsClearsSuggestion) {
  EXPECT_CALL(*controller_, ClearSuggestion()).Times(1);
  controller_->ExecuteCommand(internal::kSettingsCommand, 0);
}

TEST_F(FilterUiControllerTest, ExecuteCommandWithNullWebContents) {
  EXPECT_CALL(*mock_tab_, GetContents()).WillOnce(Return(nullptr));

  EXPECT_CALL(*controller_, ClearSuggestion()).Times(1);

  // Should not crash when attempting to open settings.
  controller_->ExecuteCommand(internal::kSettingsCommand, 0);
}

// === Group 7: Action Invocation (OnActionInvoked) ===

TEST_F(FilterUiControllerTest,
       OnActionInvokedAppliesSuggestionWhenBubbleShowing) {
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  controller_->OnSuggestionGenerated(suggestion);

  // Simulate bubble showing!
  EXPECT_CALL(*mock_page_action_controller_, GetActiveAnchoredMessage())
      .WillRepeatedly(Return(kActionMultistepFilter));

  EXPECT_CALL(*controller_, NavigateTo(_)).Times(1);
  controller_->OnActionInvoked();
}

TEST_F(FilterUiControllerTest, OnActionInvokedShowsCueWhenBubbleNotShowing) {
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  controller_->OnSuggestionGenerated(suggestion);

  // Simulate bubble NOT showing!
  EXPECT_CALL(*mock_page_action_controller_, GetActiveAnchoredMessage())
      .WillRepeatedly(Return(std::nullopt));

  // ShowCue is not virtual, so we verify that it does NOT call NavigateTo!
  EXPECT_CALL(*controller_, NavigateTo(_)).Times(0);

  controller_->OnActionInvoked();
}

}  // namespace

}  // namespace multistep_filter
