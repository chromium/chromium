// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_mode_dimensions.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_test_utils.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class DemoModeDimensionsTest : public testing::Test {
 public:
  DemoModeDimensionsTest()
      : scoped_install_attributes_(StubInstallAttributes::CreateDemoMode()) {}

  ~DemoModeDimensionsTest() override = default;

 protected:
  ScopedStubInstallAttributes scoped_install_attributes_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(DemoModeDimensionsTest, Country) {
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      prefs::kDemoModeCountry, "DE");
  ASSERT_EQ(ash::demo_mode::Country(), "DE");
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      prefs::kDemoModeCountry, "ca");
  ASSERT_EQ(ash::demo_mode::Country(), "CA");
}

TEST_F(DemoModeDimensionsTest, RetailerName) {
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      prefs::kDemoModeRetailerId, "retailer");
  ASSERT_EQ(ash::demo_mode::RetailerName(), "retailer");
}

TEST_F(DemoModeDimensionsTest, StoreNumber) {
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      prefs::kDemoModeStoreId, "1234");
  ASSERT_EQ(ash::demo_mode::StoreNumber(), "1234");
}

TEST_F(DemoModeDimensionsTest, IsCloudGamingDevice) {
  ASSERT_FALSE(ash::demo_mode::IsCloudGamingDevice());
  feature_list_.InitAndEnableFeature(chromeos::features::kCloudGamingDevice);
  ASSERT_TRUE(ash::demo_mode::IsCloudGamingDevice());
}

TEST_F(DemoModeDimensionsTest, IsFeatureAwareDevice) {
  ASSERT_FALSE(ash::demo_mode::IsFeatureAwareDevice());
  feature_list_.InitAndEnableFeature(
      ash::features::kFeatureManagementFeatureAwareDeviceDemoMode);
  ASSERT_TRUE(ash::demo_mode::IsFeatureAwareDevice());
}

TEST_F(DemoModeDimensionsTest, GetDemoModeDimensions) {
  feature_list_.InitWithFeatures(
      {chromeos::features::kCloudGamingDevice,
       ash::features::kFeatureManagementFeatureAwareDeviceDemoMode},
      {});
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      prefs::kDemoModeCountry, "CA");
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      prefs::kDemoModeRetailerId, "retailer");
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      prefs::kDemoModeStoreId, "1234");

  enterprise_management::DemoModeDimensions expected;
  expected.set_country("CA");
  expected.set_retailer_name("retailer");
  expected.set_store_number("1234");
  expected.add_customization_facets(
      enterprise_management::DemoModeDimensions::CLOUD_GAMING_DEVICE);
  expected.add_customization_facets(
      enterprise_management::DemoModeDimensions::FEATURE_AWARE_DEVICE);

  enterprise_management::DemoModeDimensions actual =
      ash::demo_mode::GetDemoModeDimensions();
  ash::test::AssertDemoDimensionsEqual(actual, expected);
}

}  // namespace ash
