// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/chrome_tailored_security_service.h"

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/prefs/testing_pref_store.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_notification_result.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/pref_model_associator_client.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class Profile;

namespace safe_browsing {

// Names for Tailored Security status to make the test cases clearer.
const bool kTailoredSecurityEnabled = true;
const bool kTailoredSecurityDisabled = false;

namespace {
// Test implementation of ChromeTailoredSecurityService.
class TestChromeTailoredSecurityService : public ChromeTailoredSecurityService {
 public:
  explicit TestChromeTailoredSecurityService(Profile* profile)
      : ChromeTailoredSecurityService(profile) {}
  ~TestChromeTailoredSecurityService() override = default;

  // Returns the most recent value of `show_enable_dialog` that was provided to
  // `DisplayDesktopDialog`.
  //
  // This method should be used in conjunction with `times_dialog_displayed` to
  // ensure that the dialog has been displayed the number of times you expected.
  bool previous_show_enable_dialog_value() const {
    return previous_show_enable_dialog_value_;
  }

  // Returns the number of times that `DisplayDesktopDialog` has been called.
  int times_dialog_displayed() const {
    return times_display_desktop_dialog_called_;
  }

  // ChromeTailoredSecurityService:
  // This method is overridden so we can detect the number of times that the
  // dialog has been requested to be shown and what the last value was.
  void DisplayDesktopDialog(Browser* browser,
                            bool show_enable_dialog) override {
    previous_show_enable_dialog_value_ = show_enable_dialog;
    times_display_desktop_dialog_called_++;
  }

  // overridden to make the method public for testing.
  void MaybeNotifySyncUser(bool is_enabled,
                           base::Time previous_update) override {
    called_maybe_notify_sync_user_ = true;
    ChromeTailoredSecurityService::MaybeNotifySyncUser(is_enabled,
                                                       previous_update);
  }

  // Sets the value that is expected to be returned by the remote server.
  void SetTailoredSecurityServiceValue(bool tailored_security_bit_value) {
    tailored_security_service_value_ = tailored_security_bit_value;
  }

  bool GetTailoredSecurityServiceValue() {
    return tailored_security_service_value_;
  }

  bool MaybeNotifySyncUserWasCalled() { return called_maybe_notify_sync_user_; }
  void ResetMaybeNotifySyncUserWasCalled() {
    called_maybe_notify_sync_user_ = false;
  }

  // Overridden to skip calling the remote server. It supplies the value that
  // was most recently set through `SetTailoredSecurityServiceValue`.
  void TailoredSecurityTimestampUpdateCallback() override {
    MaybeNotifySyncUser(tailored_security_service_value_, base::Time::Now());
  }

 private:
  bool previous_show_enable_dialog_value_ = false;
  int times_display_desktop_dialog_called_ = 0;
  // Represents the value that we want the remote service to provide.
  bool tailored_security_service_value_ = true;
  bool called_maybe_notify_sync_user_ = false;
};
}  // namespace

// TODO(crbug.com/40927036): Move tests related to base class behavior of
// MaybeNotifySyncUser to the test suite for TailoredSecurityService.
class ChromeTailoredSecurityServiceTest : public testing::Test {
 public:
  ChromeTailoredSecurityServiceTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  ChromeTailoredSecurityServiceTest(const ChromeTailoredSecurityServiceTest&) =
      delete;
  ChromeTailoredSecurityServiceTest& operator=(
      const ChromeTailoredSecurityServiceTest&) = delete;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        safe_browsing::kTailoredSecurityRetryForSyncUsers);
    SetUpPrerequisites(/* history_sync_enabled= */ true,
                       /* policy_controlled_sb_enabled= */ false);
  }

  // Sets up the member variables for testing. Classes that extend this class
  // will need to call `SetUpPrerequisites()` from their `SetUp()` method.
  void SetUpPrerequisites(bool history_sync_enabled,
                          bool policy_controlled_sb_enabled) {
    if (profile_manager_needs_setup_) {
      ASSERT_TRUE(profile_manager_.SetUp());
    }
    profile_manager_needs_setup_ = false;
    profiles_created_count_++;
    profile_ = profile_manager_.CreateTestingProfile(
        "primary_account" + base::NumberToString(profiles_created_count_),
        GetTestingFactories());
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
    GetIdentityTestEnv()->SetTestURLLoaderFactory(&test_url_loader_factory_);
    // TODO(crbug.com/40067771): `ConsentLevel::kSync` is deprecated and should
    // be removed. See `ConsentLevel::kSync` documentation for details.
    GetIdentityTestEnv()->MakePrimaryAccountAvailable(
        "test@foo.com", signin::ConsentLevel::kSync);
    prefs_ = profile_->GetTestingPrefService();
    if (history_sync_enabled) {
      sync_service()->GetUserSettings()->SetSelectedTypes(
          /*sync_everything=*/false,
          /*types=*/{syncer::UserSelectableType::kHistory});
    } else {
      sync_service()->GetUserSettings()->SetSelectedTypes(
          /*sync_everything=*/false,
          /*types=*/{});
    }

    if (policy_controlled_sb_enabled) {
      prefs()->SetManagedPref(prefs::kSafeBrowsingEnabled,
                              std::make_unique<base::Value>(true));
      prefs()->SetManagedPref(prefs::kSafeBrowsingEnhanced,
                              std::make_unique<base::Value>(false));
    } else {
      prefs()->RemoveManagedPref(prefs::kSafeBrowsingEnabled);
      prefs()->RemoveManagedPref(prefs::kSafeBrowsingEnhanced);
    }

    browser_window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(profile(), true);
    params.type = Browser::TYPE_NORMAL;
    params.window = browser_window_.get();
    browser_ = std::unique_ptr<Browser>(Browser::Create(params));
    chrome_tailored_security_service_ =
        std::make_unique<TestChromeTailoredSecurityService>(profile_);
  }

  TestingProfile::TestingFactories GetTestingFactories() {
    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.emplace_back(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                            &test_url_loader_factory_));
    factories.emplace_back(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::TestSyncService>();
            }));
    return factories;
  }

  void TearDown() override {
    // Remove any tabs in the tab strip otherwise the test will crash.
    if (browser_) {
      while (!browser_->tab_strip_model()->empty()) {
        browser_->tab_strip_model()->DetachAndDeleteWebContentsAt(0);
      }
    }

    browser_.reset();
    browser_window_.reset();
    chrome_tailored_security_service_->Shutdown();
    chrome_tailored_security_service_.reset();

    if (profile_) {
      auto* partition = profile_->GetDefaultStoragePartition();
      if (partition) {
        partition->WaitForDeletionTasksForTesting();
      }
    }
  }

  TestChromeTailoredSecurityService* tailored_security_service() {
    return chrome_tailored_security_service_.get();
  }

  Browser* browser() { return browser_.get(); }

  // Add a tab with a test `content::WebContents` to the browser.
  content::WebContents* AddTab(const GURL& url) {
    std::unique_ptr<content::WebContents> web_contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
    content::WebContents* raw_contents = web_contents.get();

    browser()->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                    true);
    EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
              raw_contents);

    content::NavigationSimulator::NavigateAndCommitFromBrowser(raw_contents,
                                                               url);
    EXPECT_EQ(url, raw_contents->GetLastCommittedURL());

    return raw_contents;
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile_->GetTestingPrefService();
  }

  TestingProfile* profile() { return profile_; }

  syncer::TestSyncService* sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile()));
  }

  signin::IdentityTestEnvironment* GetIdentityTestEnv() {
    DCHECK(identity_test_env_adaptor_);
    return identity_test_env_adaptor_->identity_test_env();
  }

  void SetAccountTailoredSecurityTimestamp(base::Time time) {
    // Changing prefs::kAccountTailoredSecurityUpdateTimestamp triggers the
    // preference observer, so here we prevent the preference observer method
    // from doing anything by having the server bit value match the safe
    // browsing level in preferences.
    bool original_tailored_security_service_value =
        tailored_security_service()->GetTailoredSecurityServiceValue();

    tailored_security_service()->SetTailoredSecurityServiceValue(
        IsEnhancedProtectionEnabled(*prefs()));
    prefs()->SetTime(prefs::kAccountTailoredSecurityUpdateTimestamp, time);
    tailored_security_service()->SetTailoredSecurityServiceValue(
        original_tailored_security_service_value);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  // Must be declared before anything that may make use of the
  // directory so as to ensure files are closed before cleanup.
  base::ScopedTempDir temp_dir_;
  // This is required to create browser tabs in the tests.
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable, DanglingUntriaged>
      prefs_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_environment_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestChromeTailoredSecurityService>
      chrome_tailored_security_service_;
  bool profile_manager_needs_setup_ = true;
  int profiles_created_count_ = 0;
};

class ChromeTailoredSecurityServiceRetryForSyncUsersDisabledTest
    : public ChromeTailoredSecurityServiceTest {
 public:
  ChromeTailoredSecurityServiceRetryForSyncUsersDisabledTest() = default;
  ChromeTailoredSecurityServiceRetryForSyncUsersDisabledTest(
      const ChromeTailoredSecurityServiceRetryForSyncUsersDisabledTest&) =
      delete;
  ChromeTailoredSecurityServiceRetryForSyncUsersDisabledTest& operator=(
      const ChromeTailoredSecurityServiceRetryForSyncUsersDisabledTest&) =
      delete;

  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(
        safe_browsing::kTailoredSecurityRetryForSyncUsers);
    SetUpPrerequisites(/* history_sync_enabled= */ false,
                       /* policy_controlled_sb_enabled= */ false);
  }
};

// Some of the test names are shorted using "Ts" for Tailored Security, "Ep"
// for Enhanced Protection and "Sb" for Safe Browsing.

TEST_F(ChromeTailoredSecurityServiceTest,
       TailoredSecurityEnabledShowsEnableDialog) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  const GURL google_url("https://www.google.com");
  AddTab(google_url);
  int initial_times_displayed =
      tailored_security_service()->times_dialog_displayed();

  tailored_security_service()->MaybeNotifySyncUser(kTailoredSecurityEnabled,
                                                   base::Time::Now());

  EXPECT_EQ(tailored_security_service()->times_dialog_displayed(),
            initial_times_displayed + 1);
  EXPECT_TRUE(tailored_security_service()->previous_show_enable_dialog_value());
}

TEST_F(ChromeTailoredSecurityServiceTest,
       TailoredSecurityEnabledButHistorySyncDisabledDoesNotShowEnableDialog) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  const GURL google_url("https://www.google.com");
  AddTab(google_url);
  int initial_times_displayed =
      tailored_security_service()->times_dialog_displayed();

  // disable history sync
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{});
  tailored_security_service()->MaybeNotifySyncUser(kTailoredSecurityEnabled,
                                                   base::Time::Now());

  EXPECT_EQ(tailored_security_service()->times_dialog_displayed(),
            initial_times_displayed);
}

TEST_F(ChromeTailoredSecurityServiceTest,
       TailoredSecurityEnabledButHistorySyncDisabledLogsHistoryNotSynced) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  const GURL google_url("https://www.google.com");
  AddTab(google_url);

  // disable history sync
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{});
  base::HistogramTester tester;
  tailored_security_service()->MaybeNotifySyncUser(kTailoredSecurityEnabled,
                                                   base::Time::Now());

  tester.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.SyncPromptEnabledNotificationResult2",
      TailoredSecurityNotificationResult::kHistoryNotSynced, 1);
}

TEST_F(ChromeTailoredSecurityServiceTest,
       TailoredSecurityEnabledButHistorySyncEnabledDoesNotLogHistoryNotSynced) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  const GURL google_url("https://www.google.com");
  AddTab(google_url);

  // enable history sync
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kHistory});
  base::HistogramTester tester;
  tailored_security_service()->MaybeNotifySyncUser(kTailoredSecurityEnabled,
                                                   base::Time::Now());

  tester.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.SyncPromptEnabledNotificationResult2",
      TailoredSecurityNotificationResult::kHistoryNotSynced, 0);
}

TEST_F(ChromeTailoredSecurityServiceTest, TsEnabledEnablesEp) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  const GURL google_url("https://www.google.com");
  AddTab(google_url);
  EXPECT_FALSE(IsEnhancedProtectionEnabled(*prefs()));
  tailored_security_service()->MaybeNotifySyncUser(kTailoredSecurityEnabled,
                                                   base::Time::Now());
  EXPECT_TRUE(IsEnhancedProtectionEnabled(*prefs()));
}

TEST_F(ChromeTailoredSecurityServiceTest, EpAlreadyEnabledDoesNotShowDialog) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);
  const GURL google_url("https://www.google.com");
  AddTab(google_url);
  int initial_times_displayed =
      tailored_security_service()->times_dialog_displayed();
  tailored_security_service()->MaybeNotifySyncUser(kTailoredSecurityEnabled,
                                                   base::Time::Now());
  EXPECT_EQ(tailored_security_service()->times_dialog_displayed(),
            initial_times_displayed);
}

TEST_F(ChromeTailoredSecurityServiceTest, EpAlreadyEnabledLeavesEpEnabled) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);
  const GURL google_url("https://www.google.com");
  AddTab(google_url);
  EXPECT_TRUE(IsEnhancedProtectionEnabled(*prefs()));
  tailored_security_service()->MaybeNotifySyncUser(kTailoredSecurityEnabled,
                                                   base::Time::Now());
  EXPECT_TRUE(IsEnhancedProtectionEnabled(*prefs()));
}

TEST_F(ChromeTailoredSecurityServiceTest,
       EpWasEnabledByTsAndTsNowDisabledShowsDialog) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  const GURL google_url("https://www.google.com");
  AddTab(google_url);
  int initial_times_displayed =
      tailored_security_service()->times_dialog_displayed();
  // Enable ESB - this will display the dialog once.
  tailored_security_service()->MaybeNotifySyncUser(kTailoredSecurityEnabled,
                                                   base::Time::Now());

  EXPECT_EQ(tailored_security_service()->times_dialog_displayed(),
            initial_times_displayed + 1);
  // Then detect that TailoredSecurity was disabled.
  tailored_security_service()->MaybeNotifySyncUser(kTailoredSecurityDisabled,
                                                   base::Time::Now());
  EXPECT_EQ(tailored_security_service()->times_dialog_displayed(),
            initial_times_displayed + 2);
  EXPECT_FALSE(
      tailored_security_service()->previous_show_enable_dialog_value());
}

TEST_F(ChromeTailoredSecurityServiceTest,
       EpEnabledByTsAndTsNowDisabledDisablesEp) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  const GURL google_url("https://www.google.com");
  AddTab(google_url);
  // Enable ESB
  tailored_security_service()->MaybeNotifySyncUser(kTailoredSecurityEnabled,
                                                   base::Time::Now());

  EXPECT_TRUE(IsEnhancedProtectionEnabled(*prefs()));
  // Then detect that TailoredSecurity was disabled.
  tailored_security_service()->MaybeNotifySyncUser(kTailoredSecurityDisabled,
                                                   base::Time::Now());

  EXPECT_FALSE(IsEnhancedProtectionEnabled(*prefs()));
  EXPECT_TRUE(IsSafeBrowsingEnabled(*prefs()));
}

TEST_F(ChromeTailoredSecurityServiceTest,
       SpEnabledAndTsNowDisabledDoesNotShowDialog) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  const GURL google_url("https://www.google.com");
  AddTab(google_url);
  int initial_times_displayed =
      tailored_security_service()->times_dialog_displayed();
  tailored_security_service()->MaybeNotifySyncUser(kTailoredSecurityDisabled,
                                                   base::Time::Now());
  EXPECT_EQ(tailored_security_service()->times_dialog_displayed(),
            initial_times_displayed);
}

TEST_F(ChromeTailoredSecurityServiceTest,
       SpEnabledAndTsNowDisabledDoesNotChangeSb) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  const GURL google_url("https://www.google.com");
  AddTab(google_url);

  EXPECT_FALSE(IsEnhancedProtectionEnabled(*prefs()));
  EXPECT_TRUE(IsSafeBrowsingEnabled(*prefs()));

  tailored_security_service()->MaybeNotifySyncUser(kTailoredSecurityDisabled,
                                                   base::Time::Now());
  EXPECT_FALSE(IsEnhancedProtectionEnabled(*prefs()));
  EXPECT_TRUE(IsSafeBrowsingEnabled(*prefs()));
}

TEST_F(ChromeTailoredSecurityServiceTest,
       EpEnabledByUserTsDisabledDoesNotShowDialog) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);
  const GURL google_url("https://www.google.com");
  AddTab(google_url);

  int times_dialog_displayed_before =
      tailored_security_service()->times_dialog_displayed();
  tailored_security_service()->MaybeNotifySyncUser(kTailoredSecurityDisabled,
                                                   base::Time::Now());
  EXPECT_EQ(tailored_security_service()->times_dialog_displayed(),
            times_dialog_displayed_before);
}

TEST_F(ChromeTailoredSecurityServiceTest,
       EpEnabledByUserTsDisabledDoesNotChangeSb) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);
  const GURL google_url("https://www.google.com");
  AddTab(google_url);

  EXPECT_TRUE(IsEnhancedProtectionEnabled(*prefs()));
  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kEnhancedProtectionEnabledViaTailoredSecurity));

  tailored_security_service()->MaybeNotifySyncUser(kTailoredSecurityDisabled,
                                                   base::Time::Now());
  EXPECT_TRUE(IsEnhancedProtectionEnabled(*prefs()));
}

TEST_F(ChromeTailoredSecurityServiceTest,
       WhenRetryEnabledOnSuccessStoresNoRetryNeeded) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  tailored_security_service()->MaybeNotifySyncUser(kTailoredSecurityEnabled,
                                                   base::Time::Now());
  EXPECT_EQ(prefs()->GetInteger(prefs::kTailoredSecuritySyncFlowRetryState),
            TailoredSecurityRetryState::NO_RETRY_NEEDED);
}

TEST_F(ChromeTailoredSecurityServiceTest,
       WhenRetryEnabledButHistorySyncDisabledSetsNoRetryNeeded) {
  SetUpPrerequisites(/* history_sync_enabled= */ false,
                     /* policy_controlled_sb_enabled= */ false);
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);

  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::TailoredSecurityRetryState::UNSET);

  tailored_security_service()->MaybeNotifySyncUser(kTailoredSecurityEnabled,
                                                   base::Time::Now());

  EXPECT_EQ(prefs()->GetInteger(prefs::kTailoredSecuritySyncFlowRetryState),
            safe_browsing::TailoredSecurityRetryState::NO_RETRY_NEEDED);
}

TEST_F(ChromeTailoredSecurityServiceTest,
       WhenRetryEnabledAndSbControlledByPolicySetsNoRetryNeeded) {
  SetUpPrerequisites(/* history_sync_enabled= */ true,
                     /* policy_controlled_sb_enabled= */ true);

  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::UNSET);

  tailored_security_service()->MaybeNotifySyncUser(kTailoredSecurityEnabled,
                                                   base::Time::Now());

  EXPECT_EQ(prefs()->GetInteger(prefs::kTailoredSecuritySyncFlowRetryState),
            safe_browsing::NO_RETRY_NEEDED);
}

TEST_F(ChromeTailoredSecurityServiceTest,
       WhenRetryEnabledAndEpAlreadyEnabledSetsNoRetryNeeded) {
  SetUpPrerequisites(/* history_sync_enabled= */ true,
                     /* policy_controlled_sb_enabled= */ false);

  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);
  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::TailoredSecurityRetryState::UNSET);

  tailored_security_service()->MaybeNotifySyncUser(kTailoredSecurityEnabled,
                                                   base::Time::Now());

  EXPECT_EQ(prefs()->GetInteger(prefs::kTailoredSecuritySyncFlowRetryState),
            safe_browsing::TailoredSecurityRetryState::NO_RETRY_NEEDED);
}

TEST_F(ChromeTailoredSecurityServiceTest,
       HistorySyncAndSbNotControlledByPolicyRunsRetryLogicAfterStartupDelay) {
  SetUpPrerequisites(/* history_sync_enabled= */ true,
                     /* policy_controlled_sb_enabled= */ false);

  const GURL google_url("https://www.google.com");
  AddTab(google_url);
  // Setup the state so that a dialog will be displayed because that is what we
  // will use to check if the startup task ran at the correct time.
  SetAccountTailoredSecurityTimestamp(base::Time::Now());
  tailored_security_service()->SetTailoredSecurityServiceValue(true);
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  int initial_times_displayed =
      tailored_security_service()->times_dialog_displayed();
  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::RETRY_NEEDED);
  prefs()->SetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp,
                   base::Time::Now());

  // The logic should run after the startup delay, so check that it does not run
  // before that.
  task_environment_.FastForwardBy(
      ChromeTailoredSecurityService::kRetryAttemptStartupDelay -
      base::Seconds(1));
  EXPECT_EQ(tailored_security_service()->times_dialog_displayed(),
            initial_times_displayed);
  // Startup delay has passed, so verify that the retry ran.
  task_environment_.FastForwardBy(base::Seconds(1));
  // We're checking if the dialog was displayed as a proxy to checking if the
  // logic ran because we don't have a direct way of checking this.
  EXPECT_EQ(tailored_security_service()->times_dialog_displayed(),
            initial_times_displayed + 1);
}

TEST_F(ChromeTailoredSecurityServiceTest, HistorySyncNotSetDoesNotRetry) {
  SetUpPrerequisites(/* history_sync_enabled= */ false,
                     /* policy_controlled_sb_enabled= */ false);

  const GURL google_url("https://www.google.com");
  AddTab(google_url);
  // Setup the state so that a dialog will be displayed because that is what we
  // will use to check if the startup task ran at the correct time.
  SetAccountTailoredSecurityTimestamp(base::Time::Now());
  tailored_security_service()->SetTailoredSecurityServiceValue(true);
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::RETRY_NEEDED);
  prefs()->SetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp,
                   base::Time::Now());

  tailored_security_service()->ResetMaybeNotifySyncUserWasCalled();
  task_environment_.FastForwardBy(
      ChromeTailoredSecurityService::kRetryAttemptStartupDelay);
  EXPECT_FALSE(tailored_security_service()->MaybeNotifySyncUserWasCalled());
}

TEST_F(ChromeTailoredSecurityServiceTest, SbControlledByPolicyDoesNotRetry) {
  SetUpPrerequisites(/* history_sync_enabled= */ true,
                     /* policy_controlled_sb_enabled= */ true);

  const GURL google_url("https://www.google.com");
  AddTab(google_url);
  // Setup the state so that a dialog will be displayed because that is what we
  // will use to check if the startup task ran at the correct time.
  SetAccountTailoredSecurityTimestamp(base::Time::Now());
  tailored_security_service()->SetTailoredSecurityServiceValue(true);
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);

  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::RETRY_NEEDED);
  prefs()->SetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp,
                   base::Time::Now());

  tailored_security_service()->ResetMaybeNotifySyncUserWasCalled();
  task_environment_.FastForwardBy(
      ChromeTailoredSecurityService::kRetryAttemptStartupDelay);
  EXPECT_FALSE(tailored_security_service()->MaybeNotifySyncUserWasCalled());
}

TEST_F(ChromeTailoredSecurityServiceTest,
       TailoredSecurityUpdateTimeNotSetDoesNotRetry) {
  const GURL google_url("https://www.google.com");
  AddTab(google_url);
  tailored_security_service()->SetTailoredSecurityServiceValue(true);
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  int initial_times_displayed =
      tailored_security_service()->times_dialog_displayed();

  SetAccountTailoredSecurityTimestamp(base::Time());
  task_environment_.FastForwardBy(
      ChromeTailoredSecurityService::kRetryAttemptStartupDelay);

  // Verify that notification was not shown.
  EXPECT_EQ(tailored_security_service()->times_dialog_displayed(),
            initial_times_displayed);
}

TEST_F(ChromeTailoredSecurityServiceTest,
       WhenRetryNeededButNotEnoughTimeHasPassedDoesNotRetry) {
  const GURL google_url("https://www.google.com");
  AddTab(google_url);
  SetAccountTailoredSecurityTimestamp(base::Time::Now());
  tailored_security_service()->SetTailoredSecurityServiceValue(true);
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);

  int initial_times_displayed =
      tailored_security_service()->times_dialog_displayed();
  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::RETRY_NEEDED);

  // set next sync flow to after when the retry check will happen.
  prefs()->SetTime(
      prefs::kTailoredSecurityNextSyncFlowTimestamp,
      base::Time::Now() +
          ChromeTailoredSecurityService::kRetryAttemptStartupDelay +
          base::Seconds(1));

  base::HistogramTester tester;
  task_environment_.FastForwardBy(
      ChromeTailoredSecurityService::kRetryAttemptStartupDelay);

  EXPECT_EQ(tailored_security_service()->times_dialog_displayed(),
            initial_times_displayed);

  tester.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
      ChromeTailoredSecurityService::TailoredSecurityShouldRetryOutcome::
          kRetryNeededKeepWaiting,
      1);
}

TEST_F(ChromeTailoredSecurityServiceTest,
       WhenRetryNeededAndEnoughTimeHasPassedRetries) {
  const GURL google_url("https://www.google.com");
  AddTab(google_url);
  SetAccountTailoredSecurityTimestamp(base::Time::Now());
  tailored_security_service()->SetTailoredSecurityServiceValue(true);
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  int initial_times_displayed =
      tailored_security_service()->times_dialog_displayed();
  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::RETRY_NEEDED);
  prefs()->SetTime(
      prefs::kTailoredSecurityNextSyncFlowTimestamp,
      base::Time::Now() +
          ChromeTailoredSecurityService::kRetryAttemptStartupDelay -
          base::Seconds(1));

  base::HistogramTester tester;
  task_environment_.FastForwardBy(
      ChromeTailoredSecurityService::kRetryAttemptStartupDelay);

  // Verify that notification was shown.
  EXPECT_EQ(tailored_security_service()->times_dialog_displayed(),
            initial_times_displayed + 1);
  tester.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
      ChromeTailoredSecurityService::TailoredSecurityShouldRetryOutcome::
          kRetryNeededDoRetry,
      1);
}

TEST_F(
    ChromeTailoredSecurityServiceTest,
    WhenRetryNeededAndEnoughTimeHasPassedUpdatesNextSyncFlowTimestampByNextAttemptDelay) {
  const GURL google_url("https://www.google.com");
  AddTab(google_url);

  SetAccountTailoredSecurityTimestamp(base::Time::Now());
  tailored_security_service()->SetTailoredSecurityServiceValue(true);
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::RETRY_NEEDED);

  prefs()->SetTime(
      prefs::kTailoredSecurityNextSyncFlowTimestamp,
      base::Time::Now() +
          ChromeTailoredSecurityService::kRetryAttemptStartupDelay -
          base::Seconds(1));
  task_environment_.FastForwardBy(
      ChromeTailoredSecurityService::kRetryAttemptStartupDelay);

  EXPECT_EQ(prefs()->GetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp),
            base::Time::Now() +
                ChromeTailoredSecurityService::kRetryNextAttemptDelay);
}

TEST_F(
    ChromeTailoredSecurityServiceTest,
    WhenRetryNotSetAndEnhancedProtectionEnabledViaTailoredSecurityDoesNotSetNextSyncFlowTimestamp) {
  const GURL google_url("https://www.google.com");
  AddTab(google_url);
  SetAccountTailoredSecurityTimestamp(base::Time::Now());
  tailored_security_service()->SetTailoredSecurityServiceValue(true);
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);

  prefs()->SetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp, base::Time());
  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::UNSET);
  prefs()->SetBoolean(prefs::kEnhancedProtectionEnabledViaTailoredSecurity,
                      true);

  base::HistogramTester tester;
  task_environment_.FastForwardBy(
      ChromeTailoredSecurityService::kRetryAttemptStartupDelay);
  EXPECT_EQ(prefs()->GetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp),
            base::Time());

  tester.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
      ChromeTailoredSecurityService::TailoredSecurityShouldRetryOutcome::
          kUnsetInitializeWaitingPeriod,
      0);
}

TEST_F(
    ChromeTailoredSecurityServiceTest,
    WhenRetryNotSetAndNextSyncFlowNotSetSetsNextSyncFlowToWaitingIntervalFromNow) {
  const GURL google_url("https://www.google.com");
  AddTab(google_url);
  SetAccountTailoredSecurityTimestamp(base::Time::Now());
  tailored_security_service()->SetTailoredSecurityServiceValue(true);
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);

  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::UNSET);
  prefs()->SetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp, base::Time());

  base::HistogramTester tester;
  task_environment_.FastForwardBy(
      ChromeTailoredSecurityService::kRetryAttemptStartupDelay);
  EXPECT_EQ(prefs()->GetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp),
            base::Time::Now() +
                ChromeTailoredSecurityService::kWaitingPeriodInterval);

  tester.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
      ChromeTailoredSecurityService::TailoredSecurityShouldRetryOutcome::
          kUnsetInitializeWaitingPeriod,
      1);
}

TEST_F(ChromeTailoredSecurityServiceTest,
       WhenRetryNotSetAndNextSyncFlowHasPassedRunsRetry) {
  const GURL google_url("https://www.google.com");
  AddTab(google_url);

  SetAccountTailoredSecurityTimestamp(base::Time::Now());
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  tailored_security_service()->SetTailoredSecurityServiceValue(true);

  int initial_times_displayed =
      tailored_security_service()->times_dialog_displayed();

  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::UNSET);
  prefs()->SetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp,
                   base::Time::Now());
  base::HistogramTester tester;
  task_environment_.FastForwardBy(
      ChromeTailoredSecurityService::kRetryAttemptStartupDelay);
  // Verify that notification was shown.
  EXPECT_EQ(tailored_security_service()->times_dialog_displayed(),
            initial_times_displayed + 1);
  tester.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
      ChromeTailoredSecurityService::TailoredSecurityShouldRetryOutcome::
          kUnsetRetryBecauseDoneWaiting,
      1);
}

TEST_F(ChromeTailoredSecurityServiceTest,
       WhenRetryNotSetAndNextSyncFlowHasPassedSetsNextSyncFlowToTomorrow) {
  const GURL google_url("https://www.google.com");
  AddTab(google_url);

  SetAccountTailoredSecurityTimestamp(base::Time::Now());
  tailored_security_service()->SetTailoredSecurityServiceValue(true);
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);

  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::UNSET);
  prefs()->SetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp,
                   base::Time::Now());
  task_environment_.FastForwardBy(
      ChromeTailoredSecurityService::kRetryAttemptStartupDelay);

  // Next sync flow time should be tomorrow.
  EXPECT_EQ(prefs()->GetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp),
            base::Time::Now() +
                ChromeTailoredSecurityService::kRetryNextAttemptDelay);
}

TEST_F(ChromeTailoredSecurityServiceTest,
       WhenRetryNotSetAndNextSyncFlowHasNotPassedDoesNotRunRetryLogic) {
  const GURL google_url("https://www.google.com");
  AddTab(google_url);

  SetAccountTailoredSecurityTimestamp(base::Time::Now());
  tailored_security_service()->SetTailoredSecurityServiceValue(true);
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);

  int initial_times_displayed =
      tailored_security_service()->times_dialog_displayed();

  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::UNSET);
  // Set the next flow time to tomorrow. The logic should not run until then.
  prefs()->SetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp,
                   base::Time::Now() + base::Days(1));
  base::HistogramTester tester;
  task_environment_.FastForwardBy(
      ChromeTailoredSecurityService::kRetryAttemptStartupDelay);
  // Should not have displayed because it needs to wait more.
  EXPECT_EQ(tailored_security_service()->times_dialog_displayed(),
            initial_times_displayed);

  tester.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
      ChromeTailoredSecurityService::TailoredSecurityShouldRetryOutcome::
          kUnsetStillWaiting,
      1);
}

TEST_F(ChromeTailoredSecurityServiceTest, WhenNoRetryNeededDoesNotRetry) {
  const GURL google_url("https://www.google.com");
  AddTab(google_url);

  SetAccountTailoredSecurityTimestamp(base::Time::Now());
  tailored_security_service()->SetTailoredSecurityServiceValue(true);
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  int initial_times_displayed =
      tailored_security_service()->times_dialog_displayed();

  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::NO_RETRY_NEEDED);
  task_environment_.FastForwardBy(
      ChromeTailoredSecurityService::kRetryAttemptStartupDelay);

  // Verify that notification was not shown.
  EXPECT_EQ(tailored_security_service()->times_dialog_displayed(),
            initial_times_displayed);
}

TEST_F(ChromeTailoredSecurityServiceRetryForSyncUsersDisabledTest,
       OnSuccessDoesNotUpdateRetryStatePref) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  auto original_value =
      prefs()->GetInteger(prefs::kTailoredSecuritySyncFlowRetryState);
  tailored_security_service()->MaybeNotifySyncUser(kTailoredSecurityEnabled,
                                                   base::Time::Now());
  EXPECT_EQ(prefs()->GetInteger(prefs::kTailoredSecuritySyncFlowRetryState),
            original_value);
}

TEST_F(ChromeTailoredSecurityServiceRetryForSyncUsersDisabledTest,
       WhenRetryForSyncUsersIsDisabledDoesNotRunRetryLogicAfterStartupDelay) {
  const GURL google_url("https://www.google.com");
  AddTab(google_url);
  SetAccountTailoredSecurityTimestamp(base::Time::Now());
  tailored_security_service()->SetTailoredSecurityServiceValue(true);
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  int initial_times_displayed =
      tailored_security_service()->times_dialog_displayed();

  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::RETRY_NEEDED);
  task_environment_.FastForwardBy(
      ChromeTailoredSecurityService::kRetryAttemptStartupDelay);

  EXPECT_EQ(tailored_security_service()->times_dialog_displayed(),
            initial_times_displayed);
}

}  // namespace safe_browsing
