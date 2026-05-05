// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/multistep_filter/content/filter_initiated_navigation_marker.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/strings/grit/ui_strings.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

constexpr int64_t kTestNavigationId = 0;

class MockFilterUiController : public FilterUiController {
 public:
  explicit MockFilterUiController(tabs::TabInterface& tab)
      : FilterUiController(tab) {}
  ~MockFilterUiController() override = default;

  MOCK_METHOD(void,
              ShowSuggestionUi,
              (const UrlFilterSuggestion& suggestion),
              (override));
  MOCK_METHOD(void, NavigateTo, (const GURL& url), (override));

  using FilterUiController::GetOnDismissedCallback;
};

class TestFilterUiController : public FilterUiController {
 public:
  explicit TestFilterUiController(tabs::TabInterface& tab)
      : FilterUiController(tab) {}
  ~TestFilterUiController() override = default;

  // Expose protected methods for testing
  using FilterUiController::NavigateTo;
  using FilterUiController::OnSuggestionGenerated;
  using FilterUiController::ShowSuggestionUi;
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
  return UrlFilterSuggestion(url, base::UTF8ToUTF16(GetEtldPlusOne(url)),
                             base::Time::Now(), std::move(attribute_ui_labels),
                             kTestNavigationId, GetEtldPlusOne(url));
}

class FilterUiControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL("about:blank"));
    mock_tab_ = std::make_unique<tabs::MockTabInterface>();
    ON_CALL(*mock_tab_, GetContents())
        .WillByDefault(testing::Return(web_contents()));
    ON_CALL(*mock_tab_, GetProfile()).WillByDefault(testing::Return(profile()));
    ON_CALL(*mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));
    controller_ = std::make_unique<MockFilterUiController>(*mock_tab_);
  }

  void TearDown() override {
    controller_.reset();
    mock_tab_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<tabs::MockTabInterface> CreateMockTab(
      content::WebContents* contents,
      ui::UnownedUserDataHost& host) {
    auto tab = std::make_unique<tabs::MockTabInterface>();
    ON_CALL(*tab, GetContents()).WillByDefault(testing::Return(contents));
    ON_CALL(*tab, GetProfile()).WillByDefault(testing::Return(profile()));
    ON_CALL(*tab, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(host));
    return tab;
  }

 protected:
  base::test::ScopedFeatureList feature_list_{kMultistepFilter};
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<tabs::MockTabInterface> mock_tab_;
  std::unique_ptr<MockFilterUiController> controller_;
};

TEST_F(FilterUiControllerTest, FromReturnsInstance) {
  EXPECT_EQ(FilterUiController::From(mock_tab_.get()), controller_.get());
}

TEST_F(FilterUiControllerTest, FromReturnsNullIfNotFound) {
  controller_.reset();
  EXPECT_EQ(FilterUiController::From(mock_tab_.get()), nullptr);
}

TEST_F(FilterUiControllerTest, SuggestionCallbackGeneratesSuggestion) {
  GURL url("https://example.com");
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(url, DefaultAttributes());

  EXPECT_CALL(*controller_, ShowSuggestionUi(testing::Eq(suggestion)));
  controller_->OnSuggestionGenerated(suggestion);
}

TEST_F(FilterUiControllerTest, SuggestionCallbackIgnoresNullopt) {
  // Also verify that direct calls with nullopt are ignored.
  EXPECT_CALL(*controller_, ShowSuggestionUi(testing::_)).Times(0);
  controller_->OnSuggestionGenerated(std::nullopt);
}

TEST_F(FilterUiControllerTest, ClearSuggestion) {
  GURL url("https://example.com");
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(url, DefaultAttributes());

  EXPECT_CALL(*controller_, ShowSuggestionUi(testing::Eq(suggestion)));
  controller_->OnSuggestionGenerated(suggestion);

  controller_->ClearSuggestion();

  // Verify that the current suggestion is reset.
  EXPECT_CALL(*controller_, NavigateTo(testing::_)).Times(0);
  controller_->ApplySuggestion();
}

TEST_F(FilterUiControllerTest, ShowSuggestionUiWithNullBrowserWindowInterface) {
  // Destroy the mocked controller created in SetUp()
  controller_.reset();

  auto controller = std::make_unique<TestFilterUiController>(*mock_tab_);
  EXPECT_CALL(*mock_tab_, GetBrowserWindowInterface())
      .WillOnce(testing::Return(nullptr));

  // Should not crash.
  controller->OnSuggestionGenerated(
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes()));
}

TEST_F(FilterUiControllerTest, ShouldSuppressSuggestionsForDismissedHost) {
  GURL source_url("https://example.com/source");
  GURL target_url("https://example.com/target");

  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(target_url, DefaultAttributes());

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(source_url);

  // Set the current suggestion.
  EXPECT_CALL(*controller_, ShowSuggestionUi(testing::Eq(suggestion)));
  controller_->OnSuggestionGenerated(suggestion);

  // 1. Trigger suggestion and simulate dismissal for the source URL.
  auto callback = controller_->GetOnDismissedCallback(
      GetEtldPlusOne(source_url), kTestNavigationId,
      GetEtldPlusOne(target_url));
  std::move(callback).Run();

  // 2. Verify subsequent suggestion for same host is suppressed.
  EXPECT_TRUE(controller_->ShouldSuppressSuggestions(source_url));

  // 3. Directly calling OnSuggestionGenerated should return early without
  // showing UI.
  EXPECT_CALL(*controller_, ShowSuggestionUi(testing::_)).Times(0);
  controller_->OnSuggestionGenerated(suggestion);
}

TEST_F(FilterUiControllerTest, ShouldSuppressSuggestionsForEtldPlusOne) {
  GURL source_url("https://sub.example.com/source");
  GURL other_url("https://other.example.com/other");
  GURL target_url("https://target.com");
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(target_url, DefaultAttributes());

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(source_url);

  // Set the current suggestion.
  EXPECT_CALL(*controller_, ShowSuggestionUi(testing::Eq(suggestion)));
  controller_->OnSuggestionGenerated(suggestion);

  // 1. Trigger suggestion for sub.example.com and simulate dismissal for source
  // site.
  auto callback = controller_->GetOnDismissedCallback(
      GetEtldPlusOne(source_url), kTestNavigationId,
      GetEtldPlusOne(target_url));
  std::move(callback).Run();

  // 2. Verify subsequent suggestion for other.example.com is suppressed.
  EXPECT_TRUE(controller_->ShouldSuppressSuggestions(other_url));

  // 3. Directly calling OnSuggestionGenerated should return early.
  EXPECT_CALL(*controller_, ShowSuggestionUi(testing::_)).Times(0);
  controller_->OnSuggestionGenerated(suggestion);
}

TEST_F(FilterUiControllerTest, ShouldSuppressSuggestionsForLocalhost) {
  GURL source_url("http://localhost:8080/source");
  GURL target_url("http://localhost:8080/target");
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(target_url, DefaultAttributes());

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(source_url);

  // Set the current suggestion.
  EXPECT_CALL(*controller_, ShowSuggestionUi(testing::Eq(suggestion)));
  controller_->OnSuggestionGenerated(suggestion);

  // Simulate dismissal.
  base::OnceClosure callback = controller_->GetOnDismissedCallback(
      GetEtldPlusOne(source_url), kTestNavigationId,
      GetEtldPlusOne(target_url));
  std::move(callback).Run();

  // Verify suppression works for localhost (host fallback).
  EXPECT_TRUE(controller_->ShouldSuppressSuggestions(source_url));
}

TEST_F(FilterUiControllerTest, DismissalDoesNotClearNewSuggestion) {
  GURL url_a("https://a.com");
  UrlFilterSuggestion suggestion_a =
      CreateDummySuggestion(url_a, DefaultAttributes());
  GURL url_b("https://b.com");
  UrlFilterSuggestion suggestion_b =
      CreateDummySuggestion(url_b, DefaultAttributes());

  // 1. Suggestion A is generated.
  EXPECT_CALL(*controller_, ShowSuggestionUi(testing::Eq(suggestion_a)));
  controller_->OnSuggestionGenerated(suggestion_a);
  base::OnceClosure callback_a = controller_->GetOnDismissedCallback(
      GetEtldPlusOne(url_a), kTestNavigationId, GetEtldPlusOne(url_a));

  // 2. Suggestion B is generated (preempts A).
  EXPECT_CALL(*controller_, ShowSuggestionUi(testing::Eq(suggestion_b)));
  controller_->OnSuggestionGenerated(suggestion_b);

  // 3. Dismissal callback for A runs.
  std::move(callback_a).Run();

  // 4. Verify suggestion B is NOT cleared.
  EXPECT_CALL(*controller_, NavigateTo(url_b));
  controller_->ApplySuggestion();
}

TEST_F(FilterUiControllerTest, ApplySuggestion) {
  // Should do nothing if there's no suggestion.
  EXPECT_CALL(*controller_, NavigateTo(testing::_)).Times(0);
  controller_->ApplySuggestion();

  // Should do nothing if the URL is empty.
  UrlFilterSuggestion empty_url_suggestion =
      CreateDummySuggestion(GURL(), DefaultAttributes());
  EXPECT_CALL(*controller_,
              ShowSuggestionUi(testing::Eq(empty_url_suggestion)));
  controller_->OnSuggestionGenerated(empty_url_suggestion);
  controller_->ApplySuggestion();

  // Should navigate to the suggestion URL.
  GURL url("https://example.com");
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(url, DefaultAttributes());
  EXPECT_CALL(*controller_, ShowSuggestionUi(testing::Eq(suggestion)));
  controller_->OnSuggestionGenerated(suggestion);

  EXPECT_CALL(*controller_, NavigateTo(url));
  controller_->ApplySuggestion();
}

TEST_F(FilterUiControllerTest, NullWebContentsDoesNotCrash) {
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

  EXPECT_CALL(delegate,
              OpenURLFromTab(web_contents(),
                             testing::Field(&content::OpenURLParams::url, url),
                             testing::_))
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

TEST_F(FilterUiControllerTest, ShouldSuppressSubsumedSuggestions) {
  GURL current_url("https://example.com/search?q=foo&filter=bar");
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(current_url);

  // 1. Identical URL should be suppressed.
  EXPECT_CALL(*controller_, ShowSuggestionUi(testing::_)).Times(0);
  controller_->OnSuggestionGenerated(
      CreateDummySuggestion(current_url, DefaultAttributes()));

  // 2. Subset of parameters should be suppressed.
  GURL redundant_url("https://example.com/search?q=foo");
  controller_->OnSuggestionGenerated(
      CreateDummySuggestion(redundant_url, DefaultAttributes()));

  // 3. Different base URL should NOT be suppressed even if parameters match.
  GURL different_base_url("https://example.com/other?q=foo&filter=bar");
  UrlFilterSuggestion different_base_suggestion =
      CreateDummySuggestion(different_base_url, DefaultAttributes());
  EXPECT_CALL(*controller_,
              ShowSuggestionUi(testing::Eq(different_base_suggestion)));
  controller_->OnSuggestionGenerated(different_base_suggestion);

  // 4. Additional parameters should NOT be suppressed.
  GURL new_filter_url("https://example.com/search?q=foo&filter=bar&sort=new");
  UrlFilterSuggestion new_filter_suggestion =
      CreateDummySuggestion(new_filter_url, DefaultAttributes());
  EXPECT_CALL(*controller_,
              ShowSuggestionUi(testing::Eq(new_filter_suggestion)));
  controller_->OnSuggestionGenerated(new_filter_suggestion);

  // 5. Different parameter values should NOT be suppressed.
  GURL different_value_url("https://example.com/search?q=baz&filter=bar");
  UrlFilterSuggestion different_value_suggestion =
      CreateDummySuggestion(different_value_url, DefaultAttributes());
  EXPECT_CALL(*controller_,
              ShowSuggestionUi(testing::Eq(different_value_suggestion)));
  controller_->OnSuggestionGenerated(different_value_suggestion);
}

TEST_F(FilterUiControllerTest, GetSuggestionUiData_Recent) {
  base::Time now = base::Time::Now();
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  suggestion.extraction_timestamp = now - base::Hours(2);
  suggestion.source_domain = u"example.com";

  FilterUiController::SuggestionUiData data =
      controller_->GetSuggestionUiData(suggestion, now);

  EXPECT_EQ(data.toast_id, ToastId::kMultistepFilterSuggestionRecent);
  ASSERT_EQ(data.replacement_params.size(), 3u);
  EXPECT_EQ(data.replacement_params[0], u"2");
  EXPECT_EQ(data.replacement_params[1], u"example.com");
  EXPECT_EQ(data.replacement_params[2], u"red, large");
}

TEST_F(FilterUiControllerTest, GetSuggestionUiData_Days) {
  base::Time now = base::Time::Now();
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());
  suggestion.extraction_timestamp = now - base::Days(5);

  FilterUiController::SuggestionUiData data =
      controller_->GetSuggestionUiData(suggestion, now);

  EXPECT_EQ(data.toast_id, ToastId::kMultistepFilterSuggestion);
  ASSERT_EQ(data.replacement_params.size(), 3u);
  EXPECT_EQ(data.replacement_params[0], u"2");
  EXPECT_EQ(data.replacement_params[1],
            l10n_util::GetPluralStringFUTF16(IDS_TIME_DAYS, 5));
  EXPECT_EQ(data.replacement_params[2], u"red, large");
}

TEST_F(FilterUiControllerTest, GetSuggestionUiData_Months) {
  base::Time now = base::Time::Now();
  UrlFilterSuggestion suggestion =
      CreateDummySuggestion(GURL("https://example.com"), DefaultAttributes());

  // 44 days should round down to 1 month.
  suggestion.extraction_timestamp = now - base::Days(44);
  FilterUiController::SuggestionUiData data_44 =
      controller_->GetSuggestionUiData(suggestion, now);
  EXPECT_EQ(data_44.replacement_params[1],
            l10n_util::GetPluralStringFUTF16(IDS_TIME_MONTHS, 1));

  // 45 days should round up to 2 months.
  suggestion.extraction_timestamp = now - base::Days(45);
  FilterUiController::SuggestionUiData data_45 =
      controller_->GetSuggestionUiData(suggestion, now);
  EXPECT_EQ(data_45.replacement_params[1],
            l10n_util::GetPluralStringFUTF16(IDS_TIME_MONTHS, 2));

  // 75 days should round up to 3 months.
  suggestion.extraction_timestamp = now - base::Days(75);
  FilterUiController::SuggestionUiData data_75 =
      controller_->GetSuggestionUiData(suggestion, now);
  EXPECT_EQ(data_75.replacement_params[1],
            l10n_util::GetPluralStringFUTF16(IDS_TIME_MONTHS, 3));
}

}  // namespace

}  // namespace multistep_filter
