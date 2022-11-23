// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bruschetta {

namespace {
const char kTestVmName[] = "vm_name";
const char kTestVmConfig[] = "vm_config";
}  // namespace

class BruschettaServiceTest : public testing::Test {
 public:
  BruschettaServiceTest() = default;
  BruschettaServiceTest(const BruschettaServiceTest&) = delete;
  BruschettaServiceTest& operator=(const BruschettaServiceTest&) = delete;
  ~BruschettaServiceTest() override = default;

 protected:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {ash::features::kBruschetta, ash::features::kBruschettaAlphaMigrate},
        {});
    service_ = std::make_unique<BruschettaService>(&profile_);
  }

  void TearDown() override {}

  void EnableByPolicy() {
    base::Value::Dict pref;
    base::Value::Dict config;
    config.Set(prefs::kPolicyEnabledKey,
               static_cast<int>(prefs::PolicyEnabledState::RUN_ALLOWED));

    pref.Set(kTestVmConfig, std::move(config));
    profile_.GetPrefs()->SetDict(prefs::kBruschettaVMConfiguration,
                                 std::move(pref));
  }

  void DisableByPolicy() {
    profile_.GetPrefs()->ClearPref(prefs::kBruschettaVMConfiguration);
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<BruschettaService> service_;
};

TEST_F(BruschettaServiceTest, GetLauncherForMigratedVm) {
  ASSERT_NE(service_->GetLauncher("bru"), nullptr);
}

TEST_F(BruschettaServiceTest, GetLauncherPolicyEnabled) {
  EnableByPolicy();
  service_->RegisterInPrefs(MakeBruschettaId(kTestVmName), kTestVmConfig);
  ASSERT_NE(service_->GetLauncher(kTestVmName), nullptr);
}

TEST_F(BruschettaServiceTest, GetLauncherPolicyDisabled) {
  DisableByPolicy();
  service_->RegisterInPrefs(MakeBruschettaId(kTestVmName), kTestVmConfig);
  ASSERT_EQ(service_->GetLauncher(kTestVmName), nullptr);
}

TEST_F(BruschettaServiceTest, GetLauncherPolicyUpdate) {
  EnableByPolicy();
  service_->RegisterInPrefs(MakeBruschettaId(kTestVmName), kTestVmConfig);
  DisableByPolicy();
  ASSERT_EQ(service_->GetLauncher(kTestVmName), nullptr);
  EnableByPolicy();
  ASSERT_NE(service_->GetLauncher(kTestVmName), nullptr);
}

}  // namespace bruschetta
