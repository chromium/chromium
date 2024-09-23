// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_client.h"

#include <string>

#include "base/files/file_path.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/process/process_handle.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/metrics/client_info.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/metrics/file_metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/metrics/unsent_log_store.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/ukm_service.h"
#include "components/variations/synthetic_trial_registry.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/dbus/power/power_manager_client.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_test_helper.h"
#endif

class TestChromeMetricsServiceClient : public ChromeMetricsServiceClient {
 public:
  // Equivalent to ChromeMetricsServiceClient::Create
  static std::unique_ptr<TestChromeMetricsServiceClient> Create(
      metrics::MetricsStateManager* metrics_state_manager,
      variations::SyntheticTrialRegistry* synthetic_trial_registry) {
    // Needed because RegisterMetricsServiceProviders() checks for this.
    metrics::SubprocessMetricsProvider::CreateInstance();

    std::unique_ptr<TestChromeMetricsServiceClient> client(
        new TestChromeMetricsServiceClient(metrics_state_manager,
                                           synthetic_trial_registry));
    client->Initialize();

    return client;
  }

 private:
  explicit TestChromeMetricsServiceClient(
      metrics::MetricsStateManager* state_manager,
      variations::SyntheticTrialRegistry* synthetic_trial_registry)
      : ChromeMetricsServiceClient(state_manager, synthetic_trial_registry) {}

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void AsyncInitSystemProfileProvider() override {}
#endif
};

class ChromeMetricsServiceClientTest : public testing::Test {
 public:
  ChromeMetricsServiceClientTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        enabled_state_provider_(false /* consent */, false /* enabled */) {}

  ChromeMetricsServiceClientTest(const ChromeMetricsServiceClientTest&) =
      delete;
  ChromeMetricsServiceClientTest& operator=(
      const ChromeMetricsServiceClientTest&) = delete;

  void SetUp() override {
    testing::Test::SetUp();
    metrics::MetricsService::RegisterPrefs(prefs_.registry());
    synthetic_trial_registry_ =
        std::make_unique<variations::SyntheticTrialRegistry>();
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &prefs_, &enabled_state_provider_, std::wstring(), base::FilePath());
    metrics_state_manager_->InstantiateFieldTrialList();
    ASSERT_TRUE(profile_manager_.SetUp());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    scoped_feature_list_.InitAndEnableFeature(features::kUmaStorageDimensions);
    // ChromeOs Metrics Provider require g_login_state and power manager client
    // initialized before they can be instantiated.
    chromeos::PowerManagerClient::InitializeFake();
    ash::LoginState::Initialize();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::LoginState::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    // ChromeMetricsServiceClient::Initialize() initializes
    // IdentifiabilityStudySettings as part of creating the
    // PrivacyBudgetUkmEntryFilter. Reset them after the test.
    blink::IdentifiabilityStudySettings::ResetStateForTesting();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  TestingProfileManager profile_manager_;
  base::UserActionTester user_action_runner_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_;
  metrics::TestEnabledStateProvider enabled_state_provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::ScopedLacrosServiceTestHelper lacros_test_helper_;
#endif
};

namespace {

bool TestIsProcessRunning(base::ProcessId pid) {
  // Odd are running, even are not.
  return (pid & 1) == 1;
}

TEST_F(ChromeMetricsServiceClientTest, FilterFiles) {
  TestChromeMetricsServiceClient::SetIsProcessRunningForTesting(
      &TestIsProcessRunning);

  base::ProcessId my_pid = base::GetCurrentProcId();
  base::FilePath active_dir(FILE_PATH_LITERAL("foo"));
  base::FilePath upload_dir(FILE_PATH_LITERAL("bar"));
  base::FilePath upload_path =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          upload_dir, "TestMetrics");
  EXPECT_EQ(
      metrics::FileMetricsProvider::FILTER_ACTIVE_THIS_PID,
      TestChromeMetricsServiceClient::FilterBrowserMetricsFiles(upload_path));

  EXPECT_EQ(
      metrics::FileMetricsProvider::FILTER_PROCESS_FILE,
      TestChromeMetricsServiceClient::FilterBrowserMetricsFiles(
          base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
              upload_dir, "Test", base::Time::Now(), (my_pid & ~1) + 10)));
  EXPECT_EQ(
      metrics::FileMetricsProvider::FILTER_TRY_LATER,
      TestChromeMetricsServiceClient::FilterBrowserMetricsFiles(
          base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
              upload_dir, "Test", base::Time::Now(), (my_pid & ~1) + 11)));
}

}  // namespace

TEST_F(ChromeMetricsServiceClientTest, TestRegisterUKMProviders) {
  // Test that UKM service has initialized its metrics providers. Currently
  // there are 9 providers for all platform except ChromeOS.
  // NetworkMetricsProvider, InstallDateProvider, GPUMetricsProvider,
  // CPUMetricsProvider ScreenInfoMetricsProvider, FormFactorMetricsProvider,
  // FieldTrialsProvider, PrivacyBudgetMetricsProvider, and
  // ComponentMetricsProvider.
  size_t expected_providers = 9;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOSMetricsProvider
  expected_providers++;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // LacrosMetricsProvider
  expected_providers++;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  std::unique_ptr<ChromeMetricsServiceClient> chrome_metrics_service_client =
      TestChromeMetricsServiceClient::Create(metrics_state_manager_.get(),
                                             synthetic_trial_registry_.get());
  size_t observed_count = chrome_metrics_service_client->GetUkmService()
                              ->metrics_providers_.GetProviders()
                              .size();
  if (base::FeatureList::IsEnabled(ukm::kUkmFeature)) {
    EXPECT_EQ(expected_providers, observed_count);
  } else {
    EXPECT_EQ(0ul, observed_count);
  }
}

TEST_F(ChromeMetricsServiceClientTest, TestRegisterMetricsServiceProviders) {
  // This is for the two metrics providers added in the MetricsService
  // constructor: StabilityMetricsProvider and MetricsStateMetricsProvider.
  size_t expected_providers = 2;

  // This is the number of metrics providers that are outside any #if macros.
  expected_providers += 22;

  int sample_rate;
  if (ChromeMetricsServicesManagerClient::GetSamplingRatePerMille(
          &sample_rate)) {
    // SamplingMetricsProvider.
    expected_providers++;
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  // MotherboardMetricProvider.
  expected_providers++;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  expected_providers++;  // ExtensionsMetricsProvider.
#endif                   // defined(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_ANDROID)
  // AndroidMetricsProvider, ChromeAndroidMetricsProvider,
  // PageLoadMetricsProvider, GmsMetricsProvider.
  expected_providers += 4;
#else
  // performance_manager::MetricsProvider
  expected_providers += 1;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
  // GoogleUpdateMetricsProviderWin, AntiVirusMetricsProvider, and
  // TPMMetricsProvider.
  expected_providers += 3;
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // LacrosMetricsProvider.
  expected_providers++;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // AmbientModeMetricsProvider, AssistantServiceMetricsProvider,
  // CrosHealthdMetricsProvider, ChromeOSMetricsProvider,
  // ChromeOSHistogramMetricsProvider, ChromeShelfMetricsProvider,
  // KeyboardBacklightColorMetricsProvider,
  // PersonalizationAppThemeMetricsProvider, PrinterMetricsProvider,
  // FamilyUserMetricsProvider, FamilyLinkUserMetricsProvider,
  // UpdateEngineMetricsProvider, OsSettingsMetricsProvider,
  // UserTypeByDeviceTypeMetricsProvider, WallpaperMetricsProvider,
  // and VmmMetricsProvider.
  expected_providers += 16;

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeSigninStatusMetricsProvider (for non ChromeOS).
  // AccessibilityMetricsProvider, FamilyLinkUserMetricsProvider
  expected_providers += 3;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
  expected_providers++;  // PowerMetricsProvider
#endif                   // BUILDFLAG(IS_MAC)

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  expected_providers++;  // DesktopPlatformFeaturesMetricsProvider
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_LACROS))

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // DesktopSessionMetricsProvider
  expected_providers += 1;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // BluetoothMetricsProvider
  expected_providers += 1;
#endif

  std::unique_ptr<TestChromeMetricsServiceClient>
      chrome_metrics_service_client = TestChromeMetricsServiceClient::Create(
          metrics_state_manager_.get(), synthetic_trial_registry_.get());
  EXPECT_EQ(expected_providers,
            chrome_metrics_service_client->GetMetricsService()
                ->delegating_provider_.GetProviders()
                .size());
}

// This can't be a MAYBE test because it won't compile without the extensions
// header files but those can't even be included if this build flag is not
// set. This can't be in the anonymous namespace because it is a "friend" of
// the ChromeMetricsServiceClient class.
#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(ChromeMetricsServiceClientTest, IsWebstoreExtension) {
  static const char test_extension_id1[] = "abcdefghijklmnopqrstuvwxyzabcdef";
  static const char test_extension_id2[] = "bhcnanendmgjjeghamaccjnochlnhcgj";

  TestingProfile* test_profile = profile_manager_.CreateTestingProfile("p1");
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(test_profile);
  ASSERT_TRUE(registry);

  scoped_refptr<const extensions::Extension> extension1 =
      extensions::ExtensionBuilder("e1").SetID(test_extension_id1).Build();
  registry->AddEnabled(extension1);

  scoped_refptr<const extensions::Extension> extension2 =
      extensions::ExtensionBuilder("e2")
          .SetID(test_extension_id2)
          .AddFlags(extensions::Extension::FROM_WEBSTORE)
          .Build();
  registry->AddEnabled(extension2);

  EXPECT_FALSE(TestChromeMetricsServiceClient::IsWebstoreExtension("foo"));
  EXPECT_FALSE(
      TestChromeMetricsServiceClient::IsWebstoreExtension(test_extension_id1));
  EXPECT_TRUE(
      TestChromeMetricsServiceClient::IsWebstoreExtension(test_extension_id2));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

TEST_F(ChromeMetricsServiceClientTest, GetUploadSigningKey_NotEmpty) {
  std::unique_ptr<TestChromeMetricsServiceClient>
      chrome_metrics_service_client = TestChromeMetricsServiceClient::Create(
          metrics_state_manager_.get(), synthetic_trial_registry_.get());
  [[maybe_unused]] const std::string signing_key =
      chrome_metrics_service_client->GetUploadSigningKey();
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // The signing key should never be an empty string for a Chrome-branded build.
  EXPECT_FALSE(signing_key.empty());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

TEST_F(ChromeMetricsServiceClientTest, GetUploadSigningKey_CanSignLogs) {
  std::unique_ptr<TestChromeMetricsServiceClient>
      chrome_metrics_service_client = TestChromeMetricsServiceClient::Create(
          metrics_state_manager_.get(), synthetic_trial_registry_.get());
  const std::string signing_key =
      chrome_metrics_service_client->GetUploadSigningKey();

  std::string signature;
  bool sign_success = metrics::UnsentLogStore::ComputeHMACForLog(
      "Test Log Data", signing_key, &signature);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // The signing key should be able to sign data for a Chrome-branded build.
  EXPECT_TRUE(sign_success);
  EXPECT_FALSE(signature.empty());
#else
  // In non-branded builds, we may still have a valid signing key if
  // USE_OFFICIAL_GOOGLE_API_KEYS is true. However, that macro is not available
  // in this file, so just check that success == a non-empty signature.
  EXPECT_EQ(sign_success, !signature.empty());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}
