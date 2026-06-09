// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multistep_filter/chrome_filter_navigation_observer.h"

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_move_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/multistep_filter/core/multistep_filter_service_factory.h"
#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/multistep_filter/core/annotation_index/mock_annotation_index_client.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

using ::testing::_;

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

class MockMultistepFilterService : public MultistepFilterService {
 public:
  MockMultistepFilterService(
      std::unique_ptr<AnnotationIndexClient> annotation_index_client,
      std::unique_ptr<FilterStore> filter_store)
      : MultistepFilterService([&]() {
          MultistepFilterService::Params params;
          params.annotation_index_client = std::move(annotation_index_client);
          params.filter_store = std::move(filter_store);
          params.identity_manager = nullptr;
          params.consent_helper = nullptr;
          params.log_router = nullptr;
          return params;
        }()) {}

  MOCK_METHOD(void,
              ExtractAnnotation,
              (int64_t navigation_id, const GURL& url),
              (override));
  MOCK_METHOD(
      void,
      GenerateFilterSuggestions,
      (int64_t navigation_id,
       const GURL& url,
       base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback),
      (override));
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
              testing::NiceMock<MockMultistepFilterService>>(
              std::make_unique<MockAnnotationIndexClient>(),
              std::make_unique<FilterStore>());
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

  base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>
  NavigateAndGetCallback(const GURL& url) {
    base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>
        captured_callback;
    EXPECT_CALL(*mock_service(), GenerateFilterSuggestions(_, url, _))
        .WillOnce(MoveArg<2>(&captured_callback));
    auto simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
    simulator->SetHasUserGesture(true);
    simulator->Commit();
    return captured_callback;
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

TEST_F(ChromeFilterNavigationObserverTest, SameDocumentNavigation) {
  auto mock_controller =
      std::make_unique<testing::NiceMock<MockFilterUiController>>(*mock_tab_);

  const GURL url("https://www.example.com");
  NavigateAndGetCallback(url);

  // Subsequent same-document navigation should NOT clear the suggestion in the
  // controller.
  const GURL same_doc_url("https://www.example.com/#test");
  EXPECT_CALL(*mock_controller, ClearSuggestion(_)).Times(0);
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      same_doc_url, main_rfh());
  navigation->CommitSameDocument();
}

TEST_F(ChromeFilterNavigationObserverTest, NavigationClearsSuggestion) {
  auto mock_controller =
      std::make_unique<testing::NiceMock<MockFilterUiController>>(*mock_tab_);

  NavigateAndGetCallback(GURL("https://www.example.com"));

  // Navigate to a new URL, which should trigger ClearSuggestion on the
  // delegate.
  EXPECT_CALL(
      *mock_controller,
      ClearSuggestion(FilterUiController::SuggestionUserDecision::kIgnored));
  EXPECT_CALL(*mock_service(), GenerateFilterSuggestions(
                                   _, GURL("https://www.example2.com"), _));
  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://www.example2.com"), main_rfh());
  simulator->SetHasUserGesture(true);
  simulator->Commit();
}

TEST_F(ChromeFilterNavigationObserverTest, DelegateOnSuggestionGenerated) {
  auto mock_controller =
      std::make_unique<testing::NiceMock<MockFilterUiController>>(*mock_tab_);

  base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>
      captured_callback =
          NavigateAndGetCallback(GURL("https://www.example.com"));

  ASSERT_TRUE(captured_callback);

  const GURL suggestion_url("https://suggestion.com");
  UrlFilterSuggestion suggestion(UrlFilterSuggestion::Params{
      .navigation_url = suggestion_url,
      .source_domain = base::UTF8ToUTF16(GetEtldPlusOne(suggestion_url)),
      .extraction_timestamp = base::Time::Now(),
      .attribute_ui_labels = {},
      .triggering_navigation_id = kTestNavigationId,
      .triggering_domain = GetEtldPlusOne(suggestion_url),
      .task_type = "task1"});
  EXPECT_CALL(*mock_controller,
              OnSuggestionGenerated(testing::Optional(suggestion)));
  std::move(captured_callback).Run(suggestion);
}

TEST_F(ChromeFilterNavigationObserverTest, DelegateHandlesNullController) {
  base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>
      captured_callback =
          NavigateAndGetCallback(GURL("https://www.example.com"));

  ASSERT_TRUE(captured_callback);

  // No controller is attached to the tab, so calls should be gracefully
  // handled without crashing.
  std::move(captured_callback).Run(std::nullopt);
}

TEST_F(ChromeFilterNavigationObserverTest, NavigationWithController) {
  std::optional<FilterUiController> filter_ui_controller;
  filter_ui_controller.emplace(*mock_tab_);

  const GURL url("https://www.example.com");
  EXPECT_CALL(*mock_service(), ExtractAnnotation(_, url));
  NavigateAndGetCallback(url);
}

TEST_F(ChromeFilterNavigationObserverTest, NavigationWithNullController) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(*mock_service(), ExtractAnnotation(_, url));
  NavigateAndGetCallback(url);
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
