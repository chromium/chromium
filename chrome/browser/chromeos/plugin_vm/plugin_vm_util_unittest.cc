// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

class PluginVmUtilTest : public testing::Test {
 public:
  PluginVmUtilTest() = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<PluginVmTestHelper> test_helper_;

  void SetUp() override {
    testing_profile_ = std::make_unique<TestingProfile>();
    test_helper_ = std::make_unique<PluginVmTestHelper>(testing_profile_.get());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginVmUtilTest);
};

TEST_F(PluginVmUtilTest, PluginVmShouldBeAllowedOnceAllConditionsAreMet) {
  EXPECT_FALSE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  test_helper_->AllowPluginVm();
  EXPECT_TRUE(IsPluginVmAllowedForProfile(testing_profile_.get()));
}

TEST_F(PluginVmUtilTest, PluginVmShouldNotBeAllowedUnlessAllConditionsAreMet) {
  EXPECT_FALSE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  test_helper_->SetUserRequirementsToAllowPluginVm();
  EXPECT_FALSE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  test_helper_->EnablePluginVmFeature();
  EXPECT_FALSE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  test_helper_->EnterpriseEnrollDevice();
  EXPECT_FALSE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  test_helper_->SetPolicyRequirementsToAllowPluginVm();
  EXPECT_TRUE(IsPluginVmAllowedForProfile(testing_profile_.get()));
}

TEST_F(PluginVmUtilTest, PluginVmShouldBeConfiguredOnceAllConditionsAreMet) {
  EXPECT_FALSE(IsPluginVmConfigured(testing_profile_.get()));

  testing_profile_->GetPrefs()->SetBoolean(
      plugin_vm::prefs::kPluginVmImageExists, true);
  EXPECT_TRUE(IsPluginVmConfigured(testing_profile_.get()));
}

TEST_F(PluginVmUtilTest, GetPluginVmLicenseKey) {
  // If no license key is set, the method should return the empty string.
  EXPECT_EQ(std::string(), GetPluginVmLicenseKey());

  const std::string kLicenseKey = "LICENSE_KEY";
  testing_profile_->ScopedCrosSettingsTestHelper()->SetString(
      chromeos::kPluginVmLicenseKey, kLicenseKey);
  EXPECT_EQ(kLicenseKey, GetPluginVmLicenseKey());
}

TEST_F(PluginVmUtilTest, PluginVmShouldBeAllowedForManualTesting) {
  EXPECT_FALSE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  test_helper_->AllowPluginVmForManualTesting();
  EXPECT_TRUE(IsPluginVmAllowedForProfile(testing_profile_.get()));
}

}  // namespace plugin_vm
