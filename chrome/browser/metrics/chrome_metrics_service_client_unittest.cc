// Copyright 2017 The Chromium Authors. All rights reserved.
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
#include "components/metrics/file_metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/metrics/unsent_log_store.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/ukm_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/login/login_state/login_state.h"
#endif

class ChromeMetricsServiceClientTest : public testing::Test {
 public:
  ChromeMetricsServiceClientTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        enabled_state_provider_(false /* consent */, false /* enabled */) {}

  void SetUp() override {
    testing::Test::SetUp();
    metrics::MetricsService::RegisterPrefs(prefs_.registry());
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &prefs_, &enabled_state_provider_, std::wstring(),
        base::BindRepeating(
            &ChromeMetricsServiceClientTest::FakeStoreClientInfoBackup,
            base::Unretained(this)),
        base::BindRepeating(
            &ChromeMetricsServiceClientTest::LoadFakeClientInfoBackup,
            base::Unretained(this)));
    ASSERT_TRUE(profile_manager_.SetUp());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    scoped_feature_list_.InitAndEnableFeature(features::kUmaStorageDimensions);
    // ChromeOs Metrics Provider require g_login_state and power manager client
    // initialized before they can be instantiated.
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::LoginState::Initialize();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    chromeos::LoginState::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    // ChromeMetricsServiceClient::Initialize() initializes
    // IdentifiabilityStudySettings as part of creating the
    // PrivacyBudgetUkmEntryFilter. Reset them after the test.
    blink::IdentifiabilityStudySettings::ResetStateForTesting();
  }

 protected:
  void FakeStoreClientInfoBackup(const metrics::ClientInfo& client_info) {}

  std::unique_ptr<metrics::ClientInfo> LoadFakeClientInfoBackup() {
    return std::make_unique<metrics::ClientInfo>();
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  TestingProfileManager profile_manager_;
  base::UserActionTester user_action_runner_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  metrics::TestEnabledStateProvider enabled_state_provider_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeMetricsServiceClientTest);
};

namespace {

bool TestIsProcessRunning(base::ProcessId pid) {
  // Odd are running, even are not.
  return (pid & 1) == 1;
}

TEST_F(ChromeMetricsServiceClientTest, FilterFiles) {
  ChromeMetricsServiceClient::SetIsProcessRunningForTesting(
      &TestIsProcessRunning);

  base::ProcessId my_pid = base::GetCurrentProcId();
  base::FilePath active_dir(FILE_PATH_LITERAL("foo"));
  base::FilePath upload_dir(FILE_PATH_LITERAL("bar"));
  base::FilePath upload_path =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          upload_dir, "TestMetrics");
  EXPECT_EQ(metrics::FileMetricsProvider::FILTER_ACTIVE_THIS_PID,
            ChromeMetricsServiceClient::FilterBrowserMetricsFiles(upload_path));

  EXPECT_EQ(
      metrics::FileMetricsProvider::FILTER_PROCESS_FILE,
      ChromeMetricsServiceClient::FilterBrowserMetricsFiles(
          base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
              upload_dir, "Test", base::Time::Now(), (my_pid & ~1) + 10)));
  EXPECT_EQ(
      metrics::FileMetricsProvider::FILTER_TRY_LATER,
      ChromeMetricsServiceClient::FilterBrowserMetricsFiles(
          base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
              upload_dir, "Test", base::Time::Now(), (my_pid & ~1) + 11)));
}

}  // namespace

TEST_F(ChromeMetricsServiceClientTest, TestRegisterUKMProviders) {
  // Test that UKM service has initialized its metrics providers. Currently
  // there are 6 providers for all platform except ChromeOS.
  // NetworkMetricsProvider, GPUMetricsProvider, CPUMetricsProvider
  // ScreenInfoMetricsProvider, FieldTrialsProvider, and
  // PrivacyBudgetMetricsProvider.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const size_t expected_providers = 7;  // ChromeOSMetricsProvider
#else
  const size_t expected_providers = 6;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  std::unique_ptr<ChromeMetricsServiceClient> chrome_metrics_service_client =
      ChromeMetricsServiceClient::Create(metrics_state_manager_.get());
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
  expected_providers += 20;

  int sample_rate;
  if (ChromeMetricsServicesManagerClient::GetSamplingRatePerMille(
          &sample_rate)) {
    // SamplingMetricsProvider.
    expected_providers++;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  expected_providers++;  // ExtensionsMetricsProvider.
#endif                   // defined(ENABLE_EXTENSIONS)

#if defined(OS_ANDROID)
  // AndroidMetricsProvider, ChromeAndroidMetricsProvider, and
  // PageLoadMetricsProvider.
  expected_providers += 3;
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
  // GoogleUpdateMetricsProviderWin and AntiVirusMetricsProvider.
  expected_providers += 2;
#endif  // defined(OS_WIN)

#if BUILDFLAG(ENABLE_PLUGINS)
  // PluginMetricsProvider.
  expected_providers++;
#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // LacrosMetricsProvider.
  expected_providers++;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // AmbientModeMetricsProvider, AssistantServiceMetricsProvider,
  // CrosHealthdMetricsProvider, ChromeOSMetricsProvider,
  // SigninStatusMetricsProviderChromeOS, PrinterMetricsProvider,
  // HashedLoggingMetricsProvider, FamilyUserMetricsProvider,
  // FamilyLinkUserMetricsProvider, and UserTypeByDeviceTypeMetricsProvider.
  expected_providers += 10;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeSigninStatusMetricsProvider (for non ChromeOS).
  // AccessibilityMetricsProvider
  expected_providers += 2;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  expected_providers++;  // UpgradeMetricsProvider
#endif                   //! defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_MAC)
  expected_providers++;  // PowerMetricsProvider
#endif                   // defined(OS_MAC)

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  expected_providers++;  // DesktopPlatformFeaturesMetricsProvider
#endif  // defined(OS_WIN) || defined(OS_MAC) || (defined(OS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_LACROS))

  std::unique_ptr<ChromeMetricsServiceClient> chrome_metrics_service_client =
      ChromeMetricsServiceClient::Create(metrics_state_manager_.get());
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

  EXPECT_FALSE(ChromeMetricsServiceClient::IsWebstoreExtension("foo"));
  EXPECT_FALSE(
      ChromeMetricsServiceClient::IsWebstoreExtension(test_extension_id1));
  EXPECT_TRUE(
      ChromeMetricsServiceClient::IsWebstoreExtension(test_extension_id2));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(ChromeMetricsServiceClientTest, GetUploadSigningKey_NotEmpty) {
  std::unique_ptr<ChromeMetricsServiceClient> chrome_metrics_service_client =
      ChromeMetricsServiceClient::Create(metrics_state_manager_.get());
  const std::string signed_key =
      chrome_metrics_service_client->GetUploadSigningKey();
  // The signed key should never be an empty string for a Chrome-branded build.
  EXPECT_FALSE(signed_key.empty());
}

TEST_F(ChromeMetricsServiceClientTest, GetUploadSigningKey_CanSignLogs) {
  std::unique_ptr<ChromeMetricsServiceClient> chrome_metrics_service_client =
      ChromeMetricsServiceClient::Create(metrics_state_manager_.get());
  const std::string signed_key =
      chrome_metrics_service_client->GetUploadSigningKey();

  std::string signature;
  // The signing key should be able to sign data for a Chrome-branded build.
  EXPECT_TRUE(metrics::UnsentLogStore::ComputeHMACForLog(
      "Test Log Data", signed_key, &signature));
  EXPECT_FALSE(signature.empty());
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
