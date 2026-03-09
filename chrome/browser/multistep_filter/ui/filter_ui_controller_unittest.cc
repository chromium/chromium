// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/multistep_filter/core/features.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

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

}  // namespace

class FilterUiControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL("about:blank"));
    mock_tab_ = std::make_unique<tabs::MockTabInterface>();
    ON_CALL(*mock_tab_, GetContents())
        .WillByDefault(testing::Return(web_contents()));
    ON_CALL(*mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));
    controller_ = std::make_unique<TestFilterUiController>(*mock_tab_);
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
    ON_CALL(*tab, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(host));
    return tab;
  }

 protected:
  void NotifySuggestionAvailable(const UrlFilterSuggestion& suggestion) {
    controller_->OnSuggestionGenerated(suggestion);
  }

  void NotifySuggestionCleared() { controller_->ClearSuggestion(); }

  base::test::ScopedFeatureList feature_list_{kMultistepFilter};
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<tabs::MockTabInterface> mock_tab_;
  std::unique_ptr<TestFilterUiController> controller_;
};

TEST_F(FilterUiControllerTest, FromReturnsInstance) {
  EXPECT_EQ(FilterUiController::From(mock_tab_.get()), controller_.get());
}

TEST_F(FilterUiControllerTest, FromReturnsNullIfNotFound) {
  controller_.reset();
  EXPECT_EQ(FilterUiController::From(mock_tab_.get()), nullptr);
}

TEST_F(FilterUiControllerTest, ShowSuggestionUiOnSuggestionAvailable) {
  GURL url("https://example.com");
  UrlFilterSuggestion suggestion("Example", url);
  EXPECT_CALL(*controller_, ShowSuggestionUi(testing::Eq(suggestion)));
  NotifySuggestionAvailable(suggestion);
}

TEST_F(FilterUiControllerTest, HideSuggestionUiOnSuggestionCleared) {
  GURL url("https://example.com");
  UrlFilterSuggestion suggestion("Example", url);
  EXPECT_CALL(*controller_, ShowSuggestionUi(testing::Eq(suggestion)));
  NotifySuggestionAvailable(suggestion);

  EXPECT_CALL(*controller_, HideSuggestionUi());
  NotifySuggestionCleared();
}

TEST_F(FilterUiControllerTest, ClearSuggestionDoesNothingIfNoSuggestion) {
  EXPECT_CALL(*controller_, HideSuggestionUi()).Times(0);
  NotifySuggestionCleared();
}

TEST_F(FilterUiControllerTest, ClearSuggestionInvalidatesPendingRequests) {
  auto callback = controller_->GetSuggestionCallback();
  NotifySuggestionCleared();

  // The callback should be invalidated and not trigger UI changes.
  EXPECT_CALL(*controller_, ShowSuggestionUi(testing::_)).Times(0);
  std::move(callback).Run(
      UrlFilterSuggestion("Example", GURL("https://example.com")));
}

TEST_F(FilterUiControllerTest, GetSuggestionCallbackTriggersShowSuggestionUi) {
  GURL url("https://example.com");
  UrlFilterSuggestion suggestion("Example", url);

  auto callback = controller_->GetSuggestionCallback();
  EXPECT_CALL(*controller_, ShowSuggestionUi(testing::Eq(suggestion)));
  std::move(callback).Run(suggestion);
}

TEST_F(FilterUiControllerTest, GetSuggestionCallbackIgnoresNullopt) {
  auto callback = controller_->GetSuggestionCallback();
  EXPECT_CALL(*controller_, ShowSuggestionUi(testing::_)).Times(0);
  std::move(callback).Run(std::nullopt);
}

TEST_F(FilterUiControllerTest, ApplySuggestionDoesNothingIfNoSuggestion) {
  EXPECT_CALL(*controller_, NavigateTo(testing::_)).Times(0);
  controller_->ApplySuggestion();
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
  ui::UnownedUserDataHost null_contents_host;
  auto mock_tab_null_contents = CreateMockTab(nullptr, null_contents_host);

  // Instantiating FilterUiController with a null WebContents should not crash.
  auto controller_null_contents =
      std::make_unique<TestFilterUiController>(*mock_tab_null_contents);
}

TEST_F(FilterUiControllerTest, NavigateToWithNullWebContents) {
  ui::UnownedUserDataHost null_contents_host;
  auto mock_tab_null_contents = CreateMockTab(nullptr, null_contents_host);

  auto controller = std::make_unique<TestFilterUiControllerWithRealNavigate>(
      *mock_tab_null_contents);

  // Should not crash.
  controller->NavigateTo(GURL("https://example.com"));
}

TEST_F(FilterUiControllerTest, NavigateToWithWebContents) {
  // Destroy the controller created in SetUp() so we can create a new one
  // on the same tab without hitting a reinsertion check.
  controller_.reset();

  auto controller =
      std::make_unique<TestFilterUiControllerWithRealNavigate>(*mock_tab_);

  MockWebContentsDelegate delegate;
  web_contents()->SetDelegate(&delegate);

  GURL url("https://example.com");

  EXPECT_CALL(delegate,
              OpenURLFromTab(web_contents(),
                             testing::Field(&content::OpenURLParams::url, url),
                             testing::_))
      .WillOnce(testing::Return(web_contents()));

  controller->NavigateTo(url);
}

}  // namespace multistep_filter
