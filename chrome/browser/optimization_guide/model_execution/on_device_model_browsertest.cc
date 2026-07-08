// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/manifest_builder.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/manifest.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

// A minimal collection of assets that should enable the "test" feature.
class OnDeviceAssets {
 public:
  OnDeviceAssets()
      : manifest_component_dir_(BuildManifest()),
        manifest_override_(ManifestOverrideBuilder()
                               .SetManifestPath(manifest_component_dir_.path())
                               .AddComponentOverride("base_model_public_key",
                                                     "0.0.1",
                                                     base_model_asset_.path())
                               .Build()) {
    manifest_component_dir_.Add("test_config.binarypb", []() {
      proto::SolutionConfig config;
      *config.mutable_feature() = SimpleTestFeatureConfig();
      return config;
    }());
  }
  ~OnDeviceAssets() = default;

  ManifestOverrideFile& manifest_override() { return manifest_override_; }

 private:
  static proto::Manifest BuildManifest() {
    ManifestBuilder builder;
    builder.Add("base_model",
                OnDemandComponent("base_model_public_key", "0.0.1"));
    builder.Add(
        "base_model_recipe",
        BaseModelRecipe(
            FileReference("base_model", "weights.bin"),
            BaseModelRecipeArgs(
                proto::BaseModelRecipe::BACKEND_TYPE_CPU,
                proto::BaseModelRecipe::PERFORMANCE_HINT_UNSPECIFIED, {}, 0)));
    builder.Add(
        "test_solution",
        SolutionRecipe("base_model_recipe", "",
                       FileReference("manifest", "test_config.binarypb")),
        {
            DeviceUseCase{DeviceCategory::kCpu, "test"},
            DeviceUseCase{DeviceCategory::kGpuLowTier, "test"},
            DeviceUseCase{DeviceCategory::kGpuHighTier, "test"},
        });
    return builder.Build();
  }

  FakeBaseModelAsset base_model_asset_;
  ManifestComponentDirectory manifest_component_dir_;
  ManifestOverrideFile manifest_override_;
};

class OnDeviceModelExecutionDisabledBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureStates({
        {features::kOptimizationGuideModelExecution, true},
        {features::kOptimizationGuideOnDeviceModel, false},
        {kOptimizationGuideManifestBroker, true},
    });
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    // This test depends on the disk information being available in a timely
    // manner (see crbug.com/346579988). Use this flag to have the information
    // retrieved with higher priority which reduces the chances of flakiness.
    cmd->AppendSwitch(switches::kGetFreeDiskSpaceWithUserVisiblePriorityTask);
    cmd->AppendSwitchPath("optimization-guide-manifest-override",
                          assets_.manifest_override().path());
  }

  OnDeviceModelEligibilityReason GetOnDeviceModelEligibility(
      mojom::OnDeviceFeature feature) {
    return OptimizationGuideKeyedServiceFactory::GetForProfile(
               browser()->GetProfile())
        ->GetOnDeviceModelEligibility(feature);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  OnDeviceAssets assets_;
};

IN_PROC_BROWSER_TEST_F(OnDeviceModelExecutionDisabledBrowserTest,
                       GetOnDeviceModelEligibilityExecutionDisabled) {
  // With kOptimizationGuideOnDeviceModel disabled, should not be eligible.
  EXPECT_EQ(GetOnDeviceModelEligibility(mojom::OnDeviceFeature::kTest),
            OnDeviceModelEligibilityReason::kFeatureNotEnabled);
}

IN_PROC_BROWSER_TEST_F(OnDeviceModelExecutionDisabledBrowserTest,
                       PerformanceClassNotComputed) {
  auto* service = OptimizationGuideKeyedServiceFactory::GetForProfile(
      browser()->GetProfile());
  base::HistogramTester histogram_tester;
  base::RunLoop loop;
  // The call should exit early because the service is not enabled.
  service->GetOnDeviceModelEligibilityAsync(
      mojom::OnDeviceFeature::kTest,
      /*capabilities=*/{},
      base::IgnoreArgs<OnDeviceModelEligibilityReason>(loop.QuitClosure()));
  loop.Run();
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelPerformanceClass", 0);
}

#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)

class OnDeviceModelExecutionEnabledBrowserTest
    : public OnDeviceModelExecutionDisabledBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureStates({
        {features::kOptimizationGuideModelExecution, true},
        {features::kOptimizationGuideOnDeviceModel, true},
        {kOptimizationGuideManifestBroker, true},
        {features::kLogOnDeviceMetricsOnStartup, false},
    });
    InProcessBrowserTest::SetUp();
  }

  void SetUpLocalStatePrefService(PrefService* local_state) override {
    model_execution::prefs::RecordFeatureUsage(local_state,
                                               mojom::OnDeviceFeature::kTest);
    UpdatePerformanceClassPref(local_state,
                               OnDeviceModelPerformanceClass::kVeryHigh);
  }
};

IN_PROC_BROWSER_TEST_F(OnDeviceModelExecutionEnabledBrowserTest,
                       GetOnDeviceModelEligibility) {
  // With feature enabled, should be eligible once assets installed.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetOnDeviceModelEligibility(mojom::OnDeviceFeature::kTest) ==
           OnDeviceModelEligibilityReason::kSuccess;
  })) << "Timeout waiting for model to be marked eligible.";
}

class LogOnDeviceMetricsOnStartupEnabledBrowserTest
    : public OnDeviceModelExecutionDisabledBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureStates({
        {features::kOptimizationGuideModelExecution, true},
        {features::kOptimizationGuideOnDeviceModel, true},
        {kOptimizationGuideManifestBroker, true},
        {features::kLogOnDeviceMetricsOnStartup, true},
    });
    InProcessBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(LogOnDeviceMetricsOnStartupEnabledBrowserTest,
                       LogOnDeviceMetricsAfterStart) {
  auto* service = OptimizationGuideKeyedServiceFactory::GetForProfile(
      browser()->GetProfile());
  base::HistogramTester histogram_tester;

  base::RunLoop loop;
  // The call should exit early because the service is not enabled.
  service->GetOnDeviceModelEligibilityAsync(
      mojom::OnDeviceFeature::kTest,
      /*capabilities=*/{},
      base::IgnoreArgs<OnDeviceModelEligibilityReason>(loop.QuitClosure()));
  loop.Run();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelPerformanceClass", 1);
}

#endif  // BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)

}  // namespace

}  // namespace optimization_guide
