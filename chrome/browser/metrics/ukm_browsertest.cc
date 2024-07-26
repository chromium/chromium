// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/browser/metrics/testing/metrics_reporting_pref_helper.h"
#include "chrome/browser/metrics/testing/sync_metrics_test_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "components/metrics/demographics/demographic_metrics_provider.h"
#include "components/metrics/demographics/demographic_metrics_test_utils.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/service/sync_token_status.h"
#include "components/sync/test/fake_server_network_resources.h"
#include "components/ukm/ukm_recorder_observer.h"
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

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#else
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "content/public/browser/web_contents.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

namespace metrics {
namespace {

class TestTabModel;

#if !BUILDFLAG(IS_ANDROID)
typedef Browser* PlatformBrowser;
#else
typedef std::unique_ptr<TestTabModel> PlatformBrowser;
#endif  // !BUILDFLAG(IS_ANDROID)

// Clears the specified data using BrowsingDataRemover.
void ClearBrowsingData(Profile* profile) {
  content::BrowsingDataRemover* remover = profile->GetBrowsingDataRemover();
  content::BrowsingDataRemoverCompletionObserver observer(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(),
      chrome_browsing_data_remover::DATA_TYPE_HISTORY,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, &observer);
  observer.BlockUntilCompletion();
  // Make sure HistoryServiceObservers have a chance to be notified.
  content::RunAllTasksUntilIdle();
}

ukm::UkmService* GetUkmService() {
  return g_browser_process->GetMetricsServicesManager()->GetUkmService();
}

class TestUkmRecorderObserver : public ukm::UkmRecorderObserver {
 public:
  explicit TestUkmRecorderObserver(ukm::UkmRecorderImpl* ukm_recorder)
      : ukm_recorder_(ukm_recorder) {
    ukm_recorder_->AddUkmRecorderObserver(base::flat_set<uint64_t>(), this);
  }

  ~TestUkmRecorderObserver() override {
    ukm_recorder_->RemoveUkmRecorderObserver(this);
  }

  void OnEntryAdded(ukm::mojom::UkmEntryPtr entry) override {}

  void OnUpdateSourceURL(ukm::SourceId source_id,
                         const std::vector<GURL>& urls) override {}

  void OnPurgeRecordingsWithUrlScheme(const std::string& url_scheme) override {}

  void OnPurge() override {}

  void ExpectAllowedStateChanged(ukm::UkmConsentState expected_state) {
    expected_allowed_ = expected_state;
    base::RunLoop loop;
    quit_closure_ = loop.QuitClosure();
    loop.Run();
  }

  void OnUkmAllowedStateChanged(ukm::UkmConsentState allowed_state) override {
    if (allowed_state == expected_allowed_) {
      std::move(quit_closure_).Run();
    }
  }

 private:
  ukm::UkmConsentState expected_allowed_;
  base::OnceClosure quit_closure_;
  raw_ptr<ukm::UkmRecorderImpl> ukm_recorder_;
};

#if BUILDFLAG(IS_ANDROID)

// ActivityType that doesn't restore tabs on cold start.
// Any type other than kTabbed is fine.
const auto TEST_ACTIVITY_TYPE = chrome::android::ActivityType::kCustomTab;

// TestTabModel provides a means of creating a tab associated with a given
// profile. The new tab can then be added to Android's TabModelList.
class TestTabModel : public TabModel {
 public:
  explicit TestTabModel(Profile* profile)
      : TabModel(profile, TEST_ACTIVITY_TYPE),
        web_contents_(content::WebContents::Create(
            content::WebContents::CreateParams(GetProfile()))) {}

  ~TestTabModel() override = default;

  // TabModel:
  int GetTabCount() const override { return 0; }
  int GetActiveIndex() const override { return 0; }
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const override {
    return nullptr;
  }
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
                 content::WebContents* web_contents,
                 bool select) override {}
  void HandlePopupNavigation(TabAndroid* parent,
                             NavigateParams* params) override {}
  content::WebContents* CreateNewTabForDevTools(const GURL& url) override {
    return nullptr;
  }
  bool IsSessionRestoreInProgress() const override { return false; }
  bool IsActiveModel() const override { return false; }
  void AddObserver(TabModelObserver* observer) override {}
  void RemoveObserver(TabModelObserver* observer) override {}
  int GetTabCountNavigatedInTimeWindow(
      const base::Time& begin_time,
      const base::Time& end_time) const override {
    return 0;
  }
  void CloseTabsNavigatedInTimeWindow(const base::Time& begin_time,
                                      const base::Time& end_time) override {}

 private:
  // The WebContents associated with this tab's profile.
  std::unique_ptr<content::WebContents> web_contents_;
};
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

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
    // Explicitly enable UKM and disable metrics reporting. Disabling metrics
    // reporting should affect only UMA--not UKM.
    scoped_feature_list_.InitWithFeatures({ukm::kUkmFeature},
                                          {internal::kMetricsReportingFeature});
  }

  UkmBrowserTestBase(const UkmBrowserTestBase&) = delete;
  UkmBrowserTestBase& operator=(const UkmBrowserTestBase&) = delete;

#if !BUILDFLAG(IS_ANDROID)
  ukm::UkmSource* NavigateAndGetSource(const GURL& url,
                                       Browser* browser,
                                       ukm::UkmTestHelper* ukm_test_helper) {
    content::NavigationHandleObserver observer(
        browser->tab_strip_model()->GetActiveWebContents(), url);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, url));
    const ukm::SourceId source_id = ukm::ConvertToSourceId(
        observer.navigation_id(), ukm::SourceIdType::NAVIGATION_ID);
    return ukm_test_helper->GetSource(source_id);
  }
#endif  // !BUILDFLAG(IS_ANDROID)

 protected:
  // Creates and returns a platform-appropriate browser for |profile|.
  PlatformBrowser CreatePlatformBrowser(Profile* profile) {
#if !BUILDFLAG(IS_ANDROID)
    return CreateBrowser(profile);
#else
    std::unique_ptr<TestTabModel> tab_model =
        std::make_unique<TestTabModel>(profile);
    TabModelList::AddTabModel(tab_model.get());
    EXPECT_TRUE(content::NavigateToURL(tab_model->GetActiveWebContents(),
                                       GURL("about:blank")));
    return tab_model;
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  // Creates a platform-appropriate incognito browser for |profile|.
  PlatformBrowser CreateIncognitoPlatformBrowser(Profile* profile) {
    EXPECT_TRUE(profile->IsOffTheRecord());
#if !BUILDFLAG(IS_ANDROID)
    return CreateIncognitoBrowser(profile);
#else
    return CreatePlatformBrowser(profile);
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  // Closes |browser| in a way that is appropriate for the platform.
  void ClosePlatformBrowser(PlatformBrowser& browser) {
#if !BUILDFLAG(IS_ANDROID)
    CloseBrowserSynchronously(browser);
#else
    TabModelList::RemoveTabModel(browser.get());
    browser.reset();
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  std::unique_ptr<SyncServiceImplHarness> EnableSyncForProfile(
      Profile* profile) {
    std::unique_ptr<SyncServiceImplHarness> harness =
        test::InitializeProfileForSync(profile, GetFakeServer()->AsWeakPtr());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Apps sync is controlled by a dedicated preference on Lacros,
    // corresponding to the Apps toggle in OS Sync settings.
    if (base::FeatureList::IsEnabled(syncer::kSyncChromeOSAppsToggleSharing)) {
      syncer::SyncUserSettings* user_settings =
          harness->service()->GetUserSettings();
      // Turn on App-sync in OS Sync.
      user_settings->SetAppsSyncEnabledByOs(true);
    }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    EXPECT_TRUE(harness->SetupSync());

    // If unified consent is enabled, then enable url-keyed-anonymized data
    // collection through the consent service.
    // Note: If unfied consent is not enabled, then UKM will be enabled based on
    // the history sync state.
    unified_consent::UnifiedConsentService* consent_service =
        UnifiedConsentServiceFactory::GetForProfile(profile);
    if (consent_service) {
      consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
    }

    return harness;
  }

#if !BUILDFLAG(IS_ANDROID)
  Profile* CreateNonSyncProfile() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath new_path =
        profile_manager->GenerateNextProfileDirectoryPath();
    Profile& profile =
        profiles::testing::CreateProfileSync(profile_manager, new_path);
    SetupMockGaiaResponsesForProfile(&profile);
    return &profile;
  }
#endif  // !BUILDFLAG(IS_ANDROID)

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class UkmBrowserTest : public UkmBrowserTestBase {
 public:
  UkmBrowserTest() = default;

  UkmBrowserTest(const UkmBrowserTest&) = delete;
  UkmBrowserTest& operator=(const UkmBrowserTest&) = delete;

#if BUILDFLAG(IS_ANDROID)
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
    EXPECT_EQ(1U, TabModelList::models().size());
    TabModelList::RemoveTabModel(TabModelList::models()[0]);
    EXPECT_EQ(0U, TabModelList::models().size());
  }
#endif  // BUILDFLAG(IS_ANDROID)
};

class UkmBrowserTestWithSyncTransport : public UkmBrowserTestBase {
 public:
  UkmBrowserTestWithSyncTransport() = default;

  UkmBrowserTestWithSyncTransport(const UkmBrowserTestWithSyncTransport&) =
      delete;
  UkmBrowserTestWithSyncTransport& operator=(
      const UkmBrowserTestWithSyncTransport&) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    // This is required to support (fake) secondary-account-signin (based on
    // cookies) in tests. Without this, the real GaiaCookieManagerService would
    // try talking to Google servers which of course wouldn't work in tests.
    test_signin_client_subscription_ =
        secondary_account_helper::SetUpSigninClient(&test_url_loader_factory_);
    UkmBrowserTestBase::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    secondary_account_helper::InitNetwork();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    UkmBrowserTestBase::SetUpOnMainThread();
  }

 private:
  base::CallbackListSubscription test_signin_client_subscription_;
};

// This tests if UKM service is enabled/disabled appropriately based on an
// input bool param. The bool reflects if metrics reporting state is
// enabled/disabled via prefs.
#if !BUILDFLAG(IS_ANDROID)
class UkmConsentParamBrowserTest : public UkmBrowserTestBase,
                                   public testing::WithParamInterface<bool> {
 public:
  UkmConsentParamBrowserTest() = default;

  UkmConsentParamBrowserTest(const UkmConsentParamBrowserTest&) = delete;
  UkmConsentParamBrowserTest& operator=(const UkmConsentParamBrowserTest&) =
      delete;

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
    InProcessBrowserTest::CreatedBrowserMainParts(parts);
    // IsMetricsReportingEnabled() in non-official builds always returns false.
    // Enable the official build checks so that this test can work in both
    // official and non-official builds.
    ChromeMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
        true);
  }

  bool is_metrics_reporting_enabled_initial_value() const { return GetParam(); }

 private:
  base::FilePath local_state_path_;
};
#endif  // !BUILDFLAG(IS_ANDROID)

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
          {kDemographicMetricsReporting,
           ukm::kReportUserNoisedUserBirthYearAndGender},
          // disabled_features
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          // enabled_features
          {},
          // disabled_features
          {kDemographicMetricsReporting,
           ukm::kReportUserNoisedUserBirthYearAndGender});
    }
  }

  UkmBrowserTestWithDemographics(const UkmBrowserTestWithDemographics&) =
      delete;
  UkmBrowserTestWithDemographics& operator=(
      const UkmBrowserTestWithDemographics&) = delete;

  PrefService* local_state() { return g_browser_process->local_state(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Make sure that UKM is disabled while an incognito window is open.
// Keep in sync with testRegularPlusIncognito in ios/chrome/browser/metrics/
// ukm_egtest.mm.
// Disabled on Android due to flakiness. See crbug.com/355609356.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_RegularPlusIncognitoCheck DISABLED_RegularPlusIncognitoCheck
#else
#define MAYBE_RegularPlusIncognitoCheck RegularPlusIncognitoCheck
#endif
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, MAYBE_RegularPlusIncognitoCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
      EnableSyncForProfile(profile);

  PlatformBrowser browser1 = CreatePlatformBrowser(profile);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  uint64_t original_client_id = ukm_test_helper.GetClientId();
  EXPECT_NE(0U, original_client_id);

  Profile* incognito_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
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

  ClosePlatformBrowser(browser1);
}

// Make sure opening a real window after Incognito doesn't enable UKM.
// Keep in sync with testIncognitoPlusRegular in ios/chrome/browser/metrics/
// ukm_egtest.mm.
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, IncognitoPlusRegularCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
      EnableSyncForProfile(profile);

  Profile* incognito_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  PlatformBrowser incognito_browser =
      CreateIncognitoPlatformBrowser(incognito_profile);
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());

  PlatformBrowser browser = CreatePlatformBrowser(profile);
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());

  ClosePlatformBrowser(incognito_browser);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());

  ClosePlatformBrowser(browser);
}

// Make sure that UKM is disabled while a guest profile's window is open.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, RegularPlusGuestCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
      EnableSyncForProfile(profile);

  Browser* regular_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  uint64_t original_client_id = ukm_test_helper.GetClientId();

  // Create browser for guest profile.
  Browser* guest_browser = InProcessBrowserTest::CreateGuestBrowser();
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());

  CloseBrowserSynchronously(guest_browser);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  // Client ID should not have been reset.
  EXPECT_EQ(original_client_id, ukm_test_helper.GetClientId());

  CloseBrowserSynchronously(regular_browser);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

// ProfilePicker and System profile do not exist on Chrome Ash and on Android.
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
// Displaying the ProfilePicker implicitly creates a System Profile.
// System Profile shouldn't have any effect on the UKM Enable Status.
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, ProfilePickerCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
      EnableSyncForProfile(profile);

  Browser* regular_browser = CreateBrowser(profile);
  ASSERT_TRUE(ukm_test_helper.IsRecordingEnabled());

  // ProfilePicker creates a SystemProfile.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  profiles::testing::WaitForPickerWidgetCreated();

  Profile* system_profile =
      g_browser_process->profile_manager()->GetProfileByPath(
          ProfileManager::GetSystemProfilePath());
  ASSERT_TRUE(system_profile);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());

  ProfilePicker::Hide();
  profiles::testing::WaitForPickerClosed();
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());

  CloseBrowserSynchronously(regular_browser);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

// Not applicable to Android as it doesn't have multiple profiles.
#if !BUILDFLAG(IS_ANDROID)
// Make sure that UKM is disabled while an non-sync profile's window is open.
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, OpenNonSyncCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  uint64_t original_client_id = ukm_test_helper.GetClientId();
  EXPECT_NE(0U, original_client_id);

  Profile* nonsync_profile = CreateNonSyncProfile();
  Browser* nonsync_browser = CreateBrowser(nonsync_profile);
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());

  CloseBrowserSynchronously(nonsync_browser);
  // TODO(crbug.com/40530708): UKM doesn't actually get re-enabled yet.
  // EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Client ID should not have been reset.
  EXPECT_EQ(original_client_id, ukm_test_helper.GetClientId());
#else
  // When flag is enabled, removing MSBB consent will cause the client id to be
  // reset. In this case a new profile with sync turned off is added which also
  // removes consent.
  EXPECT_NE(original_client_id, ukm_test_helper.GetClientId());
#endif

  CloseBrowserSynchronously(sync_browser);
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Make sure that UKM is disabled when metrics consent is revoked.
// Keep in sync with testMetricsConsent in ios/chrome/browser/metrics/
// ukm_egtest.mm.
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, MetricsConsentCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
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

  ClosePlatformBrowser(browser);
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, LogProtoData) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
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

  CloseBrowserSynchronously(sync_browser);
}
#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/40103988): Add the remaining test cases.
// Keep this test in sync with testUKMDemographicsReportingWithFeatureEnabled
// and testUKMDemographicsReportingWithFeatureDisabled in
// ios/chrome/browser/metrics/demographics_egtest.mm.
IN_PROC_BROWSER_TEST_P(UkmBrowserTestWithDemographics,
                       AddSyncedUserBirthYearAndGenderToProtoData) {
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

  Profile* test_profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
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

  // Check the log's content.
  std::unique_ptr<ukm::Report> report = ukm_test_helper.GetUkmReport();
  if (param.expect_reported_demographics) {
    EXPECT_EQ(test::GetNoisedBirthYear(local_state(), test_birth_year),
              report->user_demographics().birth_year());
    EXPECT_EQ(test_gender, report->user_demographics().gender());
  } else {
    EXPECT_FALSE(report->has_user_demographics());
  }

#if !BUILDFLAG(IS_CHROMEOS)
  // Sign out the user to revoke all refresh tokens. This prevents any posted
  // tasks from successfully fetching an access token during the tear-down
  // phase and crashing on a DCHECK. See crbug/1102746 for more details.
  harness->SignOutPrimaryAccount();
#endif  // !BUILDFLAG(IS_CHROMEOS)
  ClosePlatformBrowser(browser);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, NetworkProviderPopulatesSystemProfile) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  // Override network quality to 2G. This should cause the
  // |max_effective_connection_type| in the system profile to be set to 2G.
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_2G);

  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
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

  CloseBrowserSynchronously(sync_browser);
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Verifies that install date is attached.
// Disabled on Android due to flakiness. See crbug.com/355609356.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_InstallDateProviderPopulatesSystemProfile \
  DISABLED_InstallDateProviderPopulatesSystemProfile
#else
#define MAYBE_InstallDateProviderPopulatesSystemProfile \
  InstallDateProviderPopulatesSystemProfile
#endif
IN_PROC_BROWSER_TEST_F(UkmBrowserTest,
                       MAYBE_InstallDateProviderPopulatesSystemProfile) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetInt64(prefs::kInstallDate, 123456);

  ukm::UkmTestHelper ukm_test_helper(GetUkmService());

  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
      EnableSyncForProfile(profile);

  PlatformBrowser browser = CreatePlatformBrowser(profile);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  uint64_t original_client_id = ukm_test_helper.GetClientId();
  EXPECT_NE(0U, original_client_id);

  // Make sure there is a persistent log.
  ukm_test_helper.BuildAndStoreLog();
  EXPECT_TRUE(ukm_test_helper.HasUnsentLogs());
  // Check log contents.
  std::unique_ptr<ukm::Report> report = ukm_test_helper.GetUkmReport();

  // Rounded from the 123456 we set earlier, to nearest hour.
  EXPECT_EQ(122400, report->system_profile().install_date());

  ClosePlatformBrowser(browser);
}

// Make sure that providing consent doesn't enable UKM when sync is disabled.
// Keep in sync with testConsentAddedButNoSync in ios/chrome/browser/metrics/
// ukm_egtest.mm and consentAddedButNoSyncCheck in chrome/android/javatests/src/
// org/chromium/chrome/browser/sync/UkmTest.java.
// Flaky on Android crbug.com/1096400
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ConsentAddedButNoSyncCheck DISABLED_ConsentAddedButNoSyncCheck
#else
#define MAYBE_ConsentAddedButNoSyncCheck ConsentAddedButNoSyncCheck
#endif
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, MAYBE_ConsentAddedButNoSyncCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(false);

  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  PlatformBrowser browser = CreatePlatformBrowser(profile);
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());

  metrics_consent.Update(true);
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());

  std::unique_ptr<SyncServiceImplHarness> harness =
      EnableSyncForProfile(profile);
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());

  ClosePlatformBrowser(browser);
}

// Make sure that extension URLs are disabled when an open sync window
// disables it.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, SingleDisableExtensionsSyncCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
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

  CloseBrowserSynchronously(sync_browser);
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Make sure that extension URLs are disabled when any open sync window
// disables it.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, MultiDisableExtensionsSyncCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile1 = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness1 =
      EnableSyncForProfile(profile1);

  Browser* browser1 = CreateBrowser(profile1);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  uint64_t original_client_id = ukm_test_helper.GetClientId();
  EXPECT_NE(0U, original_client_id);

  Profile* profile2 = CreateNonSyncProfile();
  std::unique_ptr<SyncServiceImplHarness> harness2 =
      EnableSyncForProfile(profile2);
  Browser* browser2 = CreateBrowser(profile2);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_TRUE(ukm_test_helper.IsExtensionRecordingEnabled());
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(original_client_id, ukm_test_helper.GetClientId());
#else
  // If the feature is enabled, then the client id will have been reset. The
  // client id is reset when the second profile is initially created without any
  // sync preferences enabled or MSBB. With the feature, any consent on to off
  // change for MSBB or App Sync will cause the client id to reset.
  EXPECT_NE(original_client_id, ukm_test_helper.GetClientId());
  original_client_id = ukm_test_helper.GetClientId();
#endif

  ASSERT_TRUE(
      harness2->DisableSyncForType(syncer::UserSelectableType::kExtensions));
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_FALSE(ukm_test_helper.IsExtensionRecordingEnabled());

  ASSERT_TRUE(
      harness2->EnableSyncForType(syncer::UserSelectableType::kExtensions));
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_TRUE(ukm_test_helper.IsExtensionRecordingEnabled());
  EXPECT_EQ(original_client_id, ukm_test_helper.GetClientId());

  CloseBrowserSynchronously(browser2);
  CloseBrowserSynchronously(browser1);
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, LogsTabId) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  ASSERT_TRUE(embedded_test_server()->Start());
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
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
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, LogsPreviousSourceId) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  ASSERT_TRUE(embedded_test_server()->Start());
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
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
  EXPECT_TRUE(content::ExecJs(
      opener, content::JsReplace("window.open($1)", new_tab_url)));
  waiter.Wait();
  EXPECT_NE(opener, sync_browser->tab_strip_model()->GetActiveWebContents());
  ukm::SourceId new_id = sync_browser->tab_strip_model()
                             ->GetActiveWebContents()
                             ->GetPrimaryMainFrame()
                             ->GetPageUkmSourceId();
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
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, LogsOpenerSource) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  ASSERT_TRUE(embedded_test_server()->Start());
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
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
  EXPECT_TRUE(content::ExecJs(
      opener, content::JsReplace("window.open($1)", new_tab_url)));
  waiter.Wait();
  EXPECT_NE(opener, sync_browser->tab_strip_model()->GetActiveWebContents());
  ukm::SourceId new_id = sync_browser->tab_strip_model()
                             ->GetActiveWebContents()
                             ->GetPrimaryMainFrame()
                             ->GetPageUkmSourceId();
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
#endif  // !BUILDFLAG(IS_ANDROID)

// ChromeOS doesn't have the concept of sign-out so this test doesn't make sense
// there.
//
// Flaky on Android: https://crbug.com/1096047.
//
// Make sure that UKM is disabled when the profile signs out of Sync.
// Keep in sync with testSingleSyncSignout in ios/chrome/browser/metrics/
// ukm_egtest.mm and singleSyncSignoutCheck in chrome/android/javatests/src/org/
// chromium/chrome/browser/sync/UkmTest.java.
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, SingleSyncSignoutCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
      EnableSyncForProfile(profile);

  PlatformBrowser browser = CreatePlatformBrowser(profile);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  uint64_t original_client_id = ukm_test_helper.GetClientId();
  EXPECT_NE(0U, original_client_id);

  harness->SignOutPrimaryAccount();
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_NE(original_client_id, ukm_test_helper.GetClientId());

  ClosePlatformBrowser(browser);
}
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

// ChromeOS doesn't have the concept of sign-out so this test doesn't make sense
// there. Android doesn't have multiple profiles.
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
// Make sure that UKM is disabled when any profile signs out of Sync.
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, MultiSyncSignoutCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile1 = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness1 =
      EnableSyncForProfile(profile1);

  Browser* browser1 = CreateBrowser(profile1);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  uint64_t original_client_id = ukm_test_helper.GetClientId();
  EXPECT_NE(0U, original_client_id);

  Profile* profile2 = CreateNonSyncProfile();
  std::unique_ptr<SyncServiceImplHarness> harness2 =
      EnableSyncForProfile(profile2);
  Browser* browser2 = CreateBrowser(profile2);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_EQ(original_client_id, ukm_test_helper.GetClientId());

  harness2->SignOutPrimaryAccount();
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_NE(original_client_id, ukm_test_helper.GetClientId());

  CloseBrowserSynchronously(browser2);
  CloseBrowserSynchronously(browser1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

// Make sure that if history/sync services weren't available when we tried to
// attach listeners, UKM is not enabled.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, ServiceListenerInitFailedCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);
  ChromeMetricsServiceClient::SetNotificationListenerSetupFailedForTesting(
      true);

  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());
  CloseBrowserSynchronously(sync_browser);
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Make sure that UKM is not affected by MetricsReporting Feature (sampling).
#if !BUILDFLAG(IS_ANDROID)
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

  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());

  CloseBrowserSynchronously(sync_browser);
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Make sure that pending data is deleted when user deletes history.
//
// Keep in sync with testHistoryDelete in ios/chrome/browser/metrics/
// ukm_egtest.mm and testHistoryDeleteCheck in chrome/android/javatests/src/org/
// chromium/chrome/browser/metrics/UkmTest.java.
//
// Flaky on Android: https://crbug.com/1131541.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_HistoryDeleteCheck DISABLED_HistoryDeleteCheck
#else
#define MAYBE_HistoryDeleteCheck HistoryDeleteCheck
#endif
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, MAYBE_HistoryDeleteCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
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

  ClosePlatformBrowser(browser);
}

// On ChromeOS, the test profile starts with a primary account already set, so
// this test doesn't apply.
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTestWithSyncTransport,
                       NotEnabledForSecondaryAccountSync) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);

  // Signing in (without granting sync consent or explicitly setting up Sync)
  // should trigger starting the Sync machinery in standalone transport mode.
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
      test::InitializeProfileForSync(profile, GetFakeServer()->AsWeakPtr());
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);

  secondary_account_helper::SignInUnconsentedAccount(
      profile, &test_url_loader_factory_, "secondary_user@email.com");
  ASSERT_NE(syncer::SyncService::TransportState::DISABLED,
            sync_service->GetTransportState());
  ASSERT_TRUE(harness->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            sync_service->GetTransportState());
  ASSERT_FALSE(sync_service->IsSyncFeatureEnabled());

  // History Sync is not active.
  ASSERT_FALSE(sync_service->GetActiveDataTypes().Has(syncer::HISTORY));
  ASSERT_FALSE(sync_service->GetActiveDataTypes().Has(
      syncer::HISTORY_DELETE_DIRECTIVES));

  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());
}
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(UkmConsentParamBrowserTest, GroupPolicyConsentCheck) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  // Note we are not using the synthetic MetricsConsentOverride since we are
  // testing directly from prefs.

  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);

  // The input param controls whether we set the prefs related to group policy
  // enabled or not. Based on its value, we should report the same value for
  // both if reporting is enabled and if UKM service is enabled.
  bool is_enabled = is_metrics_reporting_enabled_initial_value();
  EXPECT_EQ(is_enabled,
            UkmConsentParamBrowserTest::IsMetricsAndCrashReportingEnabled());
  EXPECT_EQ(is_enabled, ukm_test_helper.IsRecordingEnabled());

  CloseBrowserSynchronously(sync_browser);
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
// Verify UKM is enabled/disabled for both potential settings of group policy.
INSTANTIATE_TEST_SUITE_P(UkmConsentParamBrowserTests,
                         UkmConsentParamBrowserTest,
                         testing::Bool());
#endif  // !BUILDFLAG(IS_ANDROID)

// Verify that sources kept alive in-memory will be discarded by UKM service in
// one reporting cycle after the web contents are destroyed when the tab is
// closed or when the user navigated away in the same tab.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, EvictObsoleteSources) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
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
  ASSERT_TRUE(AddTabAtIndexToBrowser(sync_browser, 1, GURL(url::kAboutBlankURL),
                                     ui::PAGE_TRANSITION_TYPED, true));
  // Gather source id from the NavigationHandle assigned to navigations that
  // start with the expected URL.
  content::NavigationHandleObserver tab_1_observer(
      sync_browser->tab_strip_model()->GetActiveWebContents(), test_urls[0]);
  // Navigate to a test URL in this new tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(sync_browser, test_urls[0]));
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
  ASSERT_TRUE(AddTabAtIndexToBrowser(sync_browser, 2, GURL(url::kAboutBlankURL),
                                     ui::PAGE_TRANSITION_TYPED, true));
  content::NavigationHandleObserver tab_2_observer(
      sync_browser->tab_strip_model()->GetActiveWebContents(), test_urls[1]);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(sync_browser, test_urls[1]));
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
      1, TabCloseTypes::CLOSE_NONE);

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(sync_browser, test_urls[2]));

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
#endif  // !BUILDFLAG(IS_ANDROID)

// Verify that correct sources are marked as obsolete when same-document
// navigation happens.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest,
                       MarkObsoleteSourcesSameDocumentNavigation) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
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
#endif  // !BUILDFLAG(IS_ANDROID)

// Verify that sources are not marked as obsolete by a new navigation that does
// not commit.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, NotMarkSourcesIfNavigationNotCommitted) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(sync_browser, test_url_no_commit));
  EXPECT_FALSE(ukm_test_helper.IsSourceObsolete(source_id));
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, DebugUiRenders) {
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
      EnableSyncForProfile(profile);
  PlatformBrowser browser = CreatePlatformBrowser(profile);

  ukm::UkmService* ukm_service(GetUkmService());
  EXPECT_TRUE(ukm_service->IsSamplingConfigured());

  // chrome://ukm
  const GURL debug_url(content::GetWebUIURLString(content::kChromeUIUkmHost));

  content::TestNavigationObserver waiter(debug_url);
  waiter.WatchExistingWebContents();
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, debug_url));
  waiter.WaitForNavigationFinished();
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(UkmBrowserTest, AllowedStateChanged) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  MetricsConsentOverride metrics_consent(true);
  Profile* test_profile = ProfileManager::GetLastUsedProfileIfLoaded();
  std::unique_ptr<SyncServiceImplHarness> harness =
      EnableSyncForProfile(test_profile);

  CreatePlatformBrowser(test_profile);

  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_TRUE(g_browser_process->GetMetricsServicesManager()
                  ->IsUkmAllowedForAllProfiles());

  TestUkmRecorderObserver observer(GetUkmService());
  unified_consent::UnifiedConsentService* consent_service =
      UnifiedConsentServiceFactory::GetForProfile(test_profile);
  consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(false);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_FALSE(g_browser_process->GetMetricsServicesManager()
                   ->IsUkmAllowedForAllProfiles());
  // Expect nothing to be consented to.
  observer.ExpectAllowedStateChanged(ukm::UkmConsentState());
#else
  // ChromeOS has a different behavior compared to other platforms.
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_TRUE(g_browser_process->GetMetricsServicesManager()
                  ->IsUkmAllowedForAllProfiles());
  observer.ExpectAllowedStateChanged(ukm::UkmConsentState({ukm::APPS}));
#endif

  consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_TRUE(g_browser_process->GetMetricsServicesManager()
                  ->IsUkmAllowedForAllProfiles());
  observer.ExpectAllowedStateChanged(ukm::UkmConsentState::All());
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
class UkmBrowserTestForAppConsent : public UkmBrowserTestBase {
 public:
  UkmBrowserTestForAppConsent() = default;
};

IN_PROC_BROWSER_TEST_F(UkmBrowserTestForAppConsent, MetricsClientEnablement) {
  ukm::UkmService* ukm_service = GetUkmService();
  ukm::UkmTestHelper ukm_test_helper(ukm_service);
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  unified_consent::UnifiedConsentService* consent_service =
      UnifiedConsentServiceFactory::GetForProfile(profile);
  std::unique_ptr<SyncServiceImplHarness> harness =
      EnableSyncForProfile(profile);

  // All consents are on.
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_TRUE(ukm_service->recording_enabled(ukm::MSBB));
  EXPECT_TRUE(ukm_service->recording_enabled(ukm::EXTENSIONS));
  EXPECT_TRUE(ukm_service->recording_enabled(ukm::APPS));

  // Turn off MSBB consent.
  consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(false);

  // Still have AppKM consent.
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_FALSE(ukm_service->recording_enabled(ukm::MSBB));
  EXPECT_FALSE(ukm_service->recording_enabled(ukm::EXTENSIONS));
  EXPECT_TRUE(ukm_service->recording_enabled(ukm::APPS));

  // Turn off App-sync.
  auto* user_settings = harness->service()->GetUserSettings();
  auto registered_os_sync_types =
      user_settings->GetRegisteredSelectableOsTypes();
  registered_os_sync_types.Remove(syncer::UserSelectableOsType::kOsApps);
  user_settings->SetSelectedOsTypes(false, registered_os_sync_types);

  // UKM recording is now disabled since MSBB and App-sync consent
  // has been removed.
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_FALSE(ukm_service->recording_enabled(ukm::MSBB));
  EXPECT_FALSE(ukm_service->recording_enabled(ukm::EXTENSIONS));
  EXPECT_FALSE(ukm_service->recording_enabled(ukm::APPS));
}

IN_PROC_BROWSER_TEST_F(UkmBrowserTestForAppConsent,
                       ClientIdResetWhenConsentRemoved) {
  ukm::UkmService* ukm_service = GetUkmService();
  ukm::UkmTestHelper ukm_test_helper(ukm_service);
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  unified_consent::UnifiedConsentService* consent_service =
      UnifiedConsentServiceFactory::GetForProfile(profile);
  std::unique_ptr<SyncServiceImplHarness> harness =
      EnableSyncForProfile(profile);
  const auto original_client_id = ukm_test_helper.GetClientId();
  EXPECT_NE(0ul, original_client_id);

  // All consents are on.
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
  EXPECT_TRUE(ukm_service->recording_enabled(ukm::MSBB));
  EXPECT_TRUE(ukm_service->recording_enabled(ukm::EXTENSIONS));
  EXPECT_TRUE(ukm_service->recording_enabled(ukm::APPS));

  // Turn off MSBB consent.
  consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(false);

  EXPECT_FALSE(ukm_service->recording_enabled(ukm::MSBB));

  // Client ID should reset when MSBB is disabled.
  const auto app_sync_client_id = ukm_test_helper.GetClientId();
  EXPECT_NE(original_client_id, app_sync_client_id);

  // Turn off app sync.
  auto* user_settings = harness->service()->GetUserSettings();
  auto registered_os_sync_types =
      user_settings->GetRegisteredSelectableOsTypes();
  registered_os_sync_types.Remove(syncer::UserSelectableOsType::kOsApps);
  user_settings->SetSelectedOsTypes(false, registered_os_sync_types);

  EXPECT_FALSE(ukm_service->recording_enabled(ukm::APPS));

  // Client ID should reset when app sync is disable.
  const auto final_client_id = ukm_test_helper.GetClientId();
  EXPECT_NE(app_sync_client_id, final_client_id);
}

IN_PROC_BROWSER_TEST_F(UkmBrowserTestForAppConsent,
                       EnsurePurgeOnConsentChange) {
  ukm::UkmService* ukm_service = GetUkmService();
  ukm::UkmTestHelper ukm_test_helper(ukm_service);
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  unified_consent::UnifiedConsentService* consent_service =
      UnifiedConsentServiceFactory::GetForProfile(profile);
  std::unique_ptr<SyncServiceImplHarness> harness =
      EnableSyncForProfile(profile);
  Browser* sync_browser = CreateBrowser(profile);
  ASSERT_TRUE(embedded_test_server()->Start());

  const std::vector<GURL> test_urls = {
      embedded_test_server()->GetURL("/title1.html"),
      embedded_test_server()->GetURL("/title2.html"),
      embedded_test_server()->GetURL("/title3.html")};

  // Verify all consents are enabled.
  EXPECT_TRUE(ukm_service->recording_enabled(ukm::MSBB));
  EXPECT_TRUE(ukm_service->recording_enabled(ukm::EXTENSIONS));
  EXPECT_TRUE(ukm_service->recording_enabled(ukm::APPS));

  int tab_index = 1;
  // Generate MSBB ukm entries by navigating to some test webpages.
  for (const auto& url : test_urls) {
    ASSERT_TRUE(AddTabAtIndexToBrowser(sync_browser, tab_index++,
                                       GURL(url::kAboutBlankURL),
                                       ui::PAGE_TRANSITION_TYPED, true));
    NavigateAndGetSource(url, sync_browser, &ukm_test_helper);
  }

  // Revoke MSBB consent.
  consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(false);

  // Verify that MSBB consent was revoked.
  EXPECT_FALSE(ukm_service->recording_enabled(ukm::MSBB));
  EXPECT_FALSE(ukm_service->recording_enabled(ukm::EXTENSIONS));
  EXPECT_TRUE(ukm_service->recording_enabled(ukm::APPS));

  ukm_test_helper.BuildAndStoreLog();
  const std::unique_ptr<ukm::Report> report = ukm_test_helper.GetUkmReport();

  // Verify that the only sources in the report are APP_ID.
  // NOTE(crbug/1395143): It was noticed that there was an APP_ID source
  // generated despite not being explicitly created. No entries are associated
  // with it though.
  for (int i = 0; i < report->sources_size(); ++i) {
    const auto id = report->sources(i).id();
    EXPECT_EQ(ukm::GetSourceIdType(id), ukm::SourceIdType::APP_ID);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace metrics
