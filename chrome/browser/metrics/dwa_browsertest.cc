// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/browser/metrics/testing/sync_metrics_test_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "components/metrics/dwa/dwa_entry_builder.h"
#include "components/metrics/dwa/dwa_recorder.h"
#include "components/metrics/dwa/dwa_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/unified_consent/unified_consent_service.h"
#include "content/public/test/browser_test.h"

// TODO(crbug.com/391901366): Add #else preprocessor directive to support
// Android browser tests.
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#endif

namespace metrics::dwa {

// TODO(crbug.com/391901366): Add #else preprocessor directive to support
// Android browser tests.
#if !BUILDFLAG(IS_ANDROID)
typedef Browser* PlatformBrowser;
#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/391901366): Remove preprocessor directive to enable for
// Android browser tests.
#if !BUILDFLAG(IS_ANDROID)
DwaService* GetDwaService() {
  return g_browser_process->GetMetricsServicesManager()->GetDwaService();
}
#endif  // !BUILDFLAG(IS_ANDROID)

bool IsDwaAllowedForAllProfiles() {
  return g_browser_process->GetMetricsServicesManager()
      ->IsDwaAllowedForAllProfiles();
}

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
        /*value=*/nullptr);
  }

  void Update(bool state) {
    state_ = state;
    // Trigger rechecking of metrics state.
    g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(
        /*may_upload=*/true);
  }

 private:
  bool state_;
};

// Test fixture that provides access to some DWA internals.
class DwaBrowserTest : public SyncTest {
 public:
  DwaBrowserTest() : SyncTest(SINGLE_CLIENT) {
    // Explicitly enable DWA and disable metrics reporting. Disabling metrics
    // reporting should affect only UMA--not DWA.
    scoped_feature_list_.InitWithFeatures({dwa::kDwaFeature},
                                          {internal::kMetricsReportingFeature});
  }

  DwaBrowserTest(const DwaBrowserTest&) = delete;
  DwaBrowserTest& operator=(const DwaBrowserTest&) = delete;

  void AssertDwaIsEnabledAndAllowed() const {
    ASSERT_TRUE(metrics::dwa::DwaRecorder::Get()->IsEnabled());
    ASSERT_TRUE(IsDwaAllowedForAllProfiles());
  }

  void AssertDwaRecorderHasMetrics() const {
    ASSERT_TRUE(metrics::dwa::DwaRecorder::Get()->HasEntries());
    ASSERT_TRUE(metrics::dwa::DwaRecorder::Get()->HasPageLoadEvents());
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
    EXPECT_FALSE(metrics::dwa::DwaRecorder::Get()->HasPageLoadEvents());
  }

  void RecordTestDwaEntryMetric() {
    ::dwa::DwaEntryBuilder builder("Kangaroo.Jumped");
    builder.SetContent("https://adtech.com");
    builder.SetMetric("Length", 5);
    builder.Record(dwa::DwaRecorder::Get());
  }

  void RecordTestDwaEntryMetricAndPageLoadEvent() {
    RecordTestDwaEntryMetric();
    metrics::dwa::DwaRecorder::Get()->OnPageLoad();
    RecordTestDwaEntryMetric();
  }

  void RecordTestMetricsAndAssertMetricsRecorded() {
    RecordTestDwaEntryMetricAndPageLoadEvent();
    AssertDwaRecorderHasMetrics();
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
      ASSERT_TRUE(
          harness->EnableSyncForType(syncer::UserSelectableType::kExtensions));
    } else {
      ASSERT_TRUE(
          harness->DisableSyncForType(syncer::UserSelectableType::kExtensions));
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
          harness->EnableSyncForType(syncer::UserSelectableType::kApps));
    } else {
      ASSERT_TRUE(
          harness->DisableSyncForType(syncer::UserSelectableType::kApps));
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

// TODO(crbug.com/391901366): Add #else preprocessor directive to support
// Android browser tests.
#if !BUILDFLAG(IS_ANDROID)
  // Creates and returns a platform-appropriate browser for |profile|.
  PlatformBrowser CreatePlatformBrowser(Profile* profile) {
    return CreateBrowser(profile);
  }

  // Creates a platform-appropriate incognito browser for |profile|.
  PlatformBrowser CreateIncognitoPlatformBrowser(Profile* profile) {
    EXPECT_TRUE(profile->IsOffTheRecord());
    return CreateIncognitoBrowser(profile);
  }

  // Closes |browser| in a way that is appropriate for the platform.
  void ClosePlatformBrowser(PlatformBrowser& browser) {
    CloseBrowserSynchronously(browser);
  }

#endif  // !BUILDFLAG(IS_ANDROID)

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/391901366): Remove preprocessor directive to enable for
// Android browser tests.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(DwaBrowserTest, DwaServiceCheck) {
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  dwa::DwaService* dwa_service = GetDwaService();
  dwa::DwaRecorder* dwa_recorder = metrics::dwa::DwaRecorder::Get();

  PlatformBrowser browser = CreatePlatformBrowser(profile);
  ASSERT_TRUE(dwa_recorder->IsEnabled());

  // Records a DWA entry metric.
  RecordTestDwaEntryMetric();
  EXPECT_TRUE(dwa_recorder->HasEntries());
  EXPECT_FALSE(dwa_recorder->HasPageLoadEvents());
  EXPECT_FALSE(dwa_service->unsent_log_store()->has_unsent_logs());

  dwa_recorder->OnPageLoad();
  EXPECT_FALSE(dwa_recorder->HasEntries());
  EXPECT_TRUE(dwa_recorder->HasPageLoadEvents());
  EXPECT_FALSE(dwa_service->unsent_log_store()->has_unsent_logs());

  GetDwaService()->Flush(
      metrics::MetricsLogsEventManager::CreateReason::kPeriodic);
  ExpectDwaRecorderIsEmpty();
  EXPECT_TRUE(dwa_service->unsent_log_store()->has_unsent_logs());

  ClosePlatformBrowser(browser);
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Make sure that DWA is disabled and purged while an incognito window is open.
// TODO(crbug.com/391901366): Remove preprocessor directive to enable for
// Android browser tests.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(DwaBrowserTest, RegularBrowserPlusIncognitoCheck) {
  dwa::DwaRecorder* dwa_recorder = metrics::dwa::DwaRecorder::Get();
  MetricsConsentOverride metrics_consent(true);
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
#endif  // !BUILDFLAG(IS_ANDROID)

// Make sure opening a regular browser after Incognito doesn't enable DWA.
// TODO(crbug.com/391901366): Remove preprocessor directive to enable for
// Android browser tests.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(DwaBrowserTest, IncognitoPlusRegularBrowserCheck) {
  dwa::DwaRecorder* dwa_recorder = metrics::dwa::DwaRecorder::Get();
  MetricsConsentOverride metrics_consent(true);
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
#endif  // !BUILDFLAG(IS_ANDROID)

// This test ensures that disabling MSBB UKM consent disables and purges DWA.
// Additionally ensures that DWA is disabled until all UKM consents are enabled.
// TODO(crbug.com/391901366): Remove preprocessor directive to enable for
// Android browser tests.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(DwaBrowserTest, UkmConsentChangeCheck_Msbb) {
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetricAndPageLoadEvent();
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

// This test ensures that disabling Extensions UKM consent disables and purges
// DWA. Additionally ensures that DWA is disabled until all UKM consents are
// enabled.
IN_PROC_BROWSER_TEST_F(DwaBrowserTest, UkmConsentChangeCheck_Extensions) {
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetricAndPageLoadEvent();
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
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetricAndPageLoadEvent();
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
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetricAndPageLoadEvent();
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
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetricAndPageLoadEvent();
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
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetricAndPageLoadEvent();
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
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetricAndPageLoadEvent();
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
