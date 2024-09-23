// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"

#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace plugin_vm {

class PluginVmUtilTest : public testing::Test {
 public:
  PluginVmUtilTest() = default;

  PluginVmUtilTest(const PluginVmUtilTest&) = delete;
  PluginVmUtilTest& operator=(const PluginVmUtilTest&) = delete;

  MOCK_METHOD(void, OnAvailabilityChanged, (bool, bool));

 protected:
  struct ScopedDBusClients {
    ScopedDBusClients() {
      ash::ConciergeClient::InitializeFake(
          /*fake_cicerone_client=*/nullptr);
    }
    ~ScopedDBusClients() { ash::ConciergeClient::Shutdown(); }
  } dbus_clients_;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<PluginVmTestHelper> test_helper_;

  void SetUp() override {
    testing_profile_ = std::make_unique<TestingProfile>();
    test_helper_ = std::make_unique<PluginVmTestHelper>(testing_profile_.get());
  }
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

TEST_F(PluginVmUtilTest, AvailabilitySubscription) {
  PluginVmAvailabilitySubscription subscription(
      testing_profile_.get(),
      base::BindRepeating(&PluginVmUtilTest::OnAvailabilityChanged,
                          base::Unretained(this)));

  // Callback args are: (is_allowed, is_configured).

  EXPECT_FALSE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));

  EXPECT_CALL(*this, OnAvailabilityChanged(true, false));
  test_helper_->AllowPluginVm();
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnAvailabilityChanged(false, false));
  testing_profile_->ScopedCrosSettingsTestHelper()->SetBoolean(
      ash::kPluginVmAllowed, false);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnAvailabilityChanged(true, false));
  testing_profile_->ScopedCrosSettingsTestHelper()->SetBoolean(
      ash::kPluginVmAllowed, true);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnAvailabilityChanged(true, true));
  testing_profile_->GetPrefs()->SetBoolean(
      plugin_vm::prefs::kPluginVmImageExists, true);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnAvailabilityChanged(false, true));
  testing_profile_->GetPrefs()->SetBoolean(plugin_vm::prefs::kPluginVmAllowed,
                                           false);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnAvailabilityChanged(true, true));
  testing_profile_->GetPrefs()->SetBoolean(plugin_vm::prefs::kPluginVmAllowed,
                                           true);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnAvailabilityChanged(false, true));
  testing_profile_->GetPrefs()->SetString(plugin_vm::prefs::kPluginVmUserId,
                                          "");
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnAvailabilityChanged(false, false));
  testing_profile_->GetPrefs()->SetBoolean(
      plugin_vm::prefs::kPluginVmImageExists, false);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnAvailabilityChanged(true, false));
  const std::string kPluginVmUserId = "fancy-user-id";
  testing_profile_->GetPrefs()->SetString(plugin_vm::prefs::kPluginVmUserId,
                                          kPluginVmUserId);
  testing::Mock::VerifyAndClearExpectations(this);
}

TEST_F(PluginVmUtilTest, DriveUrlNonMatches) {
  EXPECT_EQ(std::nullopt,
            GetIdFromDriveUrl(GURL(
                "http://192.168.0.2?id=Yxhi5BDTxsEl9onT8AunH4o_tkKviFGjY")));
  EXPECT_EQ(std::nullopt,
            GetIdFromDriveUrl(
                GURL("https://drive.notgoogle.com/open?id=someSortOfId123")));
  EXPECT_EQ(std::nullopt,
            GetIdFromDriveUrl(GURL(
                "https://site.com/a/site.com/file/d/definitelyNotDrive/view")));
  EXPECT_EQ(std::nullopt,
            GetIdFromDriveUrl(
                GURL("file:///home/chronos/user/MyFiles/Downloads/file.zip")));
  EXPECT_EQ(std::nullopt,
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
