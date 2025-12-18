// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/metrics/puma_histogram_functions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/testing/metrics_consent_override.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/country_codes/country_codes.h"
#include "components/metrics/private_metrics/private_metrics_features.h"
#include "components/metrics/private_metrics/puma_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/regional_capabilities_country_id.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "third_party/metrics_proto/private_metrics/private_metrics.pb.h"
#include "third_party/metrics_proto/private_metrics/system_profiles/rc_coarse_system_profile.pb.h"
#include "third_party/zlib/google/compression_utils.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#else
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#endif

namespace metrics::private_metrics {

namespace {

constexpr char kTestBooleanHistogram[] =
    "PUMA.PumaServiceTestHistogram.Boolean";
constexpr char kTestLinearHistogram[] = "PUMA.PumaServiceTestHistogram.Linear";
constexpr char kTestEnumHistogram[] = "PUMA.PumaServiceTestHistogram.Enum";

enum class TestEnum {
  kValueA = 0,
  kValueB = 1,
  kMaxValue = kValueB,
};

}  // namespace

#if !BUILDFLAG(IS_ANDROID)
typedef Browser* PlatformBrowser;
#else
typedef std::unique_ptr<TestTabModel> PlatformBrowser;
#endif  // !BUILDFLAG(IS_ANDROID)

PumaService* GetPumaService() {
  return g_browser_process->GetMetricsServicesManager()->GetPumaService();
}

class PumaBrowserTest : public SyncTest {
 public:
  PumaBrowserTest() : SyncTest(SINGLE_CLIENT) {
    scoped_feature_list_.InitWithFeatures(
        {kPrivateMetricsPuma, kPrivateMetricsPumaRc}, {kPrivateMetricsFeature});
  }

  PumaBrowserTest(const PumaBrowserTest&) = delete;
  PumaBrowserTest& operator=(const PumaBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SyncTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kSearchEngineChoiceCountry, "BE");
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

  void ExpectPumaReportingEnabled(const std::string& failed_message = "") {
    EXPECT_TRUE(base::test::RunUntil([]() {
      return GetPumaService()->reporting_service()->reporting_active();
    })) << failed_message;
  }

  // Helper function to get the RcCoarseSystemProfile from the persisted logs.
  std::unique_ptr<::private_metrics::PrivateMetricEndpointPayload>
  GetLastPumaPayload() {
    auto* log_store = GetPumaService()->reporting_service()->unsent_log_store();
    log_store->StageNextLog();

    if (log_store->staged_log().empty()) {
      return nullptr;
    }

    std::string uncompressed_log_data;
    if (!compression::GzipUncompress(log_store->staged_log(),
                                     &uncompressed_log_data)) {
      return nullptr;
    }

    auto payload =
        std::make_unique<::private_metrics::PrivateMetricEndpointPayload>();
    if (!payload->ParseFromString(uncompressed_log_data)) {
      return nullptr;
    }

    return payload;
  }

  void RecordTestPumaMetric() {
    base::PumaHistogramBoolean(base::PumaType::kRc, kTestBooleanHistogram,
                               true);
    base::PumaHistogramExactLinear(base::PumaType::kRc, kTestLinearHistogram,
                                   50, 101);
    base::PumaHistogramEnumeration(base::PumaType::kRc, kTestEnumHistogram,
                                   TestEnum::kValueA);
  }

  void FlushPumaService() {
    GetPumaService()->Flush(
        metrics::MetricsLogsEventManager::CreateReason::kPeriodic);
  }

  void ExpectUnsentLogs() {
    EXPECT_TRUE(GetPumaService()
                    ->reporting_service()
                    ->unsent_log_store()
                    ->has_unsent_logs());
  }

  void ExpectNoUnsentLogs() {
    EXPECT_FALSE(GetPumaService()
                     ->reporting_service()
                     ->unsent_log_store()
                     ->has_unsent_logs());
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// LINT.IfChange(VerifyRcCoarseSystemProfile)
IN_PROC_BROWSER_TEST_F(PumaBrowserTest, VerifyRcCoarseSystemProfile) {
  test::MetricsConsentOverride metrics_consent(true);

  RecordTestPumaMetric();
  FlushPumaService();
  ExpectUnsentLogs();

  std::unique_ptr<::private_metrics::PrivateMetricEndpointPayload> payload =
      GetLastPumaPayload();
  ASSERT_NE(payload, nullptr);
  ASSERT_TRUE(payload->has_private_uma_report());
  ASSERT_TRUE(payload->private_uma_report().has_rc_profile());

  const auto& rc_profile = payload->private_uma_report().rc_profile();

  // Verify platform.
#if BUILDFLAG(IS_LINUX)
  EXPECT_EQ(rc_profile.platform(), ::private_metrics::Platform::PLATFORM_LINUX);
#elif BUILDFLAG(IS_WIN)
  EXPECT_EQ(rc_profile.platform(),
            ::private_metrics::Platform::PLATFORM_WINDOWS);
#elif BUILDFLAG(IS_MAC)
  EXPECT_EQ(rc_profile.platform(), ::private_metrics::Platform::PLATFORM_MACOS);
#elif BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(rc_profile.platform(),
            ::private_metrics::Platform::PLATFORM_ANDROID);
#elif BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(rc_profile.platform(),
            ::private_metrics::Platform::PLATFORM_CHROMEOS);
#else
  EXPECT_EQ(rc_profile.platform(), ::private_metrics::Platform::PLATFORM_OTHER);
#endif

  // Verify milestone.
  EXPECT_EQ(rc_profile.milestone(), version_info::GetMajorVersionNumberAsInt());

  // Verify country.
  EXPECT_EQ(rc_profile.profile_country_id(),
            country_codes::CountryId("BE").Serialize());

  // Verify channel. In a test environment, the channel is UNKNOWN.
  EXPECT_EQ(rc_profile.channel(),
            ::private_metrics::RcCoarseSystemProfile::CHANNEL_UNKNOWN);
}
// LINT.ThenChange(/ios/chrome/browser/metrics/model/puma_egtest.mm:VerifyRcCoarseSystemProfile)

// Make sure PumaService does not crash during a graceful browser shutdown.
// LINT.IfChange(PumaServiceCheck)
IN_PROC_BROWSER_TEST_F(PumaBrowserTest, PumaServiceCheck) {
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  ASSERT_NE(GetPumaService(), nullptr);
  // For Android, EnableReporting to avoid flaky test failures.
#if BUILDFLAG(IS_ANDROID)
  GetPumaService()->EnableReporting();
#endif  // BUILDFLAG(IS_ANDROID)

  PlatformBrowser browser = CreatePlatformBrowser(profile);
  ExpectPumaReportingEnabled("Browser did not enable PUMA.");
  RecordTestPumaMetric();
  histogram_tester_.ExpectBucketCount(kTestBooleanHistogram, true, 1);
  histogram_tester_.ExpectBucketCount(kTestLinearHistogram, 50, 1);
  histogram_tester_.ExpectBucketCount(kTestEnumHistogram,
                                      static_cast<int>(TestEnum::kValueA), 1);
  FlushPumaService();
  ExpectUnsentLogs();
  ClosePlatformBrowser(browser);
}
// LINT.ThenChange(/ios/chrome/browser/metrics/model/puma_egtest.mm:PumaServiceCheck)

// Make sure that PUMA is enabled when an incognito window is open.
// LINT.IfChange(RegularBrowserPlusIncognitoCheck)
IN_PROC_BROWSER_TEST_F(PumaBrowserTest, RegularBrowserPlusIncognitoCheck) {
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  // For Android, EnableReporting to avoid flaky test failures.
#if BUILDFLAG(IS_ANDROID)
  GetPumaService()->EnableReporting();
#endif  // BUILDFLAG(IS_ANDROID)

  // PUMA should be enabled when opening the first regular browser.
  PlatformBrowser browser1 = CreatePlatformBrowser(profile);
  ExpectPumaReportingEnabled("Initial regular browser did not enable PUMA.");

  // Opening an incognito browser should not disable PumaService.
  Profile* incognito_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  PlatformBrowser incognito_browser1 =
      CreateIncognitoPlatformBrowser(incognito_profile);
  ExpectPumaReportingEnabled("Incognito browser did not keep PUMA enabled.");

  // We should be able to record metrics.
  RecordTestPumaMetric();
  histogram_tester_.ExpectBucketCount(kTestBooleanHistogram, true, 1);
  histogram_tester_.ExpectBucketCount(kTestLinearHistogram, 50, 1);
  histogram_tester_.ExpectBucketCount(kTestEnumHistogram,
                                      static_cast<int>(TestEnum::kValueA), 1);
  FlushPumaService();
  ExpectUnsentLogs();

  // Opening another regular browser should not change the state.
  PlatformBrowser browser2 = CreatePlatformBrowser(profile);
  ExpectPumaReportingEnabled(
      "Second regular browser did not keep PUMA enabled.");

  // Opening and closing another Incognito browser should not change the state.
  PlatformBrowser incognito_browser2 =
      CreateIncognitoPlatformBrowser(incognito_profile);
  ClosePlatformBrowser(incognito_browser2);
  ExpectPumaReportingEnabled("Incognito browser close changed PUMA state.");

  ClosePlatformBrowser(browser2);
  ExpectPumaReportingEnabled("Regular browser close changed PUMA state.");

  // Closing the first incognito browser should not change the state.
  ClosePlatformBrowser(incognito_browser1);
  ExpectPumaReportingEnabled("Incognito browser close changed PUMA state.");

  // We should still be able to record metrics.
  GetPumaService()->reporting_service()->unsent_log_store()->Purge();
  ExpectNoUnsentLogs();
  RecordTestPumaMetric();
  histogram_tester_.ExpectBucketCount(kTestBooleanHistogram, true, 2);
  histogram_tester_.ExpectBucketCount(kTestLinearHistogram, 50, 2);
  histogram_tester_.ExpectBucketCount(kTestEnumHistogram,
                                      static_cast<int>(TestEnum::kValueA), 2);
  FlushPumaService();
  ExpectUnsentLogs();

  ClosePlatformBrowser(browser1);
}
// LINT.ThenChange(/ios/chrome/browser/metrics/model/puma_egtest.mm:RegularBrowserPlusIncognitoCheck)

// Make sure opening a regular browser after Incognito PUMA still get enabled.
// LINT.IfChange(IncognitoPlusRegularBrowserCheck)
IN_PROC_BROWSER_TEST_F(PumaBrowserTest, IncognitoPlusRegularBrowserCheck) {
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  // For Android, EnableReporting to avoid flaky test failures.
#if BUILDFLAG(IS_ANDROID)
  GetPumaService()->EnableReporting();
#endif  // BUILDFLAG(IS_ANDROID)

  Profile* incognito_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  PlatformBrowser incognito_browser =
      CreateIncognitoPlatformBrowser(incognito_profile);
  ExpectPumaReportingEnabled("Incognito browser did not enable PUMA.");

  PlatformBrowser browser = CreatePlatformBrowser(profile);
  ExpectPumaReportingEnabled("Regular browser did not keep PUMA enabled.");

  ClosePlatformBrowser(incognito_browser);
  ExpectPumaReportingEnabled("Incognito browser close changed PUMA state.");

  ClosePlatformBrowser(browser);
}
// LINT.ThenChange(/ios/chrome/browser/metrics/model/puma_egtest.mm:IncognitoPlusRegularBrowserCheck)

}  // namespace metrics::private_metrics
