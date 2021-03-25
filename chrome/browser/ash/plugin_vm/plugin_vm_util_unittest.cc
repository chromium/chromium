// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"

#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

class PluginVmUtilTest : public testing::Test {
 public:
  PluginVmUtilTest() = default;

  MOCK_METHOD(void, OnPolicyChanged, (bool));

 protected:
  struct ScopedDBusThreadManager {
    ScopedDBusThreadManager() { chromeos::DBusThreadManager::Initialize(); }
    ~ScopedDBusThreadManager() { chromeos::DBusThreadManager::Shutdown(); }
  } dbus_thread_manager_;

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
  EXPECT_FALSE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));

  test_helper_->AllowPluginVm();
  EXPECT_TRUE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));
}

TEST_F(PluginVmUtilTest, PluginVmShouldNotBeAllowedUnlessAllConditionsAreMet) {
  EXPECT_FALSE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));

  test_helper_->SetUserRequirementsToAllowPluginVm();
  EXPECT_FALSE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));

  test_helper_->EnablePluginVmFeature();
  EXPECT_FALSE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));

  test_helper_->EnterpriseEnrollDevice();
  EXPECT_FALSE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));

  test_helper_->SetPolicyRequirementsToAllowPluginVm();
  EXPECT_TRUE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));
}

TEST_F(PluginVmUtilTest, PluginVmShouldBeConfiguredOnceAllConditionsAreMet) {
  EXPECT_FALSE(PluginVmFeatures::Get()->IsConfigured(testing_profile_.get()));

  testing_profile_->GetPrefs()->SetBoolean(
      plugin_vm::prefs::kPluginVmImageExists, true);
  EXPECT_TRUE(PluginVmFeatures::Get()->IsConfigured(testing_profile_.get()));
}

TEST_F(PluginVmUtilTest, GetPluginVmLicenseKey) {
  // If no license key is set, the method should return the empty string.
  EXPECT_EQ(std::string(), GetPluginVmLicenseKey());

  const std::string kLicenseKey = "LICENSE_KEY";
  testing_profile_->ScopedCrosSettingsTestHelper()->SetString(
      chromeos::kPluginVmLicenseKey, kLicenseKey);
  EXPECT_EQ(kLicenseKey, GetPluginVmLicenseKey());
}

TEST_F(PluginVmUtilTest, AddPluginVmPolicyObserver) {
  const std::unique_ptr<PluginVmPolicySubscription> subscription =
      std::make_unique<plugin_vm::PluginVmPolicySubscription>(
          testing_profile_.get(),
          base::BindRepeating(&PluginVmUtilTest::OnPolicyChanged,
                              base::Unretained(this)));

  EXPECT_FALSE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));

  EXPECT_CALL(*this, OnPolicyChanged(true));
  test_helper_->AllowPluginVm();
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnPolicyChanged(false));
  testing_profile_->ScopedCrosSettingsTestHelper()->SetString(
      chromeos::kPluginVmLicenseKey, "");
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnPolicyChanged(true));
  const std::string kLicenseKey = "LICENSE_KEY";
  testing_profile_->ScopedCrosSettingsTestHelper()->SetString(
      chromeos::kPluginVmLicenseKey, kLicenseKey);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnPolicyChanged(false));
  testing_profile_->ScopedCrosSettingsTestHelper()->SetBoolean(
      chromeos::kPluginVmAllowed, false);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnPolicyChanged(true));
  testing_profile_->ScopedCrosSettingsTestHelper()->SetBoolean(
      chromeos::kPluginVmAllowed, true);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnPolicyChanged(false));
  testing_profile_->GetPrefs()->SetBoolean(plugin_vm::prefs::kPluginVmAllowed,
                                           false);
}

TEST_F(PluginVmUtilTest, DriveUrlNonMatches) {
  EXPECT_EQ(base::nullopt,
            GetIdFromDriveUrl(GURL(
                "http://192.168.0.2?id=Yxhi5BDTxsEl9onT8AunH4o_tkKviFGjY")));
  EXPECT_EQ(base::nullopt,
            GetIdFromDriveUrl(
                GURL("https://drive.notgoogle.com/open?id=someSortOfId123")));
  EXPECT_EQ(base::nullopt,
            GetIdFromDriveUrl(GURL(
                "https://site.com/a/site.com/file/d/definitelyNotDrive/view")));
  EXPECT_EQ(
      base::nullopt,
      GetIdFromDriveUrl(GURL("file:///home/chronos/user/Downloads/file.zip")));
  EXPECT_EQ(base::nullopt,
            GetIdFromDriveUrl(GURL("http://drive.google.com/open?id=fancyId")));
}

TEST_F(PluginVmUtilTest, DriveUrlPatternWithOpen) {
  EXPECT_EQ("fancyId", GetIdFromDriveUrl(
                           GURL("https://drive.google.com/open?id=fancyId")));
  EXPECT_EQ("fancyId2",
            GetIdFromDriveUrl(
                GURL("https://drive.google.com/open?id=fancyId2&foo=bar")));
  EXPECT_EQ(
      "SomeCoolId000",
      GetIdFromDriveUrl(GURL(
          "https://drive.google.com/open?bar=foo&id=SomeCoolId000&foo=bar")));
}

TEST_F(PluginVmUtilTest, DriveUrlPatternWithView) {
  EXPECT_EQ("Id123",
            GetIdFromDriveUrl(GURL("https://drive.google.com/a/google.com/file/"
                                   "d/Id123/view?usp=sharing")));
  EXPECT_EQ("PluginVmIsCool",
            GetIdFromDriveUrl(GURL("https://drive.google.com/a/fancydomain.org/"
                                   "file/d/PluginVmIsCool/view")));

  EXPECT_EQ("hello",
            GetIdFromDriveUrl(GURL(
                "https://drive.google.com/file/d/hello/view?usp=sharing")));
  EXPECT_EQ("w-r-d", GetIdFromDriveUrl(
                         GURL("https://drive.google.com/file/d/w-r-d/view")));
}

}  // namespace plugin_vm
