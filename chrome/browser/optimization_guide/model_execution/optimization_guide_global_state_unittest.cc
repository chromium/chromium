// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version_info/channel.h"
#include "build/branding_buildflags.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/override_manifest_asset_manager_delegate.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/test/base/scoped_channel_override.h"
#endif

namespace optimization_guide {

namespace {

#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
class MockOnDemandUpdater : public component_updater::OnDemandUpdater {
 public:
  MOCK_METHOD(void,
              OnDemandUpdate,
              (const std::string&, Priority, component_updater::Callback),
              (override));
};
#endif  // BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)

}  // namespace

class OptimizationGuideGlobalStateTest : public testing::Test {
 public:
  OptimizationGuideGlobalStateTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~OptimizationGuideGlobalStateTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
    auto component_updater = std::make_unique<
        testing::NiceMock<component_updater::MockComponentUpdateService>>();
    ON_CALL(*component_updater, GetOnDemandUpdater())
        .WillByDefault(testing::ReturnRef(mock_on_demand_updater_));
    TestingBrowserProcess::GetGlobal()->SetComponentUpdater(
        std::move(component_updater));
    global_state_ = OptimizationGuideGlobalState::CreateForTesting();
#else
    global_state_ = OptimizationGuideGlobalState::CreateOrGet();
#endif  // BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
  }

  void TearDown() override {
    global_state_.reset();
    task_environment_.RunUntilIdle();
#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
    TestingBrowserProcess::GetGlobal()->SetComponentUpdater(nullptr);
#endif  // BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfileManager profile_manager_;
  scoped_refptr<OptimizationGuideGlobalState> global_state_;
#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
  testing::NiceMock<MockOnDemandUpdater> mock_on_demand_updater_;
#endif  // BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
};

#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
TEST_F(OptimizationGuideGlobalStateTest, FreeDiskSpaceHistogram) {
  base::HistogramTester histogram_tester;
  task_environment_.FastForwardBy(
      optimization_guide::features::GetOnDeviceStartupMetricDelay());
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.OnDeviceModel.FreeDiskSpace", 1);
}

// Verifies that on pre-Stable channels (Canary, Dev, Beta, Unknown) or custom
// unbranded builds, `ShouldEnableOnDeviceAiByDefault` evaluates to false when
// `kOnDeviceAiDefaultEnabled` is disabled (production configuration).
TEST_F(OptimizationGuideGlobalStateTest,
       ShouldEnableOnDeviceAiByDefaultDisabledOnPreStableOrCustomBuild) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kOnDeviceAiDefaultEnabled);
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();

  for (version_info::Channel channel :
       {version_info::Channel::CANARY, version_info::Channel::DEV,
        version_info::Channel::BETA, version_info::Channel::UNKNOWN}) {
    EXPECT_FALSE(ShouldEnableOnDeviceAiByDefault(
        *local_state, channel, /*is_official_branded_build=*/true));
  }

  // Official branded Stable defaults to true.
  EXPECT_TRUE(ShouldEnableOnDeviceAiByDefault(
      *local_state, version_info::Channel::STABLE,
      /*is_official_branded_build=*/true));

  // Unbranded / custom developer builds default to false even on STABLE.
  EXPECT_FALSE(ShouldEnableOnDeviceAiByDefault(
      *local_state, version_info::Channel::STABLE,
      /*is_official_branded_build=*/false));
}

// Verifies that pre-Stable users who already downloaded an on-device model
// (via either the ManifestBroker ledger or the legacy Component Updater pref)
// keep the default setting enabled on upgrade so their active setup is not
// silently broken.
TEST_F(OptimizationGuideGlobalStateTest,
       ShouldEnableOnDeviceAiByDefaultPreservesExistingInstallOnPreStable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kOnDeviceAiDefaultEnabled);
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();

  // Case 1: Legacy `kLastTimeEligibleForOnDeviceModelDownload` pref is set.
  local_state->SetTime(model_execution::prefs::localstate::
                           kLastTimeEligibleForOnDeviceModelDownload,
                       base::Time::Now());
  EXPECT_TRUE(ShouldEnableOnDeviceAiByDefault(
      *local_state, version_info::Channel::CANARY,
      /*is_official_branded_build=*/true));
  local_state->ClearPref(model_execution::prefs::localstate::
                             kLastTimeEligibleForOnDeviceModelDownload);

  // Case 2: ManifestBroker `kManifestAssetLedger` has an active asset entry.
  base::DictValue ledger;
  base::DictValue installed_entry;
  installed_entry.Set("asset_id", "model_A");
  installed_entry.Set("requested_version", "2026.01.01.1");
  ledger.Set("model_A_key", std::move(installed_entry));
  local_state->SetDict(
      model_execution::prefs::localstate::kManifestAssetLedger,
      std::move(ledger));
  EXPECT_TRUE(ShouldEnableOnDeviceAiByDefault(
      *local_state, version_info::Channel::CANARY,
      /*is_official_branded_build=*/true));
}

// Verifies that `ShouldEnableOnDeviceAiByDefault` ignores ledger entries marked
// as `"uninstalling"` or malformed empty dictionaries, ensuring evicted/broken
// entries do not falsely keep the default setting enabled on pre-Stable.
TEST_F(OptimizationGuideGlobalStateTest,
       ShouldEnableOnDeviceAiByDefaultIgnoresUninstallingLedgerEntry) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kOnDeviceAiDefaultEnabled);
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();

  base::DictValue ledger;
  base::DictValue uninstalling_entry;
  uninstalling_entry.Set("asset_id", "model_A");
  uninstalling_entry.Set("requested_version", "uninstalling");
  ledger.Set("model_A_key", std::move(uninstalling_entry));
  ledger.Set("empty_malformed_key", base::DictValue());
  local_state->SetDict(
      model_execution::prefs::localstate::kManifestAssetLedger,
      std::move(ledger));

  EXPECT_FALSE(ShouldEnableOnDeviceAiByDefault(
      *local_state, version_info::Channel::CANARY,
      /*is_official_branded_build=*/true));
}

// Verifies that `kOnDeviceAiDefaultEnabled` forces the default setting to true
// even on unbranded or pre-Stable builds.
TEST_F(OptimizationGuideGlobalStateTest,
       ShouldEnableOnDeviceAiByDefaultRespectsFeatureFlagOverride) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kOnDeviceAiDefaultEnabled);
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();

  EXPECT_TRUE(ShouldEnableOnDeviceAiByDefault(
      *local_state, version_info::Channel::UNKNOWN,
      /*is_official_branded_build=*/false));
}

// Verifies that passing `--optimization-guide-manifest-override` on the command
// line keeps the default setting enabled on pre-Stable and custom developer
// builds so local manifest testing works out of the box.
TEST_F(OptimizationGuideGlobalStateTest,
       ShouldEnableOnDeviceAiByDefaultRespectsManifestOverrideSwitch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kOnDeviceAiDefaultEnabled);
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      kOptimizationGuideManifestOverrideSwitch);
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();

  EXPECT_TRUE(ShouldEnableOnDeviceAiByDefault(
      *local_state, version_info::Channel::UNKNOWN,
      /*is_official_branded_build=*/false));
}

// Verifies that constructing `OptimizationGuideGlobalFeature` updates the
// default value of `kOnDeviceAiUserSettingsEnabled` in `local_state` while
// keeping `IsDefaultValue()` true, and never overrides an explicit user choice.
TEST_F(OptimizationGuideGlobalStateTest,
       GlobalFeatureAppliesDefaultAndRespectsExplicitUserChoice) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Official branded test binaries report Stable on Windows/macOS; pin a
  // pre-Stable channel so the expected default is deterministic.
  chrome::ScopedChannelOverride channel_override(
      chrome::ScopedChannelOverride::Channel::kDev);
#endif
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kOnDeviceAiDefaultEnabled);
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();

  OptimizationGuideGlobalFeature global_feature;
  const PrefService::Preference* pref = local_state->FindPreference(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled);
  ASSERT_NE(pref, nullptr);
  EXPECT_FALSE(local_state->GetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled));
  EXPECT_TRUE(pref->IsDefaultValue());

  // Explicit user choice in the user pref store takes precedence over the
  // default pref store.
  local_state->SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled, true);
  OptimizationGuideGlobalFeature global_feature_after_user_choice;
  EXPECT_TRUE(local_state->GetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled));
  EXPECT_FALSE(pref->IsDefaultValue());
}
#endif  // BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)

}  // namespace optimization_guide
