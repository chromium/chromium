// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_mode_dimensions.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/metrics/field_trial.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_test_utils.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/language/core/browser/pref_names.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
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

// Helper function to get the expected milestone string from version_info.
std::string GetExpectedMilestone() {
  std::string chrome_version = std::string(version_info::GetVersionNumber());
  std::vector<std::string> chrome_version_parts = base::SplitString(
      chrome_version, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::string chrome_milestone = chrome_version_parts[0];
  return chrome_milestone;
}

TEST_F(DemoModeDimensionsTest, GetChromeOSVersionString_FullInfo) {
  const char* kLsbRelease =
      "CHROMEOS_RELEASE_VERSION=15183.69.0\n"
      "CHROMEOS_RELEASE_TRACK=stable-channel\n";
  base::test::ScopedChromeOSVersionInfo version(kLsbRelease, base::Time());

  std::string version_string = demo_mode::GetChromeOSVersionString();

  std::string expected_milestone = GetExpectedMilestone();
  std::string expected_version_string = base::StringPrintf(
      "R%s-15183.69.0_stable-channel", expected_milestone.c_str());
  EXPECT_EQ(version_string, expected_version_string);
}

TEST_F(DemoModeDimensionsTest, GetChromeOSVersionString_MissingVersion) {
  const char* kLsbRelease = "CHROMEOS_RELEASE_TRACK=beta-channel\n";
  base::test::ScopedChromeOSVersionInfo version(kLsbRelease, base::Time());

  std::string version_string = demo_mode::GetChromeOSVersionString();

  std::string expected_milestone = GetExpectedMilestone();
  // Platform version should fall back to "0.0.0"
  std::string expected_version_string =
      base::StringPrintf("R%s-0.0.0_beta-channel", expected_milestone.c_str());
  EXPECT_EQ(version_string, expected_version_string);
}

TEST_F(DemoModeDimensionsTest, GetChromeOSVersionString_MissingTrack) {
  const char* kLsbRelease = "CHROMEOS_RELEASE_VERSION=12345.0.0\n";
  base::test::ScopedChromeOSVersionInfo version(kLsbRelease, base::Time());

  std::string version_string = demo_mode::GetChromeOSVersionString();

  std::string expected_milestone = GetExpectedMilestone();
  // Channel should fall back to "unknown-channel"
  std::string expected_version_string = base::StringPrintf(
      "R%s-12345.0.0_unknown-channel", expected_milestone.c_str());
  EXPECT_EQ(version_string, expected_version_string);
}

TEST_F(DemoModeDimensionsTest, GetChromeOSVersionString_NoLsbInfo) {
  const char* kLsbRelease = "";  // Empty LSB info
  base::test::ScopedChromeOSVersionInfo version(kLsbRelease, base::Time());

  std::string version_string = demo_mode::GetChromeOSVersionString();

  std::string expected_milestone = GetExpectedMilestone();
  // Platform version -> "0.0.0", Channel -> "unknown-channel"
  std::string expected_version_string = base::StringPrintf(
      "R%s-0.0.0_unknown-channel", expected_milestone.c_str());
  EXPECT_EQ(version_string, expected_version_string);
}

TEST_F(DemoModeDimensionsTest, Board) {
  const char* kLsbRelease = "CHROMEOS_RELEASE_BOARD=testboard\n";
  base::test::ScopedChromeOSVersionInfo version(kLsbRelease, base::Time());
  // Verify that Board() returns the value we set.
  EXPECT_EQ(ash::demo_mode::Board(), "testboard");
}

TEST_F(DemoModeDimensionsTest, Model) {
  // Initialize ScopedFakeStatisticsProvider on the stack.
  // This automatically sets the global StatisticsProvider instance to the fake.
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider;
  const std::string test_model = "testmodel";
  // Set the machine statistic that Model() reads.
  fake_statistics_provider.SetMachineStatistic(ash::system::kCustomizationIdKey,
                                               test_model);
  // Verify that Model() returns the value we set.
  EXPECT_EQ(ash::demo_mode::Model(), test_model);
}

TEST_F(DemoModeDimensionsTest, Locale) {
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      language::prefs::kApplicationLocale, "fr-CA");
  ASSERT_EQ(ash::demo_mode::Locale(), "fr-CA");
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
