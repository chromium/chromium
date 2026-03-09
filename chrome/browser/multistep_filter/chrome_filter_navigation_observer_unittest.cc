// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multistep_filter/chrome_filter_navigation_observer.h"

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/multistep_filter/core/multistep_filter_service_factory.h"
#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

class MockMultistepFilterService : public MultistepFilterService {
 public:
  MockMultistepFilterService() : MultistepFilterService(nullptr, nullptr) {}

  MOCK_METHOD(void, GenerateFilterSuggestions, (const GURL& url));

  void GenerateFilterSuggestions(
      const GURL& url,
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback)
      override {
    GenerateFilterSuggestions(url);
  }
};

// Helper class for ChromeFilterNavigationObserverTest.
// Exposes the internal observer for verification.
class ChromeFilterNavigationObserverWrapper
    : public ChromeFilterNavigationObserver {
 public:
  using ChromeFilterNavigationObserver::ChromeFilterNavigationObserver;

  FilterNavigationObserver* observer() { return observer_.get(); }
};

// Verifies the lifecycle management of the internal FilterNavigationObserver
// and tests the real UiDelegateImpl behavior.
class ChromeFilterNavigationObserverTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    MultistepFilterServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              testing::NiceMock<MockMultistepFilterService>>();
        }));

    mock_tab_ = std::make_unique<tabs::MockTabInterface>();
    ON_CALL(*mock_tab_, GetContents())
        .WillByDefault(testing::Return(web_contents()));
    ON_CALL(*mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(user_data_host_));

    chrome_observer_ =
        std::make_unique<ChromeFilterNavigationObserverWrapper>(*mock_tab_);
  }

  void TearDown() override {
    chrome_observer_ = nullptr;
    mock_tab_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  MockMultistepFilterService* mock_service() {
    return static_cast<MockMultistepFilterService*>(
        MultistepFilterServiceFactory::GetForProfile(profile()));
  }

  std::unique_ptr<tabs::MockTabInterface> mock_tab_;
  ui::UnownedUserDataHost user_data_host_;
  std::unique_ptr<ChromeFilterNavigationObserverWrapper> chrome_observer_;
};

TEST_F(ChromeFilterNavigationObserverTest, FromReturnsInstance) {
  EXPECT_EQ(ChromeFilterNavigationObserver::From(mock_tab_.get()),
            chrome_observer_.get());
}

TEST_F(ChromeFilterNavigationObserverTest, FromReturnsNullIfNotFound) {
  chrome_observer_.reset();
  EXPECT_EQ(ChromeFilterNavigationObserver::From(mock_tab_.get()), nullptr);
}

TEST_F(ChromeFilterNavigationObserverTest, HandlesNullWebContents) {
  EXPECT_TRUE(chrome_observer_->observer());

  chrome_observer_->OnDiscardContents(mock_tab_.get(), web_contents(), nullptr);

  EXPECT_FALSE(chrome_observer_->observer());
}

TEST_F(ChromeFilterNavigationObserverTest,
       UpdatesObserverOnDiscardWithRealImpl) {
  std::unique_ptr<content::WebContents> new_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

  chrome_observer_->OnDiscardContents(mock_tab_.get(), web_contents(),
                                      new_contents.get());

  ASSERT_TRUE(chrome_observer_->observer());
  EXPECT_EQ(chrome_observer_->observer()->web_contents(), new_contents.get());
}

TEST_F(ChromeFilterNavigationObserverTest, WebContentsDestruction) {
  DeleteContents();
  EXPECT_TRUE(chrome_observer_);
}

TEST_F(ChromeFilterNavigationObserverTest, NavigationWithController) {
  std::optional<FilterUiController> filter_ui_controller;
  filter_ui_controller.emplace(*mock_tab_);

  const GURL url("https://www.example.com");
  EXPECT_CALL(*mock_service(), GenerateFilterSuggestions(url));

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
}

TEST_F(ChromeFilterNavigationObserverTest, NavigationWithNullController) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(*mock_service(), GenerateFilterSuggestions(url));

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
}

TEST_F(ChromeFilterNavigationObserverTest, HandlesNullService) {
  chrome_observer_.reset();
  MultistepFilterServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(
                     [](content::BrowserContext* context)
                         -> std::unique_ptr<KeyedService> { return nullptr; }));

  chrome_observer_ =
      std::make_unique<ChromeFilterNavigationObserverWrapper>(*mock_tab_);

  std::optional<FilterUiController> filter_ui_controller;
  filter_ui_controller.emplace(*mock_tab_);

  const GURL url("https://www.example.com");
  // We can't mock the service call because it's null, but we verify it doesn't
  // crash.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
}

}  // namespace

}  // namespace multistep_filter
