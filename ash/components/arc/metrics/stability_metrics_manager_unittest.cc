// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/metrics/stability_metrics_manager.h"

#include "ash/components/arc/arc_prefs.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class StabilityMetricsManagerTest : public testing::Test {
 public:
  StabilityMetricsManagerTest(const StabilityMetricsManagerTest&) = delete;
  StabilityMetricsManagerTest& operator=(const StabilityMetricsManagerTest&) =
      delete;

 protected:
  StabilityMetricsManagerTest() {
    prefs::RegisterLocalStatePrefs(local_state.registry());
    StabilityMetricsManager::Initialize(&local_state);
  }

  ~StabilityMetricsManagerTest() override {
    StabilityMetricsManager::Shutdown();
  }

  static StabilityMetricsManager* manager() {
    return StabilityMetricsManager::Get();
  }

 private:
  TestingPrefServiceSimple local_state;
};

TEST_F(StabilityMetricsManagerTest, GetArcEnabledState) {
  EXPECT_FALSE(manager()->GetArcEnabledState().has_value());

  manager()->SetArcEnabledState(true);
  EXPECT_EQ(manager()->GetArcEnabledState(), true);

  manager()->SetArcEnabledState(false);
  EXPECT_EQ(manager()->GetArcEnabledState(), false);

  manager()->ResetMetrics();
  EXPECT_FALSE(manager()->GetArcEnabledState().has_value());
}

TEST_F(StabilityMetricsManagerTest, GetArcNativeBridgeType) {
  EXPECT_FALSE(manager()->GetArcNativeBridgeType().has_value());

  for (NativeBridgeType type :
       {NativeBridgeType::UNKNOWN, NativeBridgeType::NONE,
        NativeBridgeType::HOUDINI, NativeBridgeType::NDK_TRANSLATION}) {
    manager()->SetArcNativeBridgeType(type);
    EXPECT_EQ(manager()->GetArcNativeBridgeType(), type);
  }

  manager()->ResetMetrics();
  EXPECT_FALSE(manager()->GetArcNativeBridgeType().has_value());
}

TEST_F(StabilityMetricsManagerTest, RecordEnabledStateToUMA) {
  base::HistogramTester tester;

  manager()->SetArcEnabledState(true);
  manager()->RecordMetricsToUMA();
  tester.ExpectBucketCount("Arc.State", 1, 1);

  manager()->SetArcEnabledState(false);
  manager()->RecordMetricsToUMA();
  tester.ExpectBucketCount("Arc.State", 0, 1);
}

TEST_F(StabilityMetricsManagerTest, RecordNativeBridgeTypeToUMA) {
  base::HistogramTester tester;

  EXPECT_EQ(NativeBridgeType::kMaxValue, NativeBridgeType::NDK_TRANSLATION);
  for (NativeBridgeType type :
       {NativeBridgeType::UNKNOWN, NativeBridgeType::NONE,
        NativeBridgeType::HOUDINI, NativeBridgeType::NDK_TRANSLATION}) {
    manager()->SetArcNativeBridgeType(type);
    manager()->RecordMetricsToUMA();
    tester.ExpectBucketCount("Arc.NativeBridge", static_cast<int>(type), 1);
  }
}

TEST_F(StabilityMetricsManagerTest, ResetMetrics) {
  base::HistogramTester tester;

  manager()->SetArcEnabledState(true);
  manager()->SetArcNativeBridgeType(NativeBridgeType::NONE);
  manager()->RecordMetricsToUMA();
  tester.ExpectBucketCount("Arc.State", 1, 1);
  tester.ExpectBucketCount("Arc.NativeBridge",
                           static_cast<int>(NativeBridgeType::NONE), 1);

  manager()->ResetMetrics();
  manager()->RecordMetricsToUMA();
  // Bucket counts should remain the same.
  tester.ExpectBucketCount("Arc.State", 1, 1);
  tester.ExpectBucketCount("Arc.NativeBridge",
                           static_cast<int>(NativeBridgeType::NONE), 1);
}

}  // namespace arc
