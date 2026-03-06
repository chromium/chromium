// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/multistep_filter/test_utils/fake_tab_interface.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/multistep_filter/core/features.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {

namespace {

class TestFilterUiController : public FilterUiController {
 public:
  explicit TestFilterUiController(tabs::TabInterface& tab)
      : FilterUiController(tab) {}
  ~TestFilterUiController() override = default;

  MOCK_METHOD(void,
              ShowSuggestionUi,
              (const UrlFilterSuggestion& suggestion),
              (override));
  MOCK_METHOD(void, HideSuggestionUi, (), (override));
  MOCK_METHOD(void, NavigateTo, (const GURL& url), (override));
};

class TestFilterUiControllerWithRealNavigate : public FilterUiController {
 public:
  explicit TestFilterUiControllerWithRealNavigate(tabs::TabInterface& tab)
      : FilterUiController(tab) {}
  ~TestFilterUiControllerWithRealNavigate() override = default;

  MOCK_METHOD(void,
              ShowSuggestionUi,
              (const UrlFilterSuggestion& suggestion),
              (override));
  MOCK_METHOD(void, HideSuggestionUi, (), (override));

  // Expose NavigateTo for testing purposes
  using FilterUiController::NavigateTo;
};

}  // namespace

class FilterUiControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL("about:blank"));
    fake_tab_ = std::make_unique<FakeTabInterface>(profile(), web_contents());
    controller_ = std::make_unique<TestFilterUiController>(*fake_tab_);
  }

  void TearDown() override {
    controller_.reset();
    fake_tab_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  void NotifySuggestionAvailable(const UrlFilterSuggestion& suggestion) {
    controller_->OnSuggestionGenerated(suggestion);
  }

  void NotifySuggestionCleared() { controller_->ClearSuggestion(); }

  base::test::ScopedFeatureList feature_list_{kMultistepFilter};
  std::unique_ptr<FakeTabInterface> fake_tab_;
  std::unique_ptr<TestFilterUiController> controller_;
};

TEST_F(FilterUiControllerTest, ShowSuggestionUiOnSuggestionAvailable) {
  GURL url("https://example.com");
  UrlFilterSuggestion suggestion("Example", url);
  EXPECT_CALL(*controller_, ShowSuggestionUi(testing::Eq(suggestion)));
  NotifySuggestionAvailable(suggestion);
}

TEST_F(FilterUiControllerTest, HideSuggestionUiOnSuggestionCleared) {
  EXPECT_CALL(*controller_, HideSuggestionUi());
  NotifySuggestionCleared();
}

TEST_F(FilterUiControllerTest, ApplySuggestionNavigatesToUrl) {
  GURL url("https://example.com");
  UrlFilterSuggestion suggestion("Example", url);

  EXPECT_CALL(*controller_, ShowSuggestionUi(testing::Eq(suggestion)));
  controller_->OnSuggestionGenerated(suggestion);

  EXPECT_CALL(*controller_, NavigateTo(url));
  controller_->ApplySuggestion();
}

TEST_F(FilterUiControllerTest, ApplySuggestionDoesNothingWhenUrlIsEmpty) {
  UrlFilterSuggestion suggestion("Example", GURL());

  EXPECT_CALL(*controller_, ShowSuggestionUi(testing::Eq(suggestion)));
  controller_->OnSuggestionGenerated(suggestion);

  EXPECT_CALL(*controller_, NavigateTo(testing::_)).Times(0);
  controller_->ApplySuggestion();
}

TEST_F(FilterUiControllerTest, NullWebContentsDoesNotCrash) {
  // Create a FakeTabInterface with a null WebContents.
  auto fake_tab_null_contents =
      std::make_unique<FakeTabInterface>(profile(), nullptr);

  // Instantiating FilterUiController with a null WebContents should not crash.
  auto controller_null_contents =
      std::make_unique<TestFilterUiController>(*fake_tab_null_contents);
}

TEST_F(FilterUiControllerTest, NavigateToWithNullWebContents) {
  // Create a FakeTabInterface with a null WebContents.
  auto fake_tab_null_contents =
      std::make_unique<FakeTabInterface>(profile(), nullptr);

  auto controller = std::make_unique<TestFilterUiControllerWithRealNavigate>(
      *fake_tab_null_contents);

  // Should not crash.
  controller->NavigateTo(GURL("https://example.com"));
}

TEST_F(FilterUiControllerTest, NavigateToWithWebContents) {
  auto controller =
      std::make_unique<TestFilterUiControllerWithRealNavigate>(*fake_tab_);

  GURL url("https://example.com");
  // This should call WebContents::OpenURL. Since we are using a
  // TestWebContents, we just verify it doesn't crash. Verifying the actual
  // navigation in unit tests without mocking WebContents is implementation
  // dependent, but we can check if the navigation controller has a pending
  // entry if TestWebContents::OpenURL is implemented to create one.
  controller->NavigateTo(url);
}

}  // namespace multistep_filter
