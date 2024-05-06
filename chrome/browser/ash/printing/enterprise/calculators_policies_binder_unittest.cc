// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/enterprise/calculators_policies_binder.h"

#include <array>
#include <string>

#include "base/test/task_environment.h"
#include "chrome/browser/ash/printing/enterprise/bulk_printers_calculator.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr size_t kNumPrinters = 5;

constexpr size_t kAllowlistPrinters = 4;
constexpr std::array<const char*, kAllowlistPrinters> kAllowlistIds = {
    "First", "Second", "Third", "Fifth"};

constexpr std::array<const char*, 3> kBlocklistIds = {"Second", "Third",
                                                      "Fourth"};
// kNumPrinters - kBlocklistIds.size() = kBlocklistPrinters (2)
constexpr size_t kBlocklistPrinters = 2;

constexpr char kBulkPolicyContentsJson[] = R"json(
[
  {
    "guid": "First",
    "display_name": "LexaPrint",
    "description": "Laser on the test shelf",
    "uri": "ipp://192.168.1.5",
    "ppd_resource": {
      "effective_model": "MS610de"
    }
  }, {
    "guid": "Second",
    "display_name": "Color Laser",
    "description": "The printer next to the water cooler.",
    "uri":"ipps://print-server.intranet.example.com:443/ipp/cl2k4",
    "ppd_resource":{
      "effective_model": "ColorLaser2k4"
    }
  }, {
    "guid": "Third",
    "display_name": "YaLP",
    "description": "Fancy Fancy Fancy",
    "uri": "ipp://192.168.1.8",
    "ppd_resource": {
      "effective_model": "MS610de"
    }
  }, {
    "guid": "Fourth",
    "display_name": "Yon",
    "description": "Another printer",
    "uri": "ipp://192.168.1.9",
    "ppd_resource": {
      "effective_model": "Model"
    }
  }, {
    "guid": "Fifth",
    "display_name": "ABCDE",
    "description": "Yep yep yep",
    "uri": "ipp://192.168.1.10",
    "ppd_resource": {
      "effective_model": "Blah blah Blah"
    }
  }
])json";

template <class Container>
base::Value StringsToList(Container container) {
  auto first = container.begin();
  auto last = container.end();
  base::Value::List list;

  while (first != last) {
    list.Append(*first);
    first++;
  }
  return base::Value(std::move(list));
}

class CalculatorsPoliciesBinderTest : public testing::Test {
 protected:
  void SetUp() override {
    CalculatorsPoliciesBinder::RegisterProfilePrefs(prefs_.registry());
  }

  std::unique_ptr<BulkPrintersCalculator> UserCalculator() {
    auto calculator = BulkPrintersCalculator::Create();
    binder_ =
        CalculatorsPoliciesBinder::UserBinder(&prefs_, calculator->AsWeakPtr());

    // Populate data
    calculator->SetData(std::make_unique<std::string>(kBulkPolicyContentsJson));
    return calculator;
  }

  std::unique_ptr<BulkPrintersCalculator> DeviceCalculator() {
    auto calculator = BulkPrintersCalculator::Create();
    binder_ = CalculatorsPoliciesBinder::DeviceBinder(CrosSettings::Get(),
                                                      calculator->AsWeakPtr());

    // Populate data
    calculator->SetData(std::make_unique<std::string>(kBulkPolicyContentsJson));
    return calculator;
  }

  void SetDeviceSetting(const std::string& path, const base::Value& value) {
    testing_settings_.device_settings()->Set(path, value);
  }

  base::test::TaskEnvironment env_;
  ScopedTestingCrosSettings testing_settings_;
  TestingPrefServiceSimple prefs_;

  // Class under test.  Expected to be destroyed first.
  std::unique_ptr<CalculatorsPoliciesBinder> binder_;
};

TEST_F(CalculatorsPoliciesBinderTest, PrefsAllAccess) {
  auto calculator = UserCalculator();

  // Set prefs to complete computation
  prefs_.SetManagedPref(
      prefs::kRecommendedPrintersAccessMode,
      base::Value(BulkPrintersCalculator::AccessMode::ALL_ACCESS));

  env_.RunUntilIdle();
  EXPECT_TRUE(calculator->IsComplete());
  EXPECT_EQ(calculator->GetPrinters().size(), kNumPrinters);
}

TEST_F(CalculatorsPoliciesBinderTest, PrefsAllowlist) {
  auto calculator = UserCalculator();

  // Set prefs to complete computation
  prefs_.SetManagedPref(
      prefs::kRecommendedPrintersAccessMode,
      base::Value(BulkPrintersCalculator::AccessMode::ALLOWLIST_ONLY));
  prefs_.SetManagedPref(prefs::kRecommendedPrintersAllowlist,
                        StringsToList(kAllowlistIds));

  env_.RunUntilIdle();
  EXPECT_TRUE(calculator->IsComplete());
  EXPECT_EQ(calculator->GetPrinters().size(), kAllowlistPrinters);
}

TEST_F(CalculatorsPoliciesBinderTest, PrefsBlocklist) {
  auto calculator = UserCalculator();

  // Set prefs to complete computation
  prefs_.SetManagedPref(
      prefs::kRecommendedPrintersAccessMode,
      base::Value(BulkPrintersCalculator::AccessMode::BLOCKLIST_ONLY));
  prefs_.SetManagedPref(prefs::kRecommendedPrintersBlocklist,
                        StringsToList(kBlocklistIds));

  env_.RunUntilIdle();
  EXPECT_TRUE(calculator->IsComplete());
  EXPECT_EQ(calculator->GetPrinters().size(), kBlocklistPrinters);
}

TEST_F(CalculatorsPoliciesBinderTest, PrefsBeforeBind) {
  // Verify that if preferences are set before we bind to policies, the
  // calculator is still properly populated.
  prefs_.SetManagedPref(
      prefs::kRecommendedPrintersAccessMode,
      base::Value(BulkPrintersCalculator::AccessMode::ALLOWLIST_ONLY));
  prefs_.SetManagedPref(prefs::kRecommendedPrintersAllowlist,
                        StringsToList(kAllowlistIds));

  auto calculator = UserCalculator();

  env_.RunUntilIdle();
  EXPECT_TRUE(calculator->IsComplete());
  EXPECT_EQ(calculator->GetPrinters().size(), kAllowlistPrinters);
}

TEST_F(CalculatorsPoliciesBinderTest, SettingsAllAccess) {
  auto calculator = DeviceCalculator();

  SetDeviceSetting(kDevicePrintersAccessMode,
                   base::Value(BulkPrintersCalculator::AccessMode::ALL_ACCESS));

  env_.RunUntilIdle();
  EXPECT_TRUE(calculator->IsComplete());
  EXPECT_EQ(calculator->GetPrinters().size(), kNumPrinters);
}

TEST_F(CalculatorsPoliciesBinderTest, SettingsAllowlist) {
  auto calculator = DeviceCalculator();

  SetDeviceSetting(
      kDevicePrintersAccessMode,
      base::Value(BulkPrintersCalculator::AccessMode::ALLOWLIST_ONLY));
  SetDeviceSetting(kDevicePrintersAllowlist, StringsToList(kAllowlistIds));

  env_.RunUntilIdle();
  EXPECT_TRUE(calculator->IsComplete());
  EXPECT_EQ(calculator->GetPrinters().size(), kAllowlistPrinters);
}

TEST_F(CalculatorsPoliciesBinderTest, SettingsBlocklist) {
  auto calculator = DeviceCalculator();

  SetDeviceSetting(
      kDevicePrintersAccessMode,
      base::Value(BulkPrintersCalculator::AccessMode::BLOCKLIST_ONLY));
  SetDeviceSetting(kDevicePrintersBlocklist, StringsToList(kBlocklistIds));

  env_.RunUntilIdle();
  EXPECT_TRUE(calculator->IsComplete());
  EXPECT_EQ(calculator->GetPrinters().size(), kBlocklistPrinters);
}

TEST_F(CalculatorsPoliciesBinderTest, SettingsBeforeBind) {
  // Set policy before binding to the calculator.
  SetDeviceSetting(kDevicePrintersAccessMode,
                   base::Value(BulkPrintersCalculator::AccessMode::ALL_ACCESS));

  auto calculator = DeviceCalculator();

  env_.RunUntilIdle();
  EXPECT_TRUE(calculator->IsComplete());
  EXPECT_EQ(calculator->GetPrinters().size(), kNumPrinters);
}

}  // namespace
}  // namespace ash
