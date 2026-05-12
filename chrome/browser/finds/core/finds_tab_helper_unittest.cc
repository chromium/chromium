// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/core/finds_tab_helper.h"

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/finds/core/finds_features.h"
#include "chrome/browser/finds/core/finds_pref_names.h"
#include "chrome/browser/finds/core/finds_service.h"
#include "chrome/browser/finds/finds_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace finds {

class MockFindsServiceObserver : public FindsService::Observer {
 public:
  MockFindsServiceObserver() = default;
  MOCK_METHOD(void, OnOptInCriteriaFulfilled, (), (override));
};

class FindsTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  FindsTabHelperTest() {
    scoped_feature_list_.InitAndEnableFeature(finds::features::kChromeFinds);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Ensure that all unsupported platforms are skipped.
    if (!FindsTabHelper::IsSupportedPlatform()) {
      GTEST_SKIP() << "Unsupported platform for FindsTabHelper";
    }

    SyncServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::TestSyncService>();
        }));

    auto* sync_service = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile()));
    sync_service->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kHistory, true);

    profile()->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    template_url_service_ = TemplateURLServiceFactory::GetForProfile(profile());

    FindsTabHelper::CreateForWebContents(
        web_contents(), finds::FindsServiceFactory::GetForProfile(profile()),
        /*opt_guide_service=*/nullptr, template_url_service_,
        profile()->GetPrefs());
    tab_helper_ = FindsTabHelper::FromWebContents(web_contents());

    finds_service_ = FindsServiceFactory::GetForProfile(profile());
    ASSERT_NE(finds_service_, nullptr);
    finds_service_->AddObserver(&finds_service_observer_);
    srp_return_count_threshold_ =
        finds::features::kSRPReturnCountThreshold.Get();
  }

  void TearDown() override {
    if (finds_service_) {
      finds_service_->RemoveObserver(&finds_service_observer_);
    }
    finds_service_ = nullptr;
    tab_helper_ = nullptr;
    template_url_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void CallDidFinishNavigation(content::NavigationHandle* handle) {
    static_cast<content::WebContentsObserver*>(tab_helper_)
        ->DidFinishNavigation(handle);
  }

  // Configures a minimal mock state that satisfies the IsValidNavigation check
  // required to enter the tab helper's logic.
  std::unique_ptr<content::MockNavigationHandle> CreateMockNavigationHandle(
      const GURL& url,
      ui::PageTransition transition) {
    auto handle = std::make_unique<content::MockNavigationHandle>(
        url, web_contents()->GetPrimaryMainFrame());
    handle->set_has_committed(true);
    handle->set_is_in_primary_main_frame(true);
    handle->set_is_same_document(false);
    handle->set_page_transition(transition);
    return handle;
  }

  int srp_return_count() const { return tab_helper_->srp_return_count_; }

  // Runs a sequence of navigations to simulate a user hitting the trigger
  // threshold. This is used to ensure preferences override a state that would
  // otherwise successfully trigger.
  void SimulateSRPBackNavigations(int count) {
    GURL srp_url("https://www.google.com/search?q=test");
    for (int i = 0; i < count; ++i) {
      auto handle = CreateMockNavigationHandle(
          srp_url,
          static_cast<ui::PageTransition>(ui::PAGE_TRANSITION_FORWARD_BACK));
      CallDidFinishNavigation(handle.get());
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  MockFindsServiceObserver finds_service_observer_;
  raw_ptr<TemplateURLService> template_url_service_ = nullptr;
  raw_ptr<FindsService> finds_service_ = nullptr;
  raw_ptr<FindsTabHelper> tab_helper_ = nullptr;
  int srp_return_count_threshold_ = 0;
};

TEST_F(FindsTabHelperTest, TestSRPBackNavigationThresholdMet) {
  EXPECT_CALL(finds_service_observer_, OnOptInCriteriaFulfilled()).Times(1);
  SimulateSRPBackNavigations(srp_return_count_threshold_);
  EXPECT_EQ(srp_return_count(), srp_return_count_threshold_);
}

TEST_F(FindsTabHelperTest, TestNonForwardBackNavToSRPDoesNotCount) {
  GURL srp_url("https://www.google.com/search?q=test");
  // Normal navigation (TYPED) to SRP does not count.
  auto handle = CreateMockNavigationHandle(srp_url, ui::PAGE_TRANSITION_TYPED);
  CallDidFinishNavigation(handle.get());
  EXPECT_EQ(srp_return_count(), 0);
}

TEST_F(FindsTabHelperTest, TestOptInInteracted) {
  profile()->GetPrefs()->SetBoolean(prefs::kFindsOptInPromoUserInteracted,
                                    true);
  EXPECT_CALL(finds_service_observer_, OnOptInCriteriaFulfilled()).Times(0);
  SimulateSRPBackNavigations(srp_return_count_threshold_);
  EXPECT_EQ(srp_return_count(), 0);
}

TEST_F(FindsTabHelperTest, TestOptInMaxCountExceeded) {
  profile()->GetPrefs()->SetInteger(prefs::kFindsOptInPromoShownCount, 100);
  EXPECT_CALL(finds_service_observer_, OnOptInCriteriaFulfilled()).Times(0);
  SimulateSRPBackNavigations(srp_return_count_threshold_);
  EXPECT_EQ(srp_return_count(), 0);
}

TEST_F(FindsTabHelperTest, TestOptInCooldownNotPassed) {
  profile()->GetPrefs()->SetInt64(
      prefs::kFindsOptInPromoLastShownTimestamp,
      (base::Time::Now() + base::Days(1)).InMillisecondsSinceUnixEpoch());
  EXPECT_CALL(finds_service_observer_, OnOptInCriteriaFulfilled()).Times(0);
  SimulateSRPBackNavigations(srp_return_count_threshold_);
  EXPECT_EQ(srp_return_count(), 0);
}

TEST_F(FindsTabHelperTest, TestEnterprisePolicyDisabled) {
  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kFindsEnterprisePolicyAllowed,
      static_cast<int>(optimization_guide::model_execution::prefs::
                           ModelExecutionEnterprisePolicyValue::kDisable));
  EXPECT_CALL(finds_service_observer_, OnOptInCriteriaFulfilled()).Times(0);
  SimulateSRPBackNavigations(srp_return_count_threshold_);
  EXPECT_EQ(srp_return_count(), 0);
}
TEST_F(FindsTabHelperTest, TestMSBBDisabled) {
  profile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);
  EXPECT_CALL(finds_service_observer_, OnOptInCriteriaFulfilled()).Times(0);
  SimulateSRPBackNavigations(srp_return_count_threshold_);
  EXPECT_EQ(srp_return_count(), 0);
}

TEST_F(FindsTabHelperTest, TestHistorySyncDisabled) {
  auto* sync_service = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetForProfile(profile()));
  sync_service->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, false);
  sync_service->FireStateChanged();

  EXPECT_CALL(finds_service_observer_, OnOptInCriteriaFulfilled()).Times(0);
  SimulateSRPBackNavigations(srp_return_count_threshold_);
  EXPECT_EQ(srp_return_count(), 0);
}

}  // namespace finds
