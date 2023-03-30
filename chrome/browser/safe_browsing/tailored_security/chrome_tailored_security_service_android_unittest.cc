// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/chrome_tailored_security_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_notification_result.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Names for Tailored Security status to make the test cases clearer.
const bool kTailoredSecurityEnabled = true;

namespace {
// Test implementation of ChromeTailoredSecurityService.
class TestChromeTailoredSecurityService
    : public safe_browsing::ChromeTailoredSecurityService {
 public:
  explicit TestChromeTailoredSecurityService(Profile* profile)
      : ChromeTailoredSecurityService(profile) {}
  ~TestChromeTailoredSecurityService() override = default;

  // We are overriding this method because we don't want to test
  // the calls to the History API
  void TailoredSecurityTimestampUpdateCallback() override {
    ChromeTailoredSecurityService::OnSyncNotificationMessageRequest(
        kTailoredSecurityEnabled);
  }
};
}  // namespace

class ChromeTailoredSecurityServiceTest : public testing::Test {
 public:
  ChromeTailoredSecurityServiceTest() = default;

  TestingProfile* getProfile() { return &profile_; }

  void SetUp() override {
    browser_process_ = TestingBrowserProcess::GetGlobal();
    chrome_tailored_security_service_ =
        std::make_unique<TestChromeTailoredSecurityService>(&profile_);
    messages::MessageDispatcherBridge::SetInstanceForTesting(
        &message_dispatcher_bridge_);
  }

  void TearDown() override {
    // Remove all tabs so testing state doesn't get affected
    for (TabModel* tab : TabModelList::models()) {
      TabModelList::RemoveTabModel(tab);
    }
    messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
    chrome_tailored_security_service_->Shutdown();
    chrome_tailored_security_service_.reset();
  }

  TestChromeTailoredSecurityService* tailored_security_service() {
    return chrome_tailored_security_service_.get();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<TestingBrowserProcess> browser_process_;
  TestingProfile profile_;
  std::unique_ptr<TestChromeTailoredSecurityService>
      chrome_tailored_security_service_;
  base::HistogramTester histograms_;
  testing::NiceMock<messages::MockMessageDispatcherBridge>
      message_dispatcher_bridge_;
  base::test::ScopedFeatureList feature_list;
};

class TestTabModel : public TabModel {
 public:
  explicit TestTabModel(TestingProfile* profile)
      : TabModel(profile, chrome::android::ActivityType::kCustomTab),
        profile_(profile),
        tab_count_() {}

  int GetTabCount() const override { return tab_count_; }
  int GetActiveIndex() const override { return 0; }
  void SetWebContents(content::WebContents* webcontents) {
    web_contents_ = webcontents;
  }
  content::WebContents* GetWebContentsAt(int index) const override {
    return web_contents_;
  }
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const override {
    return nullptr;
  }

  void SetProfile(TestingProfile* profile) { profile_ = profile; }

  Profile* GetProfile() const override { return profile_; }

  void CreateTab(TabAndroid* parent,
                 content::WebContents* web_contents) override {}
  void HandlePopupNavigation(TabAndroid* parent,
                             NavigateParams* params) override {}
  content::WebContents* CreateNewTabForDevTools(const GURL& url) override {
    return nullptr;
  }
  bool IsSessionRestoreInProgress() const override { return false; }
  bool IsActiveModel() const override { return false; }
  TabAndroid* GetTabAt(int index) const override { return nullptr; }
  void SetActiveIndex(int index) override {}
  void CloseTabAt(int index) override {}
  void AddObserver(TabModelObserver* observer) override {
    observer_ = observer;
  }
  void RemoveObserver(TabModelObserver* observer) override {}

  TabModelObserver* observer_;
  TestingProfile* profile_;
  // A fake value for the current number of tabs.
  int tab_count_{0};
  content::WebContents* web_contents_;
};

TEST_F(ChromeTailoredSecurityServiceTest,
       RetryDisabledWithNoTabsLogsNoWebContents) {
  feature_list.InitAndDisableFeature(
      safe_browsing::kTailoredSecurityObserverRetries);

  for (TabModel* tab : TabModelList::models()) {
    TabModelList::RemoveTabModel(tab);
  }

  EXPECT_EQ(TabModelList::models().size(), 0U);

  chrome_tailored_security_service_->OnSyncNotificationMessageRequest(
      kTailoredSecurityEnabled);
  histograms_.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.SyncPromptEnabledNotificationResult2",
      TailoredSecurityNotificationResult::kNoWebContentsAvailable, 1);
}

TEST_F(ChromeTailoredSecurityServiceTest, WhenATabIsAvailableShowsTheMessage) {
  TestTabModel tab_model(getProfile());
  TabModelList::AddTabModel(&tab_model);

  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(getProfile(), nullptr));
  content::WebContents* raw_contents = web_contents.get();
  tab_model.SetWebContents(raw_contents);
  tab_model.tab_count_ = 1;

  chrome_tailored_security_service_->OnSyncNotificationMessageRequest(
      kTailoredSecurityEnabled);
  histograms_.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.SyncPromptEnabledNotificationResult2",
      TailoredSecurityNotificationResult::kShown, 1);
}

TEST_F(ChromeTailoredSecurityServiceTest,
       RetryEnabledWithNoTabsDoesNotLogWebContents) {
  feature_list.InitAndEnableFeature(
      safe_browsing::kTailoredSecurityObserverRetries);

  EXPECT_EQ(TabModelList::models().size(), 0U);

  chrome_tailored_security_service_->OnSyncNotificationMessageRequest(
      kTailoredSecurityEnabled);
  histograms_.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.SyncPromptEnabledNotificationResult2",
      TailoredSecurityNotificationResult::kNoWebContentsAvailable, 0);
}

TEST_F(ChromeTailoredSecurityServiceTest,
       RetryEnabledWithNoTabsThenCallOnSyncThenAddTabLogsThatMessageWasShown) {
  feature_list.InitAndEnableFeature(
      safe_browsing::kTailoredSecurityObserverRetries);

  // Call OnSync method while there are no tabs
  EXPECT_EQ(TabModelList::models().size(), 0U);
  tailored_security_service()->TailoredSecurityTimestampUpdateCallback();
  histograms_.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.SyncPromptEnabledNotificationResult2",
      TailoredSecurityNotificationResult::kShown, 0);

  // Add a tab and set the web contents
  TestTabModel tab_model(getProfile());
  TabModelList::AddTabModel(&tab_model);

  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(getProfile(), nullptr));
  content::WebContents* raw_contents = web_contents.get();
  tab_model.SetWebContents(raw_contents);
  tab_model.tab_count_ = 1;

  // Simulate observers being notified after a tab is added.
  tab_model.observer_->DidAddTab(nullptr, TabModel::TabLaunchType::FROM_LINK);

  histograms_.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.SyncPromptEnabledNotificationResult2",
      TailoredSecurityNotificationResult::kShown, 1);
}

TEST_F(ChromeTailoredSecurityServiceTest,
       RetryEnabledWithNoWebContentsLogsRetryMechanism) {
  feature_list.InitAndEnableFeature(
      safe_browsing::kTailoredSecurityObserverRetries);

  for (TabModel* tab : TabModelList::models()) {
    TabModelList::RemoveTabModel(tab);
  }

  EXPECT_EQ(TabModelList::models().size(), 0U);

  chrome_tailored_security_service_->OnSyncNotificationMessageRequest(
      kTailoredSecurityEnabled);
  histograms_.ExpectUniqueSample(
      "SafeBrowsing.TailoredSecurity.IsRecoveryTriggered", true, 1);
}

TEST_F(ChromeTailoredSecurityServiceTest,
       RetryEnabledWithWebContentsDoesNotLogRetryMechanism) {
  feature_list.InitAndEnableFeature(
      safe_browsing::kTailoredSecurityObserverRetries);

  TestTabModel tab_model(getProfile());
  TabModelList::AddTabModel(&tab_model);

  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(getProfile(), nullptr));
  content::WebContents* raw_contents = web_contents.get();
  tab_model.SetWebContents(raw_contents);
  tab_model.tab_count_ = 1;

  chrome_tailored_security_service_->OnSyncNotificationMessageRequest(
      kTailoredSecurityEnabled);
  histograms_.ExpectUniqueSample(
      "SafeBrowsing.TailoredSecurity.IsRecoveryTriggered", false, 1);
}

}  // namespace
