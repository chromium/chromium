// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_ui_availability_reporter.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

using Mode = ArcUiAvailabilityReporter::Mode;

const base::HistogramBase* GetHistogram(Mode mode) {
  return base::StatisticsRecorder::FindHistogram(
      "Arc.UiAvailable." +
      ArcUiAvailabilityReporter::GetHistogramNameForMode(mode) +
      ".TimeDelta.Unmanaged");
}

int64_t ReadSingleStatistics(Mode mode) {
  const base::HistogramBase* histogram = GetHistogram(mode);
  DCHECK(histogram);

  std::unique_ptr<base::HistogramSamples> samples =
      histogram->SnapshotFinalDelta();
  DCHECK(samples.get());
  // Expected reported only once.
  DCHECK_EQ(1, samples->TotalCount());
  return samples->sum();
}

}  // namespace

class ArcUiAvailabilityReporterTest : public testing::Test {
 public:
  ArcUiAvailabilityReporterTest() {}
  ~ArcUiAvailabilityReporterTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    arc::SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    // arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();
    profile_ = TestingProfile::Builder().Build();

    // Use ArcAppTest to initialize infrastructure.
    arc_app_test_.set_activate_arc_on_start(false);
    arc_app_test_.SetUp(profile());
    app_instance_ = std::make_unique<FakeAppInstance>(
        arc_app_test_.arc_app_list_prefs() /* app_host */);
    intent_helper_instance_ = std::make_unique<FakeIntentHelperInstance>();
    arc_intent_helper_bridge_ = std::make_unique<ArcIntentHelperBridge>(
        profile(), arc_bridge_service());
  }

  void TearDown() override {
    arc_intent_helper_bridge_.reset();
    intent_helper_instance_.reset();
    app_instance_.reset();
    arc_app_test_.TearDown();
    profile_.reset();
    // arc_service_manager_.reset();
    testing::Test::TearDown();
  }

  TestingProfile* profile() { return profile_.get(); }

  ArcBridgeService* arc_bridge_service() {
    return ArcServiceManager::Get()->arc_bridge_service();
  }

  FakeAppInstance* app_instance() { return app_instance_.get(); }

  FakeIntentHelperInstance* intent_helper_instance() {
    return intent_helper_instance_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  // std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<TestingProfile> profile_;
  ArcAppTest arc_app_test_;
  std::unique_ptr<ArcIntentHelperBridge> arc_intent_helper_bridge_;
  std::unique_ptr<FakeAppInstance> app_instance_;
  std::unique_ptr<FakeIntentHelperInstance> intent_helper_instance_;

  DISALLOW_COPY_AND_ASSIGN(ArcUiAvailabilityReporterTest);
};

// Reporting is triggered in case both instances are connected.
TEST_F(ArcUiAvailabilityReporterTest, Basic) {
  ArcUiAvailabilityReporter reporter(profile(), Mode::kOobeProvisioning);
  // No reporting at this time.
  EXPECT_FALSE(GetHistogram(Mode::kOobeProvisioning));
  arc_bridge_service()->app()->SetInstance(app_instance());
  EXPECT_FALSE(GetHistogram(Mode::kOobeProvisioning));
  arc_bridge_service()->intent_helper()->SetInstance(intent_helper_instance());
  EXPECT_GE(ReadSingleStatistics(Mode::kOobeProvisioning), 0);
  EXPECT_FALSE(GetHistogram(Mode::kInSessionProvisioning));
  EXPECT_FALSE(GetHistogram(Mode::kAlreadyProvisioned));
}

// Reporting is triggered only once, even if instances are reconnected.
TEST_F(ArcUiAvailabilityReporterTest, TriggeredOnce) {
  ArcUiAvailabilityReporter reporter(profile(), Mode::kAlreadyProvisioned);
  arc_bridge_service()->app()->SetInstance(app_instance());
  arc_bridge_service()->intent_helper()->SetInstance(intent_helper_instance());
  arc_bridge_service()->app()->CloseInstance(app_instance());
  arc_bridge_service()->intent_helper()->CloseInstance(
      intent_helper_instance());
  arc_bridge_service()->app()->SetInstance(app_instance());
  arc_bridge_service()->intent_helper()->SetInstance(intent_helper_instance());
  EXPECT_GE(ReadSingleStatistics(Mode::kAlreadyProvisioned), 0);
}

// Reporting is triggered even if instance restart happened.
TEST_F(ArcUiAvailabilityReporterTest, InstanceRestartAllowed) {
  ArcUiAvailabilityReporter reporter(profile(), Mode::kInSessionProvisioning);
  arc_bridge_service()->app()->SetInstance(app_instance());
  arc_bridge_service()->app()->CloseInstance(app_instance());
  arc_bridge_service()->intent_helper()->SetInstance(intent_helper_instance());
  arc_bridge_service()->app()->SetInstance(app_instance());
  EXPECT_GE(ReadSingleStatistics(Mode::kInSessionProvisioning), 0);
}

}  // namespace arc
