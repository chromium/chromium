// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/hps_notify_view.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "chromeos/dbus/hps/fake_hps_dbus_client.h"
#include "chromeos/dbus/hps/hps_dbus_client.h"

namespace ash {
namespace {

// Enables or disables the user pref for the feature.
void SetEnabledPref(bool enabled) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kSnoopingProtectionEnabled, enabled);
}

// Provides access to a fake daemon and an instance of the view hooked up to the
// test environment.
class HpsNotifyViewTest : public NoSessionAshTestBase {
 public:
  HpsNotifyViewTest() = default;
  HpsNotifyViewTest(const HpsNotifyViewTest&) = delete;
  HpsNotifyViewTest& operator=(const HpsNotifyViewTest&) = delete;
  ~HpsNotifyViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    chromeos::HpsDBusClient::InitializeFake();
    dbus_client_ = chromeos::FakeHpsDBusClient::Get();
  }

  void TearDown() override {
    view_.reset();
    dbus_client_ = nullptr;
    chromeos::HpsDBusClient::Shutdown();

    AshTestBase::TearDown();
  }

 protected:
  // Recreates the view. Used to defer view construction.
  void InitializeView() {
    view_ = std::make_unique<HpsNotifyView>(GetPrimaryShelf());
  }

  chromeos::FakeHpsDBusClient* dbus_client_ = nullptr;
  std::unique_ptr<HpsNotifyView> view_;
};

// Test that icon is hidden by default.
TEST_F(HpsNotifyViewTest, Hidden) {
  SimulateUserLogin("testuser@gmail.com");
  SetEnabledPref(false);
  dbus_client_->set_hps_notify_result(false);
  InitializeView();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_FALSE(view_->GetVisible());
}

// Test that initial daemon state is considered.
TEST_F(HpsNotifyViewTest, HpsState) {
  SimulateUserLogin("testuser@gmail.com");
  SetEnabledPref(true);
  dbus_client_->set_hps_notify_result(true);
  InitializeView();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_TRUE(view_->GetVisible());
}

// Test that messages from the daemon toggle the icon.
TEST_F(HpsNotifyViewTest, HpsStateChange) {
  SimulateUserLogin("testuser@gmail.com");
  SetEnabledPref(true);
  dbus_client_->set_hps_notify_result(false);
  InitializeView();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_FALSE(view_->GetVisible());

  view_->OnHpsNotifyChanged(true);

  EXPECT_TRUE(view_->GetVisible());

  view_->OnHpsNotifyChanged(false);

  EXPECT_FALSE(view_->GetVisible());
}

// Test that a user changing their preference toggles the icon.
TEST_F(HpsNotifyViewTest, PrefChanged) {
  SimulateUserLogin("testuser@gmail.com");
  SetEnabledPref(false);
  dbus_client_->set_hps_notify_result(true);
  InitializeView();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_FALSE(view_->GetVisible());

  SetEnabledPref(true);

  EXPECT_TRUE(view_->GetVisible());
}

// Test that eye icon isn't shown during the OOBE.
TEST_F(HpsNotifyViewTest, Oobe) {
  // Simulate end of OOBE when user is logged in.
  SimulateUserLogin("testuser@gmail.com");
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::OOBE);

  SetEnabledPref(true);
  dbus_client_->set_hps_notify_result(true);
  InitializeView();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_FALSE(view_->GetVisible());

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  EXPECT_TRUE(view_->GetVisible());
}

// Test that the eye icon isn't shown at the login page.
TEST_F(HpsNotifyViewTest, Login) {
  // Note: login deferred.

  SetEnabledPref(true);
  dbus_client_->set_hps_notify_result(true);
  InitializeView();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_FALSE(view_->GetVisible());

  SimulateUserLogin("testuser@gmail.com");

  // Don't show until new user has enabled their preference.
  EXPECT_FALSE(view_->GetVisible());

  SetEnabledPref(true);
  EXPECT_TRUE(view_->GetVisible());
}

}  // namespace
}  // namespace ash
