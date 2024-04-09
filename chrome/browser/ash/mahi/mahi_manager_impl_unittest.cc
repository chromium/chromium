// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_manager_impl.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

namespace {
using ::testing::IsNull;
}  // namespace

namespace ash {

class MahiManagerImplTest : public NoSessionAshTestBase {
 public:
  MahiManagerImplTest()
      : NoSessionAshTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  MahiManagerImplTest(const MahiManagerImplTest&) = delete;
  MahiManagerImplTest& operator=(const MahiManagerImplTest&) = delete;

  ~MahiManagerImplTest() override = default;

  // NoSessionAshTestBase::
  void SetUp() override {
    NoSessionAshTestBase::SetUp();
    mahi_manager_impl_ = std::make_unique<MahiManagerImpl>();
    CreateUserSessions(1);
  }

  void TearDown() override {
    mahi_manager_impl_.reset();
    NoSessionAshTestBase::TearDown();
  }

  views::Widget* GetMahiPanelWidget() {
    if (!mahi_manager_impl_->mahi_panel_widget_) {
      return nullptr;
    }
    return mahi_manager_impl_->mahi_panel_widget_->AsWidget();
  }

  void SetUserPref(bool enabled) {
    Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
        ash::prefs::kMahiEnabled, enabled);
  }

  bool IsEnabled() const { return mahi_manager_impl_->IsEnabled(); }

 protected:
  std::unique_ptr<MahiManagerImpl> mahi_manager_impl_;

 private:
  base::test::ScopedFeatureList feature_list_{chromeos::features::kMahi};
  base::AutoReset<bool> ignore_mahi_secret_key_ =
      ash::switches::SetIgnoreMahiSecretKeyForTest();
};

TEST_F(MahiManagerImplTest, SetMahiPrefOnLogin) {
  // Checks that it should work for both when the first user's default pref is
  // true or false.
  for (bool mahi_enabled : {false, true}) {
    // Sets the pref for the default user.
    SetUserPref(mahi_enabled);
    ASSERT_EQ(IsEnabled(), mahi_enabled);
    const AccountId user1_account_id =
        Shell::Get()->session_controller()->GetActiveAccountId();

    // Sets the pref for the second user.
    SimulateUserLogin("other@user.test");
    SetUserPref(!mahi_enabled);
    EXPECT_EQ(IsEnabled(), !mahi_enabled);

    // Switching back to the previous user will update to correct pref.
    GetSessionControllerClient()->SwitchActiveUser(user1_account_id);
    EXPECT_EQ(IsEnabled(), mahi_enabled);

    // Clears all logins and re-logins the default user.
    GetSessionControllerClient()->Reset();
    SimulateUserLogin(user1_account_id);
  }
}

TEST_F(MahiManagerImplTest, OnPreferenceChanged) {
  for (bool mahi_enabled : {false, true, false}) {
    SetUserPref(mahi_enabled);
    EXPECT_EQ(IsEnabled(), mahi_enabled);
  }
}

class MahiManagerImplFeatureKeyTest : public NoSessionAshTestBase {
 public:
  MahiManagerImplFeatureKeyTest() {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(ash::switches::kMahiFeatureKey, "hello");
  }

  // NoSessionAshTestBase::
  void SetUp() override {
    NoSessionAshTestBase::SetUp();
    mahi_manager_impl_ = std::make_unique<MahiManagerImpl>();
    CreateUserSessions(1);
  }

  void TearDown() override {
    mahi_manager_impl_.reset();
    NoSessionAshTestBase::TearDown();
  }

 protected:
  views::Widget* GetMahiPanelWidget() {
    return mahi_manager_impl_->mahi_panel_widget_.get();
  }
  std::unique_ptr<MahiManagerImpl> mahi_manager_impl_;

 private:
  base::test::ScopedFeatureList feature_list_{chromeos::features::kMahi};
};

TEST_F(MahiManagerImplFeatureKeyTest, DoesNotShowWidgetIfFeatureKeyIsWrong) {
  mahi_manager_impl_->OpenMahiPanel(/*display_id=*/0);

  EXPECT_THAT(GetMahiPanelWidget(), IsNull());
}

}  // namespace ash
