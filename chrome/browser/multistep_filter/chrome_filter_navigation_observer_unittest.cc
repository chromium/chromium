// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multistep_filter/chrome_filter_navigation_observer.h"

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_move_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/multistep_filter/chrome_filter_navigation_observer_test_api.h"
#include "chrome/browser/multistep_filter/core/multistep_filter_service_factory.h"
#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/multistep_filter/content/content_filter_navigation_observer_test_api.h"
#include "components/multistep_filter/core/annotation_index/mock_annotation_index_client.h"
#include "components/multistep_filter/core/data_models/filter_navigation_metadata.h"
#include "components/multistep_filter/core/data_models/suggestion_user_decision.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/filter_tab_controller.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/multistep_filter_ui_delegate.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/multistep_filter/core/prefs/multistep_filter_retention_prefs.h"
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
              (std::optional<UrlFilterSuggestion> suggestion,
               MultistepFilterUiDelegate::SuggestionUiCallbacks callbacks),
              (override));
  MOCK_METHOD(void, ClearSuggestion, (SuggestionUserDecision), (override));
};

class MockMultistepFilterService : public MultistepFilterService {
 public:
  MockMultistepFilterService(
      std::unique_ptr<AnnotationIndexClient> annotation_index_client,
      std::unique_ptr<FilterStore> filter_store,
      PrefService* pref_service,
      signin::IdentityManager* identity_manager)
      : MultistepFilterService([&]() {
          MultistepFilterService::Params params;
          params.annotation_index_client = std::move(annotation_index_client);
          params.filter_store = std::move(filter_store);
          params.identity_manager = identity_manager;
          params.pref_service = pref_service;
          params.consent_helper = nullptr;
          params.log_router = nullptr;
          return params;
        }()) {}
  ~MockMultistepFilterService() override = default;
};

// Verifies the lifecycle management of the internal
// ContentFilterNavigationObserver and tests the real UiDelegateImpl behavior.
class ChromeFilterNavigationObserverTest
    : public ChromeRenderViewHostTestHarness {
 public:
  std::unique_ptr<TestingProfile> CreateTestingProfile() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactories(IdentityTestEnvironmentProfileAdaptor::
                                    GetIdentityTestEnvironmentFactories());
    return builder.Build();
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    MultistepFilterServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          Profile* profile = Profile::FromBrowserContext(context);
          return std::make_unique<
              testing::NiceMock<MockMultistepFilterService>>(
              std::make_unique<MockAnnotationIndexClient>(),
              std::make_unique<FilterStore>(), profile->GetPrefs(),
              IdentityManagerFactory::GetForProfile(profile));
        }));

    mock_tab_ = std::make_unique<tabs::MockTabInterface>();
    ON_CALL(*mock_tab_, GetContents())
        .WillByDefault(testing::Return(web_contents()));
    ON_CALL(*mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(user_data_host_));

    chrome_observer_ =
        std::make_unique<ChromeFilterNavigationObserver>(*mock_tab_);
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

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<tabs::MockTabInterface> mock_tab_;
  ui::UnownedUserDataHost user_data_host_;
  std::unique_ptr<ChromeFilterNavigationObserver> chrome_observer_;
};

// Tests that the observer can be retrieved from the TabInterface's user data.
TEST_F(ChromeFilterNavigationObserverTest, FromReturnsInstance) {
  EXPECT_EQ(ChromeFilterNavigationObserver::From(mock_tab_.get()),
            chrome_observer_.get());
}

// Tests that retrieving the observer returns null if it has not been created
// yet.
TEST_F(ChromeFilterNavigationObserverTest, FromReturnsNullIfNotFound) {
  chrome_observer_.reset();
  EXPECT_EQ(ChromeFilterNavigationObserver::From(mock_tab_.get()), nullptr);
}

// Tests that the inner observer is destroyed when the tab discards its
// WebContents.
TEST_F(ChromeFilterNavigationObserverTest, HandlesNullWebContents) {
  EXPECT_TRUE(test_api(*chrome_observer_).GetObserver());

  chrome_observer_->OnDiscardContents(mock_tab_.get(), web_contents(), nullptr);

  EXPECT_FALSE(test_api(*chrome_observer_).GetObserver());
}

// Tests that the inner observer is recreated when the tab is updated with new
// WebContents.
TEST_F(ChromeFilterNavigationObserverTest,
       UpdatesObserverOnDiscardWithRealImpl) {
  std::unique_ptr<content::WebContents> new_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

  chrome_observer_->OnDiscardContents(mock_tab_.get(), web_contents(),
                                      new_contents.get());

  ContentFilterNavigationObserver* observer =
      test_api(*chrome_observer_).GetObserver();
  ASSERT_TRUE(observer);
  EXPECT_EQ(observer->web_contents(), new_contents.get());
}

// Tests that the outer observer survives the destruction of its observed
// WebContents.
TEST_F(ChromeFilterNavigationObserverTest, WebContentsDestruction) {
  DeleteContents();
  EXPECT_TRUE(chrome_observer_);
}

// Tests that the UI delegate correctly forwards generated suggestions to the
// tab's UI controller.
TEST_F(ChromeFilterNavigationObserverTest, DelegateOnSuggestionGenerated) {
  auto mock_controller =
      std::make_unique<testing::NiceMock<MockFilterUiController>>(*mock_tab_);

  ContentFilterNavigationObserver* observer =
      test_api(*chrome_observer_).GetObserver();
  ASSERT_TRUE(observer);
  MultistepFilterUiDelegate* delegate = test_api(*observer).GetDelegate();
  ASSERT_TRUE(delegate);

  const GURL suggestion_url("https://suggestion.com");
  UrlFilterSuggestion suggestion(UrlFilterSuggestion::Params{
      .navigation_url = suggestion_url,
      .source_host = base::UTF8ToUTF16(suggestion_url.GetHost()),
      .extraction_timestamp = base::Time::Now(),
      .attribute_ui_labels = {},
      .triggering_navigation_id = kTestNavigationId,
      .triggering_host = suggestion_url.GetHost(),
      .task_type = "task1"});
  EXPECT_CALL(*mock_controller,
              OnSuggestionGenerated(testing::Optional(suggestion), _));
  delegate->OnSuggestionGenerated(suggestion, {});
}

// Tests that the UI delegate handles a null UI controller on the tab without
// crashing.
TEST_F(ChromeFilterNavigationObserverTest, DelegateHandlesNullUiController) {
  ContentFilterNavigationObserver* observer =
      test_api(*chrome_observer_).GetObserver();
  ASSERT_TRUE(observer);
  MultistepFilterUiDelegate* delegate = test_api(*observer).GetDelegate();
  ASSERT_TRUE(delegate);

  // Verify that the delegate call is handled gracefully without crashing when
  // no UI controller is attached to the tab.
  delegate->OnSuggestionGenerated(std::nullopt, {});
}

// Tests that the observer behaves safely and doesn't crash when the
// MultistepFilterService is null.
TEST_F(ChromeFilterNavigationObserverTest, HandlesNullService) {
  chrome_observer_.reset();
  MultistepFilterServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(
                     [](content::BrowserContext* context)
                         -> std::unique_ptr<KeyedService> { return nullptr; }));

  chrome_observer_ =
      std::make_unique<ChromeFilterNavigationObserver>(*mock_tab_);

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
