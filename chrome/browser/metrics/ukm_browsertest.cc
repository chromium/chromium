// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/browser/metrics/testing/metrics_reporting_pref_helper.h"
#include "chrome/browser/metrics/testing/sync_metrics_test_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "components/metrics/demographic_metrics_provider.h"
#include "components/metrics/test/demographic_metrics_test_utils.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_token_status.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/ukm_service.h"
#include "components/ukm/ukm_test_helper.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "third_party/metrics_proto/ukm/report.pb.h"
#include "third_party/metrics_proto/user_demographics.pb.h"
#include "url/url_constants.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#else
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "content/public/browser/web_contents.h"
#endif  // !defined(OS_ANDROID)

namespace metrics {
namespace {

class TestTabModel;

#if !defined(OS_ANDROID)
typedef Browser* PlatformBrowser;
#else
typedef std::unique_ptr<TestTabModel> PlatformBrowser;
#endif  // !defined(OS_ANDROID)

// Clears the specified data using BrowsingDataRemover.
void ClearBrowsingData(Profile* profile) {
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(profile);
  content::BrowsingDataRemoverCompletionObserver observer(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, &observer);
  observer.BlockUntilCompletion();
  // Make sure HistoryServiceObservers have a chance to be notified.
  content::RunAllTasksUntilIdle();
}

ukm::UkmService* GetUkmService() {
  return g_browser_process->GetMetricsServicesManager()->GetUkmService();
}

#if defined(OS_ANDROID)
// TestTabModel provides a means of creating a tab associated with a given
// profile. The new tab can then be added to Android's TabModelList.
class TestTabModel : public TabModel {
 public:
  explicit TestTabModel(Profile* profile)
      : TabModel(profile, /*is_tabbed_activity=*/false),
        web_contents_(content::WebContents::Create(
            content::WebContents::CreateParams(GetProfile()))) {}

  ~TestTabModel() override = default;

  // TabModel:
  int GetTabCount() const override { return 0; }
  int GetActiveIndex() const override { return 0; }
  content::WebContents* GetActiveWebContents() const override {
    return web_contents_.get();
  }
  content::WebContents* GetWebContentsAt(int index) const override {
    return nullptr;
  }
  TabAndroid* GetTabAt(int index) const override { return nullptr; }
  void SetActiveIndex(int index) override {}
  void CloseTabAt(int index) override {}
  void CreateTab(TabAndroid* parent,
                 content::WebContents* web_contents) override {}
  void HandlePopupNavigation(TabAndroid* parent,
                             NavigateParams* params) override {}
  content::WebContents* CreateNewTabForDevTools(const GURL& url) override {
    return nullptr;
  }
  bool IsSessionRestoreInProgress() const override { return false; }
  bool IsCurrentModel() const override { return false; }
  void AddObserver(TabModelObserver* observer) override {}
  void RemoveObserver(TabModelObserver* observer) override {}

 private:
  // The WebContents associated with this tab's profile.
  std::unique_ptr<content::WebContents> web_contents_;
};
#endif  // defined(OS_ANDROID)

}  // namespace

// An observer that returns back to test code after a new profile is
// initialized.
#if !defined(OS_ANDROID)
void UnblockOnProfileCreation(base::RunLoop* run_loop,
                              Profile* profile,
                              Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    run_loop->Quit();
}
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
Profile* CreateGuestProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path = profile_manager->GetGuestProfilePath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      new_path, base::Bind(&UnblockOnProfileCreation, &run_loop),
      base::string16(), std::string());
  run_loop.Run();
  return profile_manager->GetProfileByPath(new_path);
}
#endif  // !defined(OS_ANDROID)

// A helper object for overriding metrics enabled state.
class MetricsConsentOverride {
 public:
  explicit MetricsConsentOverride(bool initial_state) : state_(initial_state) {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &state_);
    Update(initial_state);
  }

  ~MetricsConsentOverride() {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        nullptr);
  }

  void Update(bool state) {
    state_ = state;
    // Trigger rechecking of metrics state.
    g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(
        true);
  }

 private:
  bool state_;
};

// Test fixture that provides access to some UKM internals.
class UkmBrowserTestBase : public SyncTest {
 public:
  UkmBrowserTestBase() : SyncTest(SINGLE_CLIENT) {
    // TODO(crbug.com/1068796): Replace kMetricsReportingFeature with a more
    // apt name.
    // Explicitly enable UKM and disable metrics reporting. Disabling metrics
    // reporting should affect only UMA--not UKM.
    scoped_feature_list_.InitWithFeatures({ukm::kUkmFeature},
                                          {internal::kMetricsReportingFeature});
  }

#if !defined(OS_ANDROID)
  ukm::UkmSource* NavigateAndGetSource(const GURL& url,
                                       Browser* browser,
                                       ukm::UkmTestHelper* ukm_test_helper) {
    content::NavigationHandleObserver observer(
        browser->tab_strip_model()->GetActiveWebContents(), url);
    ui_test_utils::NavigateToURL(browser, url);
    const ukm::SourceId source_id = ukm::ConvertToSourceId(
        observer.navigation_id(), ukm::SourceIdType::NAVIGATION_ID);
    return ukm_test_helper->GetSource(source_id);
  }
#endif  // !defined(OS_ANDROID)

 protected:
  // Creates and returns a platform-appropriate browser for |profile|.
  PlatformBrowser CreatePlatformBrowser(Profile* profile) {
#if !defined(OS_ANDROID)
    return CreateBrowser(profile);
#else
    std::unique_ptr<TestTabModel> tab_model =
        std::make_unique<TestTabModel>(profile);
    TabModelList::AddTabModel(tab_model.get());
    EXPECT_TRUE(content::NavigateToURL(tab_model->GetActiveWebContents(),
                                       GURL("about:blank")));
    return tab_model;
#endif  // !defined(OS_ANDROID)
  }

  // Creates a platform-appropriate incognito browser for |profile|.
  PlatformBrowser CreateIncognitoPlatformBrowser(Profile* profile) {
    EXPECT_TRUE(profile->IsOffTheRecord());
#if !defined(OS_ANDROID)
    return CreateIncognitoBrowser(profile);
#else
    return CreatePlatformBrowser(profile);
#endif  // !defined(OS_ANDROID)
  }

  // Closes |browser| in a way that is appropriate for the platform.
  void ClosePlatformBrowser(PlatformBrowser& browser) {
#if !defined(OS_ANDROID)
    CloseBrowserSynchronously(browser);
#else
    TabModelList::RemoveTabModel(browser.get());
    browser.reset();
#endif  // !defined(OS_ANDROID)
  }

  std::unique_ptr<ProfileSyncServiceHarness> EnableSyncForProfile(
      Profile* profile) {
    std::unique_ptr<ProfileSyncServiceHarness> harness =
        test::InitializeProfileForSync(profile, GetFakeServer()->AsWeakPtr());
    EXPECT_TRUE(harness->SetupSync());

    // If unified consent is enabled, then enable url-keyed-anonymized data
    // collection through the consent service.
    // Note: If unfied consent is not enabled, then UKM will be enabled based on
    // the history sync state.
    unified_consent::UnifiedConsentService* consent_service =
        UnifiedConsentServiceFactory::GetForProfile(profile);
    if (consent_service)
      consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(true);

    return harness;
  }

#if !defined(OS_ANDROID)
  Profile* CreateNonSyncProfile() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath new_path =
        profile_manager->GenerateNextProfileDirectoryPath();
    base::RunLoop run_loop;
    profile_manager->CreateProfileAsync(
        new_path, base::Bind(&UnblockOnProfileCreation, &run_loop),
        base::string16(), std::string());
    run_loop.Run();
    Profile* profile = profile_manager->GetProfileByPath(new_path);
    SetupMockGaiaResponsesForProfile(profile);
    return profile;
  }
#endif  // !defined(OS_ANDROID)

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(UkmBrowserTestBase);
};

class UkmBrowserTest : public UkmBrowserTestBase {
 public:
  UkmBrowserTest() : UkmBrowserTestBase() {}

#if defined(OS_ANDROID)
  void PreRunTestOnMainThread() override {
    // At some point during set-up, Android's TabModelList is populated with a
    // TabModel. However, it is desirable to begin the tests with an empty
    // TabModelList to avoid complicated logic in CreatePlatformBrowser.
    //
    // For example, if the pre-existing TabModel is not deleted and if the first
    // tab created in a test is an incognito tab, then CreatePlatformBrowser
    // would need to remove the pre-existing TabModel and add a new one.
    // Having an empty TabModelList allows us to simply add the appropriate
    // TabModel.
    TabModelList::RemoveTabModel(TabModelList::get(0));
    EXPECT_EQ(0U, TabModelList::size());
  }
#endif  // defined(OS_ANDROID)

 private:
  DISALLOW_COPY_AND_ASSIGN(UkmBrowserTest);
};

class UkmBrowserTestWithSyncTransport : public UkmBrowserTestBase {
 public:
  UkmBrowserTestWithSyncTransport() {}

  void SetUpInProcessBrowserTestFixture() override {
    // This is required to support (fake) secondary-account-signin (based on
    // cookies) in tests. Without this, the real GaiaCookieManagerService would
    // try talking to Google servers which of course wouldn't work in tests.
    test_signin_client_factory_ =
        secondary_account_helper::SetUpSigninClient(&test_url_loader_factory_);
    UkmBrowserTestBase::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
#if defined(OS_CHROMEOS)
    secondary_account_helper::InitNetwork();
#endif  // defined(OS_CHROMEOS)
    UkmBrowserTestBase::SetUpOnMainThread();
  }

 private:
  secondary_account_helper::ScopedSigninClientFactory
      test_signin_client_factory_;

  DISALLOW_COPY_AND_ASSIGN(UkmBrowserTestWithSyncTransport);
};

// This tests if UKM service is enabled/disabled appropriately based on an
// input bool param. The bool reflects if metrics reporting state is
// enabled/disabled via prefs.
#if !defined(OS_ANDROID)
class UkmConsentParamBrowserTest : public UkmBrowserTestBase,
                                   public testing::WithParamInterface<bool> {
 public:
  UkmConsentParamBrowserTest() : UkmBrowserTestBase() {}

  static bool IsMetricsAndCrashReportingEnabled() {
    return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
  }

  // InProcessBrowserTest overrides.
  bool SetUpUserDataDirectory() override {
    local_state_path_ = SetUpUserDataDirectoryForTesting(
        is_metrics_reporting_enabled_initial_value());
    return !local_state_path_.empty();
  }

  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    // IsMetricsReportingEnabled() in non-official builds always returns false.
    // Enable the official build checks so that this test can work in both
    // official and non-official builds.
    ChromeMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
        true);
  }

  bool is_metrics_reporting_enabled_initial_value() const { return GetParam(); }

 private:
  base::FilePath local_state_path_;

  DISALLOW_COPY_AND_ASSIGN(UkmConsentParamBrowserTest);
};
#endif  // !defined(OS_ANDROID)

// Test the reporting of the synced user's birth year and gender.
class UkmBrowserTestWithDemographics
    : public UkmBrowserTestBase,
      public testing::WithParamInterface<test::DemographicsTestParams> {
 public:
  UkmBrowserTestWithDemographics() : UkmBrowserTestBase() {
    test::DemographicsTestParams param = GetParam();
    if (param.enable_feature) {
      scoped_feature_list_.InitWithFeatures(
          // enabled_features
          {DemographicMetricsProvider::kDemographicMetricsReporting,
           ukm::UkmService::kReportUserNoisedUserBirthYearAndGender},
          // disabled_features
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          // enabled_features
          {},
          // disabled_features
          {DemographicMetricsProvider::kDemographicMetricsReporting,
           ukm::UkmService::kReportUserNoisedUserBirthYearAndGender});
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(UkmBrowserTestWithDemographics);
};

// Make sure that UKM is disabled while an incognito window is open.
// Keep in sync with testRegularPlusIncognito in ios/chrome/browser/metrics/
// ukm_egtest.mm and with RegularPlusIncognitoCheck in
// weblayer/browser/ukm_browsertest.cc.
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, RegularPlusIncognitoCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  PlatformBrowser browser1 = CreatePlatformBrowser(profile);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  uint64_t original_client_id = ukm_test_helper.GetClientId();
  EXPECT_NE(0U, original_client_id);

  Profile* incognito_profile = profile->GetPrimaryOTRProfile();
  PlatformBrowser incognito_browser1 =
      CreateIncognitoPlatformBrowser(incognito_profile);
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());

  // Opening another regular browser mustn't enable UKM.
  PlatformBrowser browser2 = CreatePlatformBrowser(profile);
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());

  // Opening and closing another Incognito browser mustn't enable UKM.
  PlatformBrowser incognito_browser2 =
      CreateIncognitoPlatformBrowser(incognito_profile);
  ClosePlatformBrowser(incognito_browser2);
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());

  ClosePlatformBrowser(browser2);
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());

  ClosePlatformBrowser(incognito_browser1);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  // Client ID should not have been reset.
  EXPECT_EQ(original_client_id, ukm_test_helper.GetClientId());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  ClosePlatformBrowser(browser1);
}

// Make sure opening a real window after Incognito doesn't enable UKM.
// Keep in sync with testIncognitoPlusRegular in ios/chrome/browser/metrics/
// ukm_egtest.mm and with IncognitoPlusRegularCheck in
// weblayer/browser/ukm_browsertest.cc.
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, IncognitoPlusRegularCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Profile* incognito_profile = profile->GetPrimaryOTRProfile();
  PlatformBrowser incognito_browser =
      CreateIncognitoPlatformBrowser(incognito_profile);
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());

  PlatformBrowser browser = CreatePlatformBrowser(profile);
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());

  ClosePlatformBrowser(incognito_browser);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  ClosePlatformBrowser(browser);
}

// Make sure that UKM is disabled while a guest profile's window is open.
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, RegularPlusGuestCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* regular_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  uint64_t original_client_id = ukm_test_helper.GetClientId();

  // Create browser for guest profile. Only "off the record" browsers may be
  // opened in this mode.
  Profile* guest_profile = CreateGuestProfile();
  Browser* guest_browser = CreateIncognitoBrowser(guest_profile);
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());

  CloseBrowserSynchronously(guest_browser);
  // TODO(crbug/746076): UKM doesn't actually get re-enabled yet.
  // EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  // Client ID should not have been reset.
  EXPECT_EQ(original_client_id, ukm_test_helper.GetClientId());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(regular_browser);
}
#endif  // !defined(OS_ANDROID)

// Make sure that UKM is disabled while an non-sync profile's window is open.
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, OpenNonSyncCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  uint64_t original_client_id = ukm_test_helper.GetClientId();
  EXPECT_NE(0U, original_client_id);

  Profile* nonsync_profile = CreateNonSyncProfile();
  Browser* nonsync_browser = CreateBrowser(nonsync_profile);
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());

  CloseBrowserSynchronously(nonsync_browser);
  // TODO(crbug/746076): UKM doesn't actually get re-enabled yet.
  // EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  // Client ID should not have been reset.
  EXPECT_EQ(original_client_id, ukm_test_helper.GetClientId());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}
#endif  // !defined(OS_ANDROID)

// Make sure that UKM is disabled when metrics consent is revoked.
// Keep in sync with testMetricsConsent in ios/chrome/browser/metrics/
// ukm_egtest.mm.
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, MetricsConsentCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  PlatformBrowser browser = CreatePlatformBrowser(profile);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  uint64_t original_client_id = ukm_test_helper.GetClientId();
  EXPECT_NE(0U, original_client_id);

  // Make sure there is a persistent log.
  ukm_test_helper.BuildAndStoreLog();
  EXPECT_TRUE(ukm_test_helper.HasUnsentLogs());

  metrics_consent.Update(false);
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_FALSE(ukm_test_helper.HasUnsentLogs());

  metrics_consent.Update(true);

  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  // Client ID should have been reset.
  EXPECT_NE(original_client_id, ukm_test_helper.GetClientId());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  ClosePlatformBrowser(browser);
}

#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, LogProtoData) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  uint64_t original_client_id = ukm_test_helper.GetClientId();
  EXPECT_NE(0U, original_client_id);

  // Make sure there is a persistent log.
  ukm_test_helper.BuildAndStoreLog();
  EXPECT_TRUE(ukm_test_helper.HasUnsentLogs());

  // Check log contents.
  std::unique_ptr<ukm::Report> report = ukm_test_helper.GetUkmReport();
  EXPECT_EQ(original_client_id, report->client_id());
  // Note: The version number reported in the proto may have a suffix, such as
  // "-64-devel", so use use StartsWith() rather than checking for equality.
  EXPECT_TRUE(base::StartsWith(report->system_profile().app_version(),
                               version_info::GetVersionNumber(),
                               base::CompareCase::SENSITIVE));

  EXPECT_EQ(base::SysInfo::HardwareModelName(),
            report->system_profile().hardware().hardware_class());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}
#endif  // !defined(OS_ANDROID)

// TODO(crbug/1016118): Add the remaining test cases.
// Keep this test in sync with testUKMDemographicsReportingWithFeatureEnabled
// and testUKMDemographicsReportingWithFeatureDisabled in
// ios/chrome/browser/metrics/demographics_egtest.mm.
// TODO(1102747): Crashes on android asan.
#if defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_AddSyncedUserBirthYearAndGenderToProtoData \
  DISABLED_AddSyncedUserBirthYearAndGenderToProtoData
#else
#define MAYBE_AddSyncedUserBirthYearAndGenderToProtoData \
  AddSyncedUserBirthYearAndGenderToProtoData
#endif
IN_PROC_BROWSER_TEST_P(UkmBrowserTestWithDemographics,
                       MAYBE_AddSyncedUserBirthYearAndGenderToProtoData) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  test::DemographicsTestParams param = GetParam();
  MetricsConsentOverride metrics_consent(true);

  base::HistogramTester histogram;

  const base::Time now = base::Time::Now();
  test::UpdateNetworkTime(now, g_browser_process->network_time_tracker());
  const int test_birth_year = test::GetMaximumEligibleBirthYear(now);
  const UserDemographicsProto::Gender test_gender =
      UserDemographicsProto::GENDER_FEMALE;

  // Add the test synced user birth year and gender priority prefs to the sync
  // server data.
  test::AddUserBirthYearAndGenderToSyncServer(GetFakeServer()->AsWeakPtr(),
                                              test_birth_year, test_gender);

  Profile* test_profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(test_profile);

  // Make sure that there is only one Profile to allow reporting the user's
  // birth year and gender.
  ASSERT_EQ(1, num_clients());

  PlatformBrowser browser = CreatePlatformBrowser(test_profile);

  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  uint64_t original_client_id = ukm_test_helper.GetClientId();
  EXPECT_NE(0U, original_client_id);

  // Log UKM metrics report.
  ukm_test_helper.BuildAndStoreLog();
  EXPECT_TRUE(ukm_test_helper.HasUnsentLogs());

  // Check the log's content and the histogram.
  std::unique_ptr<ukm::Report> report = ukm_test_helper.GetUkmReport();
  if (param.expect_reported_demographics) {
    EXPECT_EQ(
        test::GetNoisedBirthYear(*test_profile->GetPrefs(), test_birth_year),
        report->user_demographics().birth_year());
    EXPECT_EQ(test_gender, report->user_demographics().gender());
    histogram.ExpectUniqueSample("UKM.UserDemographics.Status",
                                 syncer::UserDemographicsStatus::kSuccess, 1);
  } else {
    EXPECT_FALSE(report->has_user_demographics());
    histogram.ExpectTotalCount("UKM.UserDemographics.Status", /*count=*/0);
  }

#if !defined(OS_CHROMEOS)
  // Sign out the user to revoke all refresh tokens. This prevents any posted
  // tasks from successfully fetching an access token during the tear-down
  // phase and crashing on a DCHECK. See crbug/1102746 for more details.
  harness->SignOutPrimaryAccount();
#endif  // !defined(OS_CHROMEOS)
  ClosePlatformBrowser(browser);
}

#if defined(OS_CHROMEOS)
// Cannot test for the enabled feature on Chrome OS because there are always
// multiple profiles.
static const auto kDemographicsTestParams = testing::Values(
    test::DemographicsTestParams{/*enable_feature=*/false,
                                 /*expect_reported_demographics=*/false});
#else
static const auto kDemographicsTestParams = testing::Values(
    test::DemographicsTestParams{/*enable_feature=*/false,
                                 /*expect_reported_demographics=*/false},
    test::DemographicsTestParams{/*enable_feature=*/true,
                                 /*expect_reported_demographics=*/true});
#endif

INSTANTIATE_TEST_SUITE_P(,
                         UkmBrowserTestWithDemographics,
                         kDemographicsTestParams);

// Verifies that network provider attaches effective connection type correctly
// to the UKM report.
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, NetworkProviderPopulatesSystemProfile) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  // Override network quality to 2G. This should cause the
  // |max_effective_connection_type| in the system profile to be set to 2G.
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_2G);

  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  uint64_t original_client_id = ukm_test_helper.GetClientId();
  EXPECT_NE(0U, original_client_id);

  // Override network quality to 4G. This should cause the
  // |max_effective_connection_type| in the system profile to be set to 4G.
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_4G);

  // Make sure there is a persistent log.
  ukm_test_helper.BuildAndStoreLog();
  EXPECT_TRUE(ukm_test_helper.HasUnsentLogs());
  // Check log contents.
  std::unique_ptr<ukm::Report> report = ukm_test_helper.GetUkmReport();

  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
            report->system_profile().network().min_effective_connection_type());
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_4G,
            report->system_profile().network().max_effective_connection_type());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}
#endif  // !defined(OS_ANDROID)

// Make sure that providing consent doesn't enable UKM when sync is disabled.
// Keep in sync with testConsentAddedButNoSync in ios/chrome/browser/metrics/
// ukm_egtest.mm.
// Flaky on Android crbug.com/1096400
#if defined(OS_ANDROID)
#define MAYBE_ConsentAddedButNoSyncCheck DISABLED_ConsentAddedButNoSyncCheck
#else
#define MAYBE_ConsentAddedButNoSyncCheck ConsentAddedButNoSyncCheck
#endif
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, MAYBE_ConsentAddedButNoSyncCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(false);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  PlatformBrowser browser = CreatePlatformBrowser(profile);
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());

  metrics_consent.Update(true);
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());

  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  ClosePlatformBrowser(browser);
}

// Make sure that extension URLs are disabled when an open sync window
// disables it.
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, SingleDisableExtensionsSyncCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_TRUE(ukm_test_helper.IsExtensionRecordingEnabled());
  uint64_t original_client_id = ukm_test_helper.GetClientId();
  EXPECT_NE(0U, original_client_id);

  ASSERT_TRUE(
      harness->DisableSyncForType(syncer::UserSelectableType::kExtensions));
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_FALSE(ukm_test_helper.IsExtensionRecordingEnabled());

  ASSERT_TRUE(
      harness->EnableSyncForType(syncer::UserSelectableType::kExtensions));
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_TRUE(ukm_test_helper.IsExtensionRecordingEnabled());
  // Client ID should not be reset.
  EXPECT_EQ(original_client_id, ukm_test_helper.GetClientId());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}
#endif  // !defined(OS_ANDROID)

// Make sure that extension URLs are disabled when any open sync window
// disables it.
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, MultiDisableExtensionsSyncCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile1 = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness1 =
      EnableSyncForProfile(profile1);

  Browser* browser1 = CreateBrowser(profile1);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  uint64_t original_client_id = ukm_test_helper.GetClientId();
  EXPECT_NE(0U, original_client_id);

  Profile* profile2 = CreateNonSyncProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness2 =
      EnableSyncForProfile(profile2);
  Browser* browser2 = CreateBrowser(profile2);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_TRUE(ukm_test_helper.IsExtensionRecordingEnabled());
  EXPECT_EQ(original_client_id, ukm_test_helper.GetClientId());

  harness2->DisableSyncForType(syncer::UserSelectableType::kExtensions);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_FALSE(ukm_test_helper.IsExtensionRecordingEnabled());

  harness2->EnableSyncForType(syncer::UserSelectableType::kExtensions);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_TRUE(ukm_test_helper.IsExtensionRecordingEnabled());
  EXPECT_EQ(original_client_id, ukm_test_helper.GetClientId());

  harness2->service()->GetUserSettings()->SetSyncRequested(false);
  harness1->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(browser2);
  CloseBrowserSynchronously(browser1);
}
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, LogsTabId) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  ASSERT_TRUE(embedded_test_server()->Start());
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);
  Browser* sync_browser = CreateBrowser(profile);

  const ukm::UkmSource* first_source =
      NavigateAndGetSource(embedded_test_server()->GetURL("/title1.html"),
                           sync_browser, &ukm_test_helper);

  // Tab ids are incremented starting from 1. Since we started a new sync
  // browser, this is the second tab.
  EXPECT_EQ(2, first_source->navigation_data().tab_id);

  // Ensure the tab id is constant in a single tab.
  const ukm::UkmSource* second_source =
      NavigateAndGetSource(embedded_test_server()->GetURL("/title2.html"),
                           sync_browser, &ukm_test_helper);
  EXPECT_EQ(first_source->navigation_data().tab_id,
            second_source->navigation_data().tab_id);

  // Add a new tab, it should get a new tab id.
  chrome::NewTab(sync_browser);
  const ukm::UkmSource* third_source =
      NavigateAndGetSource(embedded_test_server()->GetURL("/title3.html"),
                           sync_browser, &ukm_test_helper);
  EXPECT_EQ(3, third_source->navigation_data().tab_id);
}
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, LogsPreviousSourceId) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  ASSERT_TRUE(embedded_test_server()->Start());
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);
  Browser* sync_browser = CreateBrowser(profile);

  const ukm::UkmSource* first_source =
      NavigateAndGetSource(embedded_test_server()->GetURL("/title1.html"),
                           sync_browser, &ukm_test_helper);

  const ukm::UkmSource* second_source =
      NavigateAndGetSource(embedded_test_server()->GetURL("/title2.html"),
                           sync_browser, &ukm_test_helper);
  EXPECT_EQ(first_source->id(),
            second_source->navigation_data().previous_source_id);

  // Open a new tab with window.open.
  content::WebContents* opener =
      sync_browser->tab_strip_model()->GetActiveWebContents();
  GURL new_tab_url = embedded_test_server()->GetURL("/title3.html");
  content::TestNavigationObserver waiter(new_tab_url);
  waiter.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecuteScript(
      opener, content::JsReplace("window.open($1)", new_tab_url)));
  waiter.Wait();
  EXPECT_NE(opener, sync_browser->tab_strip_model()->GetActiveWebContents());
  ukm::SourceId new_id = ukm::GetSourceIdForWebContentsDocument(
      sync_browser->tab_strip_model()->GetActiveWebContents());
  ukm::UkmSource* new_tab_source = ukm_test_helper.GetSource(new_id);
  EXPECT_NE(nullptr, new_tab_source);
  EXPECT_EQ(ukm::kInvalidSourceId,
            new_tab_source->navigation_data().previous_source_id);

  // Subsequent navigations within the tab should get a previous_source_id field
  // set.
  const ukm::UkmSource* subsequent_source =
      NavigateAndGetSource(embedded_test_server()->GetURL("/title3.html"),
                           sync_browser, &ukm_test_helper);
  EXPECT_EQ(new_tab_source->id(),
            subsequent_source->navigation_data().previous_source_id);
}
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, LogsOpenerSource) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  ASSERT_TRUE(embedded_test_server()->Start());
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);
  Browser* sync_browser = CreateBrowser(profile);

  const ukm::UkmSource* first_source =
      NavigateAndGetSource(embedded_test_server()->GetURL("/title1.html"),
                           sync_browser, &ukm_test_helper);
  // This tab was not opened by another tab, so it should not have an opener
  // id.
  EXPECT_EQ(ukm::kInvalidSourceId,
            first_source->navigation_data().opener_source_id);

  // Open a new tab with window.open.
  content::WebContents* opener =
      sync_browser->tab_strip_model()->GetActiveWebContents();
  GURL new_tab_url = embedded_test_server()->GetURL("/title2.html");
  content::TestNavigationObserver waiter(new_tab_url);
  waiter.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecuteScript(
      opener, content::JsReplace("window.open($1)", new_tab_url)));
  waiter.Wait();
  EXPECT_NE(opener, sync_browser->tab_strip_model()->GetActiveWebContents());
  ukm::SourceId new_id = ukm::GetSourceIdForWebContentsDocument(
      sync_browser->tab_strip_model()->GetActiveWebContents());
  ukm::UkmSource* new_tab_source = ukm_test_helper.GetSource(new_id);
  EXPECT_NE(nullptr, new_tab_source);
  EXPECT_EQ(first_source->id(),
            new_tab_source->navigation_data().opener_source_id);

  // Subsequent navigations within the tab should not get an opener set.
  const ukm::UkmSource* subsequent_source =
      NavigateAndGetSource(embedded_test_server()->GetURL("/title3.html"),
                           sync_browser, &ukm_test_helper);
  EXPECT_EQ(ukm::kInvalidSourceId,
            subsequent_source->navigation_data().opener_source_id);
}
#endif  // !defined(OS_ANDROID)

// ChromeOS doesn't have the concept of sign-out so this test doesn't make sense
// there.
//
// Flaky on Android: https://crbug.com/1096047.
//
// Make sure that UKM is disabled when the profile signs out of Sync.
// Keep in sync with testSingleSyncSignout in ios/chrome/browser/metrics/
// ukm_egtest.mm.
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, SingleSyncSignoutCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  PlatformBrowser browser = CreatePlatformBrowser(profile);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  uint64_t original_client_id = ukm_test_helper.GetClientId();
  EXPECT_NE(0U, original_client_id);

  harness->SignOutPrimaryAccount();
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_NE(original_client_id, ukm_test_helper.GetClientId());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  ClosePlatformBrowser(browser);
}
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)

// ChromeOS doesn't have the concept of sign-out so this test doesn't make sense
// there. Android doesn't have multiple profiles.
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
// Make sure that UKM is disabled when any profile signs out of Sync.
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, MultiSyncSignoutCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile1 = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness1 =
      EnableSyncForProfile(profile1);

  Browser* browser1 = CreateBrowser(profile1);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  uint64_t original_client_id = ukm_test_helper.GetClientId();
  EXPECT_NE(0U, original_client_id);

  Profile* profile2 = CreateNonSyncProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness2 =
      EnableSyncForProfile(profile2);
  Browser* browser2 = CreateBrowser(profile2);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_EQ(original_client_id, ukm_test_helper.GetClientId());

  harness2->SignOutPrimaryAccount();
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_NE(original_client_id, ukm_test_helper.GetClientId());

  harness2->service()->GetUserSettings()->SetSyncRequested(false);
  harness1->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(browser2);
  CloseBrowserSynchronously(browser1);
}
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)

// Make sure that if history/sync services weren't available when we tried to
// attach listeners, UKM is not enabled.
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, ServiceListenerInitFailedCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);
  ChromeMetricsServiceClient::SetNotificationListenerSetupFailedForTesting(
      true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());
  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}
#endif  // !defined(OS_ANDROID)

// Make sure that UKM is not affected by MetricsReporting Feature (sampling).
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, MetricsReportingCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  // Need to set the Metrics Default to OPT_OUT to trigger MetricsReporting.
  PrefService* local_state = g_browser_process->local_state();
  ForceRecordMetricsReportingDefaultState(local_state,
                                          EnableMetricsDefault::OPT_OUT);
  // Verify that kMetricsReportingFeature is disabled (i.e. other metrics
  // services will be sampled out).
  EXPECT_FALSE(
      base::FeatureList::IsEnabled(internal::kMetricsReportingFeature));

  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}
#endif  // !defined(OS_ANDROID)

// Make sure that pending data is deleted when user deletes history.
// Keep in sync with testHistoryDelete in ios/chrome/browser/metrics/
// ukm_egtest.mm.
// Quite flaky: https://crbug.com/1131541
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, DISABLED_HistoryDeleteCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  PlatformBrowser browser = CreatePlatformBrowser(profile);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  uint64_t original_client_id = ukm_test_helper.GetClientId();
  EXPECT_NE(0U, original_client_id);

  const ukm::SourceId kDummySourceId = 0x54321;
  ukm_test_helper.RecordSourceForTesting(kDummySourceId);
  EXPECT_TRUE(ukm_test_helper.HasSource(kDummySourceId));

  ClearBrowsingData(profile);
  // Other sources may already have been recorded since the data was cleared,
  // but the dummy source should be gone.
  EXPECT_FALSE(ukm_test_helper.HasSource(kDummySourceId));
  // Client ID should NOT be reset.
  EXPECT_EQ(original_client_id, ukm_test_helper.GetClientId());
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  ClosePlatformBrowser(browser);
}

// On ChromeOS, the test profile starts with a primary account already set, so
// this test doesn't apply.
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTestWithSyncTransport,
                       NotEnabledForSecondaryAccountSync) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  // Signing in (without making the account Chrome's primary one or explicitly
  // setting up Sync) causes the Sync machinery to start up in standalone
  // transport mode.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      test::InitializeProfileForSync(profile, GetFakeServer()->AsWeakPtr());
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);

  secondary_account_helper::SignInSecondaryAccount(
      profile, &test_url_loader_factory_, "secondary_user@email.com");
  ASSERT_NE(syncer::SyncService::TransportState::DISABLED,
            sync_service->GetTransportState());
  ASSERT_TRUE(harness->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            sync_service->GetTransportState());
  ASSERT_FALSE(sync_service->IsSyncFeatureEnabled());

  // History Sync is not active, but (maybe surprisingly) TYPED_URLS is still
  // considered part of the "chosen" data types, since the user hasn't disabled
  // it.
  ASSERT_FALSE(sync_service->GetActiveDataTypes().Has(syncer::TYPED_URLS));
  ASSERT_FALSE(sync_service->GetActiveDataTypes().Has(
      syncer::HISTORY_DELETE_DIRECTIVES));
  ASSERT_TRUE(sync_service->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));

  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());
}
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_P(UkmConsentParamBrowserTest, GroupPolicyConsentCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  // Note we are not using the synthetic MetricsConsentOverride since we are
  // testing directly from prefs.

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);

  // The input param controls whether we set the prefs related to group policy
  // enabled or not. Based on its value, we should report the same value for
  // both if reporting is enabled and if UKM service is enabled.
  bool is_enabled = is_metrics_reporting_enabled_initial_value();
  EXPECT_EQ(is_enabled,
            UkmConsentParamBrowserTest::IsMetricsAndCrashReportingEnabled());
  EXPECT_EQ(is_enabled, ukm_test_helper.IsRecordingEnabled());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
// Verify UKM is enabled/disabled for both potential settings of group policy.
INSTANTIATE_TEST_SUITE_P(UkmConsentParamBrowserTests,
                         UkmConsentParamBrowserTest,
                         testing::Bool());
#endif  // !defined(OS_ANDROID)

// Verify that sources kept alive in-memory will be discarded by UKM service in
// one reporting cycle after the web contents are destroyed when the tab is
// closed or when the user navigated away in the same tab.
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, EvictObsoleteSources) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);
  Browser* sync_browser = CreateBrowser(profile);
  ASSERT_TRUE(embedded_test_server()->Start());

  ukm::SourceId source_id1 = ukm::kInvalidSourceId;
  ukm::SourceId source_id2 = ukm::kInvalidSourceId;

  const std::vector<GURL> test_urls = {
      embedded_test_server()->GetURL("/title1.html"),
      embedded_test_server()->GetURL("/title2.html"),
      embedded_test_server()->GetURL("/title3.html")};

  // Open a blank new tab.
  AddTabAtIndexToBrowser(sync_browser, 1, GURL(url::kAboutBlankURL),
                         ui::PAGE_TRANSITION_TYPED, true);
  // Gather source id from the NavigationHandle assigned to navigations that
  // start with the expected URL.
  content::NavigationHandleObserver tab_1_observer(
      sync_browser->tab_strip_model()->GetActiveWebContents(), test_urls[0]);
  // Navigate to a test URL in this new tab.
  ui_test_utils::NavigateToURL(sync_browser, test_urls[0]);
  // Get the source id associated to the last committed navigation, which could
  // differ from the id from WebContents for example if the site executes a
  // same-document navigation (e.g. history.pushState/replaceState). This
  // navigation source id is the one marked as obsolete by UKM recorder.
  source_id1 = ukm::ConvertToSourceId(tab_1_observer.navigation_id(),
                                      ukm::SourceIdType::NAVIGATION_ID);

  // The UKM report contains this newly-created source.
  ukm_test_helper.BuildAndStoreLog();
  std::unique_ptr<ukm::Report> report = ukm_test_helper.GetUkmReport();
  bool has_source_id1 = false;
  bool has_source_id2 = false;
  for (const auto& s : report->sources()) {
    has_source_id1 |= s.id() == source_id1;
    has_source_id2 |= s.id() == source_id2;
  }
  EXPECT_TRUE(has_source_id1);
  EXPECT_FALSE(has_source_id2);

  // Navigate to another URL in a new tab.
  AddTabAtIndexToBrowser(sync_browser, 2, GURL(url::kAboutBlankURL),
                         ui::PAGE_TRANSITION_TYPED, true);
  content::NavigationHandleObserver tab_2_observer(
      sync_browser->tab_strip_model()->GetActiveWebContents(), test_urls[1]);
  ui_test_utils::NavigateToURL(sync_browser, test_urls[1]);
  source_id2 = ukm::ConvertToSourceId(tab_2_observer.navigation_id(),
                                      ukm::SourceIdType::NAVIGATION_ID);

  // The next report should again contain source 1 because the tab is still
  // alive, and also source 2 associated to the new tab that has just been
  // opened.
  ukm_test_helper.BuildAndStoreLog();
  report = ukm_test_helper.GetUkmReport();
  has_source_id1 = false;
  has_source_id2 = false;
  for (const auto& s : report->sources()) {
    has_source_id1 |= s.id() == source_id1;
    has_source_id2 |= s.id() == source_id2;
  }
  EXPECT_TRUE(has_source_id1);
  EXPECT_TRUE(has_source_id2);

  // Close the tab corresponding to source 1, this should mark source 1 as
  // obsolete. Next report will still contain source 1 because we might have
  // associated entries before it was closed.
  sync_browser->tab_strip_model()->CloseWebContentsAt(
      1, TabStripModel::CloseTypes::CLOSE_NONE);

  ukm_test_helper.BuildAndStoreLog();
  report = ukm_test_helper.GetUkmReport();
  has_source_id1 = false;
  has_source_id2 = false;
  for (const auto& s : report->sources()) {
    has_source_id1 |= s.id() == source_id1;
    has_source_id2 |= s.id() == source_id2;
  }
  EXPECT_TRUE(has_source_id1);
  EXPECT_TRUE(has_source_id2);

  // Navigate to a new URL in the current tab, this will mark source 2 that was
  // in the current tab as obsolete.
  ui_test_utils::NavigateToURL(sync_browser, test_urls[2]);

  // The previous report was the last one that could potentially contain entries
  // for source 1. Source 1 is thus no longer included in future reports. This
  // report will still contain source 2 because we might have associated entries
  // since the last report.
  ukm_test_helper.BuildAndStoreLog();
  report = ukm_test_helper.GetUkmReport();
  has_source_id1 = false;
  has_source_id2 = false;
  for (const auto& s : report->sources()) {
    has_source_id1 |= s.id() == source_id1;
    has_source_id2 |= s.id() == source_id2;
  }
  EXPECT_FALSE(has_source_id1);
  EXPECT_TRUE(has_source_id2);

  // Neither source 1 or source 2 is alive anymore.
  ukm_test_helper.BuildAndStoreLog();
  report = ukm_test_helper.GetUkmReport();
  has_source_id1 = false;
  has_source_id2 = false;
  for (const auto& s : report->sources()) {
    has_source_id1 |= s.id() == source_id1;
    has_source_id2 |= s.id() == source_id2;
  }
  EXPECT_FALSE(has_source_id1);
  EXPECT_FALSE(has_source_id2);

  CloseBrowserSynchronously(sync_browser);
}
#endif  // !defined(OS_ANDROID)

// Verify that correct sources are marked as obsolete when same-document
// navigation happens.
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest,
                       MarkObsoleteSourcesSameDocumentNavigation) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);
  Browser* sync_browser = CreateBrowser(profile);
  ASSERT_TRUE(embedded_test_server()->Start());

  // First navigation.
  const ukm::SourceId source_id1 =
      NavigateAndGetSource(embedded_test_server()->GetURL("/title1.html"),
                           sync_browser, &ukm_test_helper)
          ->id();

  EXPECT_FALSE(ukm_test_helper.IsSourceObsolete(source_id1));

  // Cross-document navigation where the previous navigation is cross-document.
  const ukm::SourceId source_id2 =
      NavigateAndGetSource(embedded_test_server()->GetURL("/title2.html"),
                           sync_browser, &ukm_test_helper)
          ->id();
  EXPECT_TRUE(ukm_test_helper.IsSourceObsolete(source_id1));
  EXPECT_FALSE(ukm_test_helper.IsSourceObsolete(source_id2));

  // Same-document navigation where the previous navigation is cross-document.
  const ukm::SourceId source_id3 =
      NavigateAndGetSource(embedded_test_server()->GetURL("/title2.html#a"),
                           sync_browser, &ukm_test_helper)
          ->id();
  EXPECT_TRUE(ukm_test_helper.IsSourceObsolete(source_id1));
  EXPECT_FALSE(ukm_test_helper.IsSourceObsolete(source_id2));
  EXPECT_FALSE(ukm_test_helper.IsSourceObsolete(source_id3));

  // Same-document navigation where the previous navigation is same-document.
  const ukm::SourceId source_id4 =
      NavigateAndGetSource(embedded_test_server()->GetURL("/title2.html#b"),
                           sync_browser, &ukm_test_helper)
          ->id();
  EXPECT_TRUE(ukm_test_helper.IsSourceObsolete(source_id1));
  EXPECT_FALSE(ukm_test_helper.IsSourceObsolete(source_id2));
  EXPECT_TRUE(ukm_test_helper.IsSourceObsolete(source_id3));
  EXPECT_FALSE(ukm_test_helper.IsSourceObsolete(source_id4));

  // Cross-document navigation where the previous navigation is same-document.
  NavigateAndGetSource(embedded_test_server()->GetURL("/title1.html"),
                       sync_browser, &ukm_test_helper)
      ->id();
  EXPECT_TRUE(ukm_test_helper.IsSourceObsolete(source_id1));
  EXPECT_TRUE(ukm_test_helper.IsSourceObsolete(source_id2));
  EXPECT_TRUE(ukm_test_helper.IsSourceObsolete(source_id3));
  EXPECT_TRUE(ukm_test_helper.IsSourceObsolete(source_id4));
}
#endif  // !defined(OS_ANDROID)

// Verify that sources are not marked as obsolete by a new navigation that does
// not commit.
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, NotMarkSourcesIfNavigationNotCommitted) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);
  Browser* sync_browser = CreateBrowser(profile);
  ASSERT_TRUE(embedded_test_server()->Start());

  // An example navigation that commits.
  const GURL test_url_with_commit =
      embedded_test_server()->GetURL("/title1.html");

  // An example of a navigation returning "204 No Content" which does not commit
  // (i.e. the WebContents stays at the existing URL).
  const GURL test_url_no_commit =
      embedded_test_server()->GetURL("/page204.html");

  // Get the source id from the committed navigation.
  const ukm::SourceId source_id =
      NavigateAndGetSource(test_url_with_commit, sync_browser, &ukm_test_helper)
          ->id();

  // Initial default state.
  EXPECT_FALSE(ukm_test_helper.IsSourceObsolete(source_id));

  // New navigation did not commit, thus the source should still be kept alive.
  ui_test_utils::NavigateToURL(sync_browser, test_url_no_commit);
  EXPECT_FALSE(ukm_test_helper.IsSourceObsolete(source_id));
}
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, DebugUiRenders) {
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);
  PlatformBrowser browser = CreatePlatformBrowser(profile);

  ukm::UkmService* ukm_service(GetUkmService());
  EXPECT_TRUE(ukm_service->IsSamplingEnabled());

  // chrome://ukm
  const GURL debug_url(content::GetWebUIURLString(content::kChromeUIUkmHost));

  content::TestNavigationObserver waiter(debug_url);
  waiter.WatchExistingWebContents();
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, debug_url));
  waiter.WaitForNavigationFinished();
}
#endif  // !defined(OS_ANDROID)

}  // namespace metrics
