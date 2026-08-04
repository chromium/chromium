// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
#endif  // BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)

}  // namespace optimization_guide
