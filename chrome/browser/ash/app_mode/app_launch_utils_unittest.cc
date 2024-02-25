// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/app_launch_utils.h"

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const char kEmptyPrefsFile[] = "{}";

class ScopedKioskPreferencesListForTesting {
 public:
  explicit ScopedKioskPreferencesListForTesting(
      const std::vector<std::string>& prefs_to_reset)
      : prefs_(prefs_to_reset) {
    SetEphemeralKioskPreferencesListForTesting(&prefs_);
  }
  ~ScopedKioskPreferencesListForTesting() {
    SetEphemeralKioskPreferencesListForTesting(nullptr);
  }

 private:
  std::vector<std::string> prefs_;
};

}  // namespace

class AppLaunchUtilsTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();

    const AccountId account_id = AccountId::FromUserEmail("lala@example.com");
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    fake_user_manager_->AddWebKioskAppUser(account_id);
    fake_user_manager_->LoginUser(account_id);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    input_file_ = temp_dir_.GetPath().AppendASCII("prefs.json");
    ASSERT_TRUE(base::WriteFile(input_file_, kEmptyPrefsFile));

    registry_ = new PrefRegistrySimple;
  }

  std::unique_ptr<PrefService> CreatePrefService() {
    PrefServiceFactory pref_service_factory;
    pref_store_ = new JsonPrefStore(input_file_);
    pref_service_factory.set_user_prefs(pref_store_);
    return pref_service_factory.Create(registry_);
  }

 public:
  base::test::TaskEnvironment task_environment_;
  // The path to temporary directory used to contain the test operations.
  base::ScopedTempDir temp_dir_;
  base::FilePath input_file_;

  scoped_refptr<JsonPrefStore> pref_store_;
  scoped_refptr<PrefRegistrySimple> registry_;

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
};

TEST_F(AppLaunchUtilsTest, ClearUserPrefs) {
  const std::string pref_name = "pref1";
  const std::string pref2_name = "pref2";

  registry_->RegisterBooleanPref(pref_name, false);
  registry_->RegisterBooleanPref(pref2_name, false);
  auto pref_service = CreatePrefService();

  ScopedKioskPreferencesListForTesting prefs({pref_name});
  pref_service->SetBoolean(pref_name, true);
  pref_service->SetBoolean(pref2_name, true);
  ResetEphemeralKioskPreferences(pref_service.get());
  EXPECT_FALSE(pref_service->GetBoolean(pref_name));
  EXPECT_TRUE(pref_service->GetBoolean(pref2_name));
}

TEST_F(AppLaunchUtilsTest, ClearSubPrefs) {
  const std::string pref_a = "pref1.a";
  const std::string pref_b = "pref1.b";
  const std::string pref_common = "pref1";
  ScopedKioskPreferencesListForTesting prefs({pref_common});

  registry_->RegisterBooleanPref(pref_a, false);
  registry_->RegisterBooleanPref(pref_b, false);
  auto pref_service = CreatePrefService();

  pref_service->SetBoolean(pref_a, true);
  pref_service->SetBoolean(pref_b, true);

  ResetEphemeralKioskPreferences(pref_service.get());
  EXPECT_FALSE(pref_service->GetBoolean(pref_a));
  EXPECT_FALSE(pref_service->GetBoolean(pref_b));
}

}  // namespace ash
