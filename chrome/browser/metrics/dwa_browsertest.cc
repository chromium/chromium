// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/browser/metrics/testing/metrics_consent_override.h"
#include "chrome/browser/metrics/testing/sync_metrics_test_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/metrics/dwa/dwa_entry_builder.h"
#include "components/metrics/dwa/dwa_recorder.h"
#include "components/metrics/dwa/dwa_service.h"
#include "components/metrics/private_metrics/private_metrics_features.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/unified_consent/unified_consent_service.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "third_party/federated_compute/src/fcp/confidentialcompute/crypto.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#else
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#endif

namespace metrics::dwa {

#if !BUILDFLAG(IS_ANDROID)
typedef Browser* PlatformBrowser;
#else
typedef std::unique_ptr<TestTabModel> PlatformBrowser;
#endif  // !BUILDFLAG(IS_ANDROID)

DwaService* GetDwaService() {
  return g_browser_process->GetMetricsServicesManager()->GetDwaService();
}

bool IsDwaAllowedForAllProfiles() {
  return g_browser_process->GetMetricsServicesManager()
      ->IsDwaAllowedForAllProfiles();
}

// Test fixture that provides access to some DWA internals.
class DwaBrowserTest : public SyncTest {
 public:
  DwaBrowserTest() : SyncTest(SINGLE_CLIENT) {
    // Explicitly enable DWA and disable metrics reporting. Disabling metrics
    // reporting should affect only UMA--not DWA.
    scoped_feature_list_.InitWithFeatures(
        {dwa::kDwaFeature, private_metrics::kPrivateMetricsFeature},
        {internal::kMetricsReportingFeature});
  }

  DwaBrowserTest(const DwaBrowserTest&) = delete;
  DwaBrowserTest& operator=(const DwaBrowserTest&) = delete;

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
    initial_tab_model_ = TabModelList::models()[0].get();
    TabModelList::RemoveTabModel(initial_tab_model_);
    EXPECT_EQ(0U, TabModelList::models().size());
  }

  void PostRunTestOnMainThread() override {
    // Restore the initial tab model so the browser can shut down cleanly.
    TabModelList::AddTabModel(initial_tab_model_);
  }
#endif  // BUILDFLAG(IS_ANDROID)

  void AssertDwaIsEnabledAndAllowed() const {
    ASSERT_TRUE(metrics::dwa::DwaRecorder::Get()->IsEnabled());
    ASSERT_TRUE(IsDwaAllowedForAllProfiles());
  }

  void AssertDwaRecorderHasMetrics() const {
    ASSERT_TRUE(metrics::dwa::DwaRecorder::Get()->HasEntries());
  }

  void ExpectDwaIsDisabledAndDisallowed() const {
    EXPECT_FALSE(metrics::dwa::DwaRecorder::Get()->IsEnabled());
    EXPECT_FALSE(IsDwaAllowedForAllProfiles());
  }

  void ExpectDwaIsEnabledAndAllowed() const {
    EXPECT_TRUE(metrics::dwa::DwaRecorder::Get()->IsEnabled());
    EXPECT_TRUE(IsDwaAllowedForAllProfiles());
  }

  void ExpectDwaRecorderIsEmpty() const {
    EXPECT_FALSE(metrics::dwa::DwaRecorder::Get()->HasEntries());
  }

  void RecordTestDwaEntryMetric() {
    ::dwa::DwaEntryBuilder builder("Kangaroo.Jumped");
    builder.SetContent("https://adtech.com");
    builder.SetMetric("Length", 5);
    builder.Record(dwa::DwaRecorder::Get());
  }

  void RecordTestMetricsAndAssertMetricsRecorded() {
    RecordTestDwaEntryMetric();
    AssertDwaRecorderHasMetrics();
  }

  void SetupDwaService() {
    fcp::confidential_compute::MessageDecryptor decryptor;
    auto recipient_public_key =
        decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
    GetDwaService()->SetEncryptionPublicKeyForTesting(
        recipient_public_key.value());
    GetDwaService()->SetEncryptionPublicKeyVerifierForTesting(
        base::BindRepeating([](const fcp::confidential_compute::OkpCwt&)
                                -> bool { return true; }));
  }

  void SetMsbbConsentState(Profile* profile, bool consent_state) {
    unified_consent::UnifiedConsentService* consent_service =
        UnifiedConsentServiceFactory::GetForProfile(profile);
    ASSERT_NE(consent_service, nullptr);

    if (consent_service) {
      consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(
          consent_state);
    }
  }

  void SetExtensionsConsentState(Profile* profile, bool consent_state) {
    unified_consent::UnifiedConsentService* consent_service =
        UnifiedConsentServiceFactory::GetForProfile(profile);
    ASSERT_NE(consent_service, nullptr);

    std::unique_ptr<SyncServiceImplHarness> harness =
        test::InitializeProfileForSync(profile, GetFakeServer()->AsWeakPtr());
    EXPECT_TRUE(harness->SetupSync());

    if (consent_state) {
      ASSERT_TRUE(harness->EnableSelectableType(
          syncer::UserSelectableType::kExtensions));
    } else {
      ASSERT_TRUE(harness->DisableSelectableType(
          syncer::UserSelectableType::kExtensions));
    }
  }

#if !BUILDFLAG(IS_CHROMEOS)
  void SetAppsConsentState(Profile* profile, bool consent_state) {
    unified_consent::UnifiedConsentService* consent_service =
        UnifiedConsentServiceFactory::GetForProfile(profile);
    ASSERT_NE(consent_service, nullptr);

    std::unique_ptr<SyncServiceImplHarness> harness =
        test::InitializeProfileForSync(profile, GetFakeServer()->AsWeakPtr());
    EXPECT_TRUE(harness->SetupSync());

    if (consent_state) {
      ASSERT_TRUE(
          harness->EnableSelectableType(syncer::UserSelectableType::kApps));
    } else {
      ASSERT_TRUE(
          harness->DisableSelectableType(syncer::UserSelectableType::kApps));
    }
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

 protected:
  std::unique_ptr<SyncServiceImplHarness> EnableSyncForProfile(
      Profile* profile) {
    std::unique_ptr<SyncServiceImplHarness> harness =
        test::InitializeProfileForSync(profile, GetFakeServer()->AsWeakPtr());
    EXPECT_TRUE(harness->SetupSync());

    // If unified consent is enabled, then enable url-keyed-anonymized data
    // collection through the consent service.
    // Note: If unified consent is not enabled, then DWA will be enabled based
    // on the history sync state.
    SetMsbbConsentState(profile, true);

    return harness;
  }

  // Creates and returns a platform-appropriate browser for |profile|.
  PlatformBrowser CreatePlatformBrowser(Profile* profile) {
#if !BUILDFLAG(IS_ANDROID)
    return CreateBrowser(profile);
#else
    std::unique_ptr<TestTabModel> tab_model =
        std::make_unique<TestTabModel>(profile);
    tab_model->SetWebContentsList(
        {content::WebContents::Create(
             content::WebContents::CreateParams(profile))
             .release()});
    TabModelList::AddTabModel(tab_model.get());
    EXPECT_TRUE(content::NavigateToURL(tab_model->GetActiveWebContents(),
                                       GURL("about:blank")));
    return tab_model;
#endif
  }

  // Creates a platform-appropriate incognito browser for |profile|.
  PlatformBrowser CreateIncognitoPlatformBrowser(Profile* profile) {
    EXPECT_TRUE(profile->IsOffTheRecord());
#if !BUILDFLAG(IS_ANDROID)
    return CreateIncognitoBrowser(profile);
#else
    // On Android, an incognito platform is the same as a regular platform
    // browser but with an incognito profile. The incognito profile is validated
    // with profile->IsOffTheRecord().
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

#if BUILDFLAG(IS_ANDROID)
  raw_ptr<TabModel> initial_tab_model_;
#endif  // !BUILDFLAG(IS_ANDROID)
};

// LINT.IfChange(DwaServiceCheck)
IN_PROC_BROWSER_TEST_F(DwaBrowserTest, DwaServiceCheck) {
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);
  SetupDwaService();

  dwa::DwaService* dwa_service = GetDwaService();
  dwa::DwaRecorder* dwa_recorder = metrics::dwa::DwaRecorder::Get();

  PlatformBrowser browser = CreatePlatformBrowser(profile);
  ASSERT_TRUE(dwa_recorder->IsEnabled());

  // Records a DWA entry metric.
  RecordTestDwaEntryMetric();
  EXPECT_TRUE(dwa_recorder->HasEntries());
  EXPECT_FALSE(dwa_service->unsent_log_store()->has_unsent_logs());

  GetDwaService()->Flush(
      metrics::MetricsLogsEventManager::CreateReason::kPeriodic);
  ExpectDwaRecorderIsEmpty();
  EXPECT_TRUE(dwa_service->unsent_log_store()->has_unsent_logs());

  ClosePlatformBrowser(browser);
}
// LINT.ThenChange(/ios/chrome/browser/metrics/model/dwa_egtest.mm:DwaServiceCheck)

// Make sure that DWA is disabled and purged while an incognito window is open.
// LINT.IfChange(RegularBrowserPlusIncognitoCheck)
IN_PROC_BROWSER_TEST_F(DwaBrowserTest, RegularBrowserPlusIncognitoCheck) {
  dwa::DwaRecorder* dwa_recorder = metrics::dwa::DwaRecorder::Get();
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  // DWA should be enabled and capable of recording metrics when opening the
  // first regular browser.
  PlatformBrowser browser1 = CreatePlatformBrowser(profile);
  ASSERT_TRUE(dwa_recorder->IsEnabled());
  RecordTestDwaEntryMetric();
  EXPECT_TRUE(dwa_recorder->HasEntries());

  // Opening an incognito browser should disable DwaRecorder and metrics should
  // be purged.
  Profile* incognito_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  PlatformBrowser incognito_browser1 =
      CreateIncognitoPlatformBrowser(incognito_profile);
  ASSERT_FALSE(dwa_recorder->IsEnabled());
  EXPECT_FALSE(dwa_recorder->HasEntries());
  RecordTestDwaEntryMetric();
  EXPECT_FALSE(dwa_recorder->HasEntries());

  // Opening another regular browser should not enable DWA.
  PlatformBrowser browser2 = CreatePlatformBrowser(profile);
  ASSERT_FALSE(dwa_recorder->IsEnabled());
  RecordTestDwaEntryMetric();
  EXPECT_FALSE(dwa_recorder->HasEntries());

  // Opening and closing another Incognito browser must not enable DWA.
  PlatformBrowser incognito_browser2 =
      CreateIncognitoPlatformBrowser(incognito_profile);
  ClosePlatformBrowser(incognito_browser2);
  ASSERT_FALSE(dwa_recorder->IsEnabled());
  RecordTestDwaEntryMetric();
  EXPECT_FALSE(dwa_recorder->HasEntries());

  ClosePlatformBrowser(browser2);
  ASSERT_FALSE(dwa_recorder->IsEnabled());
  RecordTestDwaEntryMetric();
  EXPECT_FALSE(dwa_recorder->HasEntries());

  // Closing all incognito browsers should enable DwaRecorder and we should be
  // able to log metrics again.
  ClosePlatformBrowser(incognito_browser1);
  ASSERT_TRUE(dwa_recorder->IsEnabled());
  EXPECT_FALSE(dwa_recorder->HasEntries());
  RecordTestDwaEntryMetric();
  EXPECT_TRUE(dwa_recorder->HasEntries());

  ClosePlatformBrowser(browser1);
}
// LINT.ThenChange(/ios/chrome/browser/metrics/model/dwa_egtest.mm:RegularBrowserPlusIncognitoCheck)

// Make sure opening a regular browser after Incognito doesn't enable DWA.
// LINT.IfChange(IncognitoPlusRegularBrowserCheck)
IN_PROC_BROWSER_TEST_F(DwaBrowserTest, IncognitoPlusRegularBrowserCheck) {
  dwa::DwaRecorder* dwa_recorder = metrics::dwa::DwaRecorder::Get();
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  Profile* incognito_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  PlatformBrowser incognito_browser =
      CreateIncognitoPlatformBrowser(incognito_profile);
  ASSERT_FALSE(dwa_recorder->IsEnabled());

  PlatformBrowser browser = CreatePlatformBrowser(profile);
  ASSERT_FALSE(dwa_recorder->IsEnabled());

  ClosePlatformBrowser(incognito_browser);
  ASSERT_TRUE(dwa_recorder->IsEnabled());

  ClosePlatformBrowser(browser);
}
// LINT.ThenChange(/ios/chrome/browser/metrics/model/dwa_egtest.mm:IncognitoPlusRegularBrowserCheck)

// This test ensures that disabling MSBB UKM consent disables and purges DWA.
// Additionally ensures that DWA is disabled until all UKM consents are enabled.
// LINT.IfChange(UkmMsbbConsentChangeCheck)
IN_PROC_BROWSER_TEST_F(DwaBrowserTest, UkmConsentChangeCheck_Msbb) {
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetric();
  AssertDwaIsEnabledAndAllowed();
  AssertDwaRecorderHasMetrics();

  // Turn off MSBB consent.
  SetMsbbConsentState(profile, /*consent_state=*/false);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turn on MSBB consent.
  SetMsbbConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsEnabledAndAllowed();
  ExpectDwaRecorderIsEmpty();

  // Validate DWA entries and page load events are able to be recorded when all
  // consents are enabled.
  RecordTestMetricsAndAssertMetricsRecorded();
}
// LINT.ThenChange(/ios/chrome/browser/metrics/model/dwa_egtest.mm:UkmMsbbConsentChangeCheck)

// Not enabled on Android because on Android, kApps and kExtensions is not
// registered through UserSelectableType.
#if !BUILDFLAG(IS_ANDROID)
// This test ensures that disabling Extensions UKM consent disables and purges
// DWA. Additionally ensures that DWA is disabled until all UKM consents are
// enabled.
IN_PROC_BROWSER_TEST_F(DwaBrowserTest, UkmConsentChangeCheck_Extensions) {
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetric();
  AssertDwaIsEnabledAndAllowed();
  AssertDwaRecorderHasMetrics();

  // Turn off Extension consent.
  SetExtensionsConsentState(profile, /*consent_state=*/false);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turn on Extension consent.
  SetExtensionsConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsEnabledAndAllowed();
  ExpectDwaRecorderIsEmpty();

  // Validate DWA entries and page load events are able to be recorded when all
  // consents are enabled.
  RecordTestMetricsAndAssertMetricsRecorded();
}

// Not enabled on ChromeOS because on ChromeOS, kApps is not registered through
// UserSelectableType but rather through OS settings. This test ensures that
// disabling Apps UKM consent disables and purges DWA. Additionally ensures that
// DWA is disabled until all UKM consents are enabled.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(DwaBrowserTest, UkmConsentChangeCheck_Apps) {
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetric();
  AssertDwaIsEnabledAndAllowed();
  AssertDwaRecorderHasMetrics();

  // Turn off Apps consent.
  SetAppsConsentState(profile, /*consent_state=*/false);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turn on Apps consent.
  SetAppsConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsEnabledAndAllowed();
  ExpectDwaRecorderIsEmpty();

  // Validate DWA entries and page load events are able to be recorded when all
  // consents are enabled.
  RecordTestMetricsAndAssertMetricsRecorded();
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// This test ensures that disabling MSBB and Extensions UKM consents disables
// and purges DWA. Additionally ensures that DWA is disabled until all UKM
// consents are enabled.
IN_PROC_BROWSER_TEST_F(DwaBrowserTest,
                       UkmConsentChangeCheck_MsbbAndExtensions) {
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetric();
  AssertDwaIsEnabledAndAllowed();
  AssertDwaRecorderHasMetrics();

  // Turn off MSBB and Extension consent.
  SetMsbbConsentState(profile, /*consent_state=*/false);
  SetExtensionsConsentState(profile, /*consent_state=*/false);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turning on MSBB should not enable DWA because Extensions consent is still
  // disabled.
  SetMsbbConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turn on Extensions consent.
  SetExtensionsConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsEnabledAndAllowed();
  ExpectDwaRecorderIsEmpty();

  // Validate DWA entries and page load events are able to be recorded when all
  // consents are enabled.
  RecordTestMetricsAndAssertMetricsRecorded();
}

// Not enabled on ChromeOS because on ChromeOS, kApps is not registered through
// UserSelectableType but rather through OS settings. This test ensures that
// disabling MSBB and Apps UKM consents disables and purges DWA. Additionally
// ensures that DWA is disabled until all UKM consents are enabled.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(DwaBrowserTest, UkmConsentChangeCheck_MsbbAndApps) {
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetric();
  AssertDwaIsEnabledAndAllowed();
  AssertDwaRecorderHasMetrics();

  // Turn off MSBB and Apps consent.
  SetMsbbConsentState(profile, /*consent_state=*/false);
  SetAppsConsentState(profile, /*consent_state=*/false);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turning on MSBB should not enable DWA because Apps consent is still
  // disabled.
  SetMsbbConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turn on Apps consent.
  SetAppsConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsEnabledAndAllowed();
  ExpectDwaRecorderIsEmpty();

  // Validate DWA entries and page load events are able to be recorded when all
  // consents are enabled.
  RecordTestMetricsAndAssertMetricsRecorded();
}

// This test ensures that disabling Extensions and Apps UKM consents disables
// and purges DWA. Additionally ensures that DWA is disabled until all UKM
// consents are enabled.
IN_PROC_BROWSER_TEST_F(DwaBrowserTest,
                       UkmConsentChangeCheck_ExtensionsAndApps) {
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetric();
  AssertDwaIsEnabledAndAllowed();
  AssertDwaRecorderHasMetrics();

  // Turn off Extensions and Apps consent.
  SetExtensionsConsentState(profile, /*consent_state=*/false);
  SetAppsConsentState(profile, /*consent_state=*/false);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turning on Extensions should not enable DWA because Apps consent is still
  // disabled.
  SetExtensionsConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turn on Apps consent.
  SetAppsConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsEnabledAndAllowed();
  ExpectDwaRecorderIsEmpty();

  // Validate DWA entries and page load events are able to be recorded when all
  // consents are enabled.
  RecordTestMetricsAndAssertMetricsRecorded();
}

// This test ensures that disabling MSBB, Extensions, and Apps UKM consents
// disables and purges DWA. Additionally ensures that DWA is disabled until all
// UKM consents are enabled.
IN_PROC_BROWSER_TEST_F(DwaBrowserTest,
                       UkmConsentChangeCheck_MsbbAndExtensionsAndApps) {
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetric();
  AssertDwaIsEnabledAndAllowed();
  AssertDwaRecorderHasMetrics();

  // Turn off MSBB, Apps, and Extensions consent.
  SetMsbbConsentState(profile, /*consent_state=*/false);
  SetExtensionsConsentState(profile, /*consent_state=*/false);
  SetAppsConsentState(profile, /*consent_state=*/false);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turning on Apps consent should not enable DWA because MSBB and Extensions
  // consent are still disabled.
  SetAppsConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turning on Extensions should not enable DWA because MSBB is still disabled.
  SetExtensionsConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turning on MSBB consent should enable DWA.
  SetMsbbConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsEnabledAndAllowed();
  ExpectDwaRecorderIsEmpty();

  // Validate DWA entries and page load events are able to be recorded when all
  // consents are enabled.
  RecordTestMetricsAndAssertMetricsRecorded();
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace metrics::dwa
