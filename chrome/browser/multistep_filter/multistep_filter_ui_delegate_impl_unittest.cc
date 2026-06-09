// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multistep_filter/multistep_filter_ui_delegate_impl.h"

#include <memory>

#include "base/time/time.h"
#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
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
              OnSuggestionGenerated,
              (std::optional<UrlFilterSuggestion> suggestion),
              (override));
  MOCK_METHOD(void, ClearSuggestion, (SuggestionUserDecision), (override));
};

class MultistepFilterUiDelegateImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    mock_tab_ = std::make_unique<tabs::MockTabInterface>();
    ON_CALL(*mock_tab_, GetContents())
        .WillByDefault(testing::Return(web_contents()));
    ON_CALL(*mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(user_data_host_));

    delegate_ = std::make_unique<MultistepFilterUiDelegateImpl>(*mock_tab_);
  }

  void TearDown() override {
    delegate_.reset();
    mock_tab_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<tabs::MockTabInterface> mock_tab_;
  ui::UnownedUserDataHost user_data_host_;
  std::unique_ptr<MultistepFilterUiDelegateImpl> delegate_;
};

TEST_F(MultistepFilterUiDelegateImplTest, ClearSuggestion_WithController) {
  auto mock_controller =
      std::make_unique<testing::NiceMock<MockFilterUiController>>(*mock_tab_);

  EXPECT_CALL(
      *mock_controller,
      ClearSuggestion(FilterUiController::SuggestionUserDecision::kIgnored));
  delegate_->ClearSuggestion();
}

TEST_F(MultistepFilterUiDelegateImplTest, ClearSuggestion_WithoutController) {
  // Should not crash when there is no controller.
  delegate_->ClearSuggestion();
}

TEST_F(MultistepFilterUiDelegateImplTest,
       OnSuggestionGenerated_WithController) {
  auto mock_controller =
      std::make_unique<testing::NiceMock<MockFilterUiController>>(*mock_tab_);

  const GURL suggestion_url("https://suggestion.com");
  UrlFilterSuggestion suggestion(
      UrlFilterSuggestion::Params{.navigation_url = suggestion_url,
                                  .source_domain = u"suggestion.com",
                                  .extraction_timestamp = base::Time::Now(),
                                  .attribute_ui_labels = {},
                                  .triggering_navigation_id = kTestNavigationId,
                                  .triggering_domain = "suggestion.com",
                                  .task_type = "task1"});

  EXPECT_CALL(*mock_controller,
              OnSuggestionGenerated(testing::Optional(suggestion)));
  delegate_->OnSuggestionGenerated(suggestion);
}

TEST_F(MultistepFilterUiDelegateImplTest,
       OnSuggestionGenerated_WithoutController) {
  const GURL suggestion_url("https://suggestion.com");
  UrlFilterSuggestion suggestion(
      UrlFilterSuggestion::Params{.navigation_url = suggestion_url,
                                  .source_domain = u"suggestion.com",
                                  .extraction_timestamp = base::Time::Now(),
                                  .attribute_ui_labels = {},
                                  .triggering_navigation_id = kTestNavigationId,
                                  .triggering_domain = "suggestion.com",
                                  .task_type = "task1"});
  // Should not crash when there is no controller.
  delegate_->OnSuggestionGenerated(suggestion);
}

TEST_F(MultistepFilterUiDelegateImplTest, GetWeakPtr) {
  base::WeakPtr<MultistepFilterUiDelegate> weak_ptr = delegate_->GetWeakPtr();
  EXPECT_TRUE(weak_ptr);

  delegate_.reset();
  EXPECT_FALSE(weak_ptr);
}

TEST_F(MultistepFilterUiDelegateImplTest, ClearSuggestion_InvalidatesWeakPtr) {
  base::WeakPtr<MultistepFilterUiDelegate> weak_ptr = delegate_->GetWeakPtr();
  EXPECT_TRUE(weak_ptr);
  delegate_->ClearSuggestion();
  EXPECT_FALSE(weak_ptr);
}

}  // namespace

}  // namespace multistep_filter
