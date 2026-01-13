// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/google_services_connectivity_routine.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

}  // namespace

class GoogleServicesConnectivityRoutineTest : public testing::Test {
 public:
  GoogleServicesConnectivityRoutineTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kGoogleServicesConnectivityRoutine);
  }
  GoogleServicesConnectivityRoutineTest(
      const GoogleServicesConnectivityRoutineTest&) = delete;
  GoogleServicesConnectivityRoutineTest& operator=(
      const GoogleServicesConnectivityRoutineTest&) = delete;
  ~GoogleServicesConnectivityRoutineTest() override = default;

  void SetUp() override {
    google_services_connectivity_routine_ =
        std::make_unique<GoogleServicesConnectivityRoutine>(
            mojom::RoutineCallSource::kDiagnosticsUI);
  }

  void TearDown() override { google_services_connectivity_routine_.reset(); }

  GoogleServicesConnectivityRoutine* google_services_connectivity_routine() {
    return google_services_connectivity_routine_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<GoogleServicesConnectivityRoutine>
      google_services_connectivity_routine_;
};

TEST_F(GoogleServicesConnectivityRoutineTest, Type) {
  EXPECT_EQ(google_services_connectivity_routine()->Type(),
            mojom::RoutineType::kGoogleServicesConnectivity);
}

TEST_F(GoogleServicesConnectivityRoutineTest, NoProblem) {
  base::test::TestFuture<mojom::RoutineResultPtr> future;
  google_services_connectivity_routine()->RunRoutine(future.GetCallback());
  const auto& result = future.Get();

  EXPECT_EQ(mojom::RoutineVerdict::kNoProblem, result->verdict);
  EXPECT_TRUE(
      result->problems->get_google_services_connectivity_problems().empty());
}

// Test fixture with the feature flag disabled.
class GoogleServicesConnectivityRoutineDisabledTest : public testing::Test {
 public:
  GoogleServicesConnectivityRoutineDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        ash::features::kGoogleServicesConnectivityRoutine);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(GoogleServicesConnectivityRoutineDisabledTest,
       CanNotRunFeatureDisabled) {
  GoogleServicesConnectivityRoutine routine(
      mojom::RoutineCallSource::kDiagnosticsUI);
  ASSERT_FALSE(routine.CanRun());

  base::test::TestFuture<mojom::RoutineResultPtr> future;
  routine.RunRoutine(future.GetCallback());
  const auto& result = future.Get();
  EXPECT_EQ(result->verdict, mojom::RoutineVerdict::kNotRun);
  EXPECT_FALSE(result->timestamp.is_null());
}

}  // namespace ash::network_diagnostics
