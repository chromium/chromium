// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/access_loss/password_access_loss_warning_bridge_impl.h"

#include "base/android/build_info.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_store/split_stores_and_local_upm.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class PasswordAccessLossWarningBridgeImplTest : public testing::Test {
 public:
  PasswordAccessLossWarningBridgeImplTest() {
    // The access loss warning should not be shown to users without passwords in
    // the profile store.
    pref_service_.registry()->RegisterIntegerPref(
        password_manager::prefs::kTotalPasswordsAvailableForProfile, 1);

    base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
        getGMSVersionForTestSetUp(/*is_up_to_date=*/false));
  }

  std::string getGMSVersionForTestSetUp(bool is_up_to_date) {
    int min_version = password_manager::GetLocalUpmMinGmsVersion();
    return base::NumberToString(is_up_to_date ? min_version : min_version - 1);
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }
  PasswordAccessLossWarningBridgeImpl* bridge() { return &bridge_; }

 private:
  TestingPrefServiceSimple pref_service_;
  PasswordAccessLossWarningBridgeImpl bridge_;
};

TEST_F(PasswordAccessLossWarningBridgeImplTest,
       ShouldNotShowWarningWhenFlagIsOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning);

  EXPECT_FALSE(bridge()->ShouldShowAccessLossNoticeSheet(pref_service()));
}

TEST_F(PasswordAccessLossWarningBridgeImplTest,
       ShouldNotShowWarningWithNoWarningType) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning);
  base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
      getGMSVersionForTestSetUp(/*is_up_to_date=*/true));

  EXPECT_FALSE(bridge()->ShouldShowAccessLossNoticeSheet(pref_service()));
}

TEST_F(PasswordAccessLossWarningBridgeImplTest,
       ShouldShowWarningWithAllThePreconditionsSatisfied) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning);

  EXPECT_TRUE(bridge()->ShouldShowAccessLossNoticeSheet(pref_service()));
}
