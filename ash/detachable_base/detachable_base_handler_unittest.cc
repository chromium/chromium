// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/detachable_base/detachable_base_handler.h"

#include <memory>
#include <utility>

#include "ash/detachable_base/detachable_base_observer.h"
#include "ash/detachable_base/detachable_base_pairing_status.h"
#include "ash/public/cpp/session/user_info.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/dbus/hammerd/fake_hammerd_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

enum class UserType {
  kNormal,
  kEphemeral,
};

UserInfo CreateUser(const std::string& email,
                    const std::string& gaia_id,
                    UserType user_type) {
  UserInfo user;
  user.account_id = AccountId::FromUserEmailGaiaId(email, gaia_id);
  user.is_ephemeral = user_type == UserType::kEphemeral;
  return user;
}

class TestBaseObserver : public DetachableBaseObserver {
 public:
  TestBaseObserver() = default;
  ~TestBaseObserver() override = default;

  int pairing_status_changed_count() const {
    return pairing_status_changed_count_;
  }

  void reset_pairing_status_changed_count() {
    pairing_status_changed_count_ = 0;
  }

  int update_required_changed_count() const {
    return update_required_changed_count_;
  }

  bool requires_update() const { return requires_update_; }

  // DetachableBaseObserver:
  void OnDetachableBasePairingStatusChanged(
      DetachableBasePairingStatus status) override {
    pairing_status_changed_count_++;
  }

  void OnDetachableBaseRequiresUpdateChanged(bool requires_update) override {
    update_required_changed_count_++;
    requires_update_ = requires_update;
  }

 private:
  int pairing_status_changed_count_ = 0;
  int update_required_changed_count_ = 0;
  bool requires_update_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestBaseObserver);
};

}  // namespace

class DetachableBaseHandlerTest : public testing::Test {
 public:
  DetachableBaseHandlerTest() = default;
  ~DetachableBaseHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    chromeos::HammerdClient::InitializeFake();
    hammerd_client_ = chromeos::FakeHammerdClient::Get();

    chromeos::PowerManagerClient::InitializeFake();
    chromeos::FakePowerManagerClient::Get()->SetTabletMode(
        chromeos::PowerManagerClient::TabletMode::OFF, base::TimeTicks());

    default_user_ = CreateUser("user_1@foo.bar", "111111", UserType::kNormal);

    DetachableBaseHandler::RegisterPrefs(local_state_.registry());
    handler_ = std::make_unique<DetachableBaseHandler>(&local_state_);
    handler_->AddObserver(&detachable_base_observer_);
  }

  void TearDown() override {
    handler_->RemoveObserver(&detachable_base_observer_);
    handler_.reset();
    hammerd_client_ = nullptr;
    chromeos::PowerManagerClient::Shutdown();
    chromeos::HammerdClient::Shutdown();
  }

 protected:
  // Simulates system events associated with the detachable base being switched.
  void ChangePairedBase(const std::vector<uint8_t>& base_id) {
    chromeos::FakePowerManagerClient::Get()->SetTabletMode(
        chromeos::PowerManagerClient::TabletMode::ON, base::TimeTicks());
    chromeos::FakePowerManagerClient::Get()->SetTabletMode(
        chromeos::PowerManagerClient::TabletMode::OFF, base::TimeTicks());
    detachable_base_observer_.reset_pairing_status_changed_count();

    hammerd_client_->FirePairChallengeSucceededSignal(base_id);
  }

  void RestartHandler() {
    handler_->RemoveObserver(&detachable_base_observer_);
    handler_ = std::make_unique<DetachableBaseHandler>(&local_state_);
    handler_->AddObserver(&detachable_base_observer_);
  }

  chromeos::FakeHammerdClient* hammerd_client_ = nullptr;

  TestBaseObserver detachable_base_observer_;

  std::unique_ptr<DetachableBaseHandler> handler_;

  UserInfo default_user_;

 private:
  base::test::TaskEnvironment task_environment_;

  TestingPrefServiceSimple local_state_;

  DISALLOW_COPY_AND_ASSIGN(DetachableBaseHandlerTest);
};

TEST_F(DetachableBaseHandlerTest, NoDetachableBase) {
  // Run loop so the handler picks up initial power manager state.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(DetachableBasePairingStatus::kNone, handler_->GetPairingStatus());
  EXPECT_EQ(0, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
}

TEST_F(DetachableBaseHandlerTest, TabletModeOnOnStartup) {
  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::ON, base::TimeTicks());
  RestartHandler();

  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::OFF, base::TimeTicks());
  hammerd_client_->FirePairChallengeSucceededSignal({0x01, 0x02, 0x03, 0x04});

  // Run loop so the handler picks up initial power manager state.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(DetachableBasePairingStatus::kAuthenticated,
            handler_->GetPairingStatus());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_TRUE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
}

TEST_F(DetachableBaseHandlerTest, SuccessfullPairing) {
  // Run loop so the handler picks up initial power manager state.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(DetachableBasePairingStatus::kNone, handler_->GetPairingStatus());
  hammerd_client_->FirePairChallengeSucceededSignal({0x01, 0x02, 0x03, 0x04});

  EXPECT_EQ(DetachableBasePairingStatus::kAuthenticated,
            handler_->GetPairingStatus());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  // The user should not be notified when they attach a base for the first time,
  // so the first paired base should be reported as kAuthenticated rather than
  // kAuthenticatedNotMatchingLastUsed.
  EXPECT_TRUE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  detachable_base_observer_.reset_pairing_status_changed_count();

  // Assume the base has been detached when the device switches to tablet mode.
  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::ON, base::TimeTicks());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_EQ(DetachableBasePairingStatus::kNone, handler_->GetPairingStatus());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  detachable_base_observer_.reset_pairing_status_changed_count();

  // When the device exits tablet mode again, the base should not be reported
  // as paired until it's finished pairing.
  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::OFF, base::TimeTicks());
  EXPECT_EQ(DetachableBasePairingStatus::kNone, handler_->GetPairingStatus());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));

  hammerd_client_->FirePairChallengeSucceededSignal({0x01, 0x02, 0x03, 0x04});
  EXPECT_EQ(DetachableBasePairingStatus::kAuthenticated,
            handler_->GetPairingStatus());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_TRUE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  detachable_base_observer_.reset_pairing_status_changed_count();
}

TEST_F(DetachableBaseHandlerTest, DetachableBasePairingFailure) {
  // Run loop so the handler picks up initial power manager state.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(DetachableBasePairingStatus::kNone, handler_->GetPairingStatus());
  hammerd_client_->FirePairChallengeFailedSignal();

  EXPECT_EQ(DetachableBasePairingStatus::kNotAuthenticated,
            handler_->GetPairingStatus());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  detachable_base_observer_.reset_pairing_status_changed_count();

  // Assume the base has been detached when the device switches to tablet mode.
  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::ON, base::TimeTicks());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_EQ(DetachableBasePairingStatus::kNone, handler_->GetPairingStatus());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  detachable_base_observer_.reset_pairing_status_changed_count();
}

TEST_F(DetachableBaseHandlerTest, InvalidDetachableBase) {
  // Run loop so the handler picks up initial power manager state.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(DetachableBasePairingStatus::kNone, handler_->GetPairingStatus());
  hammerd_client_->FireInvalidBaseConnectedSignal();

  EXPECT_EQ(DetachableBasePairingStatus::kInvalidDevice,
            handler_->GetPairingStatus());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  detachable_base_observer_.reset_pairing_status_changed_count();

  // Assume the base has been detached when the device switches to tablet mode.
  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::ON, base::TimeTicks());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_EQ(DetachableBasePairingStatus::kNone, handler_->GetPairingStatus());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  detachable_base_observer_.reset_pairing_status_changed_count();
}

TEST_F(DetachableBaseHandlerTest, PairingSuccessDuringInit) {
  hammerd_client_->FirePairChallengeSucceededSignal({0x01, 0x02, 0x03, 0x04});

  // DetachableBaseHandler updates base pairing state only after it confirms the
  // device is not in tablet mode.
  EXPECT_EQ(0, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_EQ(DetachableBasePairingStatus::kNone, handler_->GetPairingStatus());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));

  // Run loop so the callback for getting the initial power manager state gets
  // run.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_EQ(DetachableBasePairingStatus::kAuthenticated,
            handler_->GetPairingStatus());
  EXPECT_TRUE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  detachable_base_observer_.reset_pairing_status_changed_count();
}

TEST_F(DetachableBaseHandlerTest, PairingFailDuringInit) {
  hammerd_client_->FirePairChallengeFailedSignal();

  // DetachableBaseHandler updates base pairing state only after it confirms the
  // device is not in tablet mode.
  EXPECT_EQ(0, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_EQ(DetachableBasePairingStatus::kNone, handler_->GetPairingStatus());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));

  // Run loop so the callback for getting the initial power manager state gets
  // run.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_EQ(DetachableBasePairingStatus::kNotAuthenticated,
            handler_->GetPairingStatus());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  detachable_base_observer_.reset_pairing_status_changed_count();
}

TEST_F(DetachableBaseHandlerTest, InvalidDeviceDuringInit) {
  hammerd_client_->FireInvalidBaseConnectedSignal();

  // DetachableBaseHandler updates base pairing state only after it confirms the
  // device is not in tablet mode.
  EXPECT_EQ(0, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_EQ(DetachableBasePairingStatus::kNone, handler_->GetPairingStatus());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));

  // Run loop so the callback for getting the initial power manager state gets
  // run.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_EQ(DetachableBasePairingStatus::kInvalidDevice,
            handler_->GetPairingStatus());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  detachable_base_observer_.reset_pairing_status_changed_count();
}

TEST_F(DetachableBaseHandlerTest, TabletModeTurnedOnDuringHandlerInit) {
  hammerd_client_->FirePairChallengeSucceededSignal({0x01, 0x02, 0x03, 0x04});
  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::ON, base::TimeTicks());

  // Run loop so the callback for getting the initial power manager state gets
  // run.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_EQ(DetachableBasePairingStatus::kNone, handler_->GetPairingStatus());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
}

TEST_F(DetachableBaseHandlerTest, DetachableBaseChangeDetection) {
  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::OFF, base::TimeTicks());
  // Run loop so the callback for getting the initial power manager state gets
  // run.
  base::RunLoop().RunUntilIdle();
  hammerd_client_->FirePairChallengeSucceededSignal({0x01, 0x02, 0x03, 0x04});

  EXPECT_EQ(DetachableBasePairingStatus::kAuthenticated,
            handler_->GetPairingStatus());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_TRUE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  detachable_base_observer_.reset_pairing_status_changed_count();

  // Set the current base as last used by the user.
  handler_->SetPairedBaseAsLastUsedByUser(default_user_);

  // Simulate the paired base change.
  ChangePairedBase({0x04, 0x05, 0x06, 0x07});

  EXPECT_EQ(DetachableBasePairingStatus::kAuthenticated,
            handler_->GetPairingStatus());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  detachable_base_observer_.reset_pairing_status_changed_count();

  // Switch back to last used base.
  ChangePairedBase({0x01, 0x02, 0x03, 0x04});

  EXPECT_EQ(DetachableBasePairingStatus::kAuthenticated,
            handler_->GetPairingStatus());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_TRUE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  detachable_base_observer_.reset_pairing_status_changed_count();

  // The last used base should be preserved if the detachable base handler is
  // restarted after the last used base was set.
  RestartHandler();
  base::RunLoop().RunUntilIdle();

  hammerd_client_->FirePairChallengeSucceededSignal({0x04, 0x05, 0x06, 0x07});

  EXPECT_EQ(DetachableBasePairingStatus::kAuthenticated,
            handler_->GetPairingStatus());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  detachable_base_observer_.reset_pairing_status_changed_count();
}

TEST_F(DetachableBaseHandlerTest, MultiUser) {
  // Assume the user_1 has a last used base.
  base::RunLoop().RunUntilIdle();
  hammerd_client_->FirePairChallengeSucceededSignal({0x01, 0x02, 0x03, 0x04});
  handler_->SetPairedBaseAsLastUsedByUser(default_user_);
  detachable_base_observer_.reset_pairing_status_changed_count();

  // Restart the handler, so it's initialized with the previously set up prefs
  // state.
  RestartHandler();
  base::RunLoop().RunUntilIdle();

  const UserInfo second_user =
      CreateUser("user_2@foo.bar", "222222", UserType::kNormal);

  EXPECT_EQ(DetachableBasePairingStatus::kNone, handler_->GetPairingStatus());
  EXPECT_EQ(0, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(second_user));

  // Pair a detachable base different than the one used by user_1.
  hammerd_client_->FirePairChallengeSucceededSignal({0x04, 0x05, 0x06, 0x07});

  EXPECT_EQ(DetachableBasePairingStatus::kAuthenticated,
            handler_->GetPairingStatus());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  // The base for user_1 has changed.
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  // User 2 has not used a detachable base yet - the base should be reported as
  // matching last used base.
  EXPECT_TRUE(handler_->PairedBaseMatchesLastUsedByUser(second_user));
  detachable_base_observer_.reset_pairing_status_changed_count();

  // Set the last used detachable base for user 2, and pair the initial base.
  handler_->SetPairedBaseAsLastUsedByUser(second_user);

  ChangePairedBase({0x01, 0x02, 0x03, 0x04});

  // This time, the second user should be notified the base has been changed.
  EXPECT_EQ(DetachableBasePairingStatus::kAuthenticated,
            handler_->GetPairingStatus());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_TRUE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(second_user));
  detachable_base_observer_.reset_pairing_status_changed_count();

  // Set the base for user 2 to the current one.
  handler_->SetPairedBaseAsLastUsedByUser(second_user);

  EXPECT_TRUE(handler_->PairedBaseMatchesLastUsedByUser(second_user));

  // When the base is paired next time, it should be considered authenticated
  // for both users.
  ChangePairedBase({0x01, 0x02, 0x03, 0x04});

  EXPECT_EQ(DetachableBasePairingStatus::kAuthenticated,
            handler_->GetPairingStatus());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_TRUE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  EXPECT_TRUE(handler_->PairedBaseMatchesLastUsedByUser(second_user));
  detachable_base_observer_.reset_pairing_status_changed_count();
}

TEST_F(DetachableBaseHandlerTest, SwitchToNonAuthenticatedBase) {
  // Run loop so the handler picks up initial power manager state.
  base::RunLoop().RunUntilIdle();

  hammerd_client_->FirePairChallengeSucceededSignal({0x01, 0x02, 0x03, 0x04});
  handler_->SetPairedBaseAsLastUsedByUser(default_user_);
  detachable_base_observer_.reset_pairing_status_changed_count();

  // Switch to non-trusted base, and verify it's reported as such regardless
  // of whether the user had previously used a detachable base.
  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::ON, base::TimeTicks());
  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::OFF, base::TimeTicks());
  detachable_base_observer_.reset_pairing_status_changed_count();

  hammerd_client_->FirePairChallengeFailedSignal();

  const UserInfo second_user =
      CreateUser("user_2@foo.bar", "222222", UserType::kNormal);

  EXPECT_EQ(DetachableBasePairingStatus::kNotAuthenticated,
            handler_->GetPairingStatus());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(second_user));
  detachable_base_observer_.reset_pairing_status_changed_count();
}

TEST_F(DetachableBaseHandlerTest, SwitchToInvalidBase) {
  // Run loop so the handler picks up initial power manager state.
  base::RunLoop().RunUntilIdle();

  hammerd_client_->FirePairChallengeSucceededSignal({0x01, 0x02, 0x03, 0x04});
  handler_->SetPairedBaseAsLastUsedByUser(default_user_);
  detachable_base_observer_.reset_pairing_status_changed_count();

  // Switch to an invalid base, and verify it's reported as such regardless
  // of whether the user had previously used a base.
  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::ON, base::TimeTicks());
  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::OFF, base::TimeTicks());
  detachable_base_observer_.reset_pairing_status_changed_count();

  hammerd_client_->FireInvalidBaseConnectedSignal();

  const UserInfo second_user =
      CreateUser("user_2@foo.bar", "222222", UserType::kNormal);

  EXPECT_EQ(DetachableBasePairingStatus::kInvalidDevice,
            handler_->GetPairingStatus());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(second_user));
  detachable_base_observer_.reset_pairing_status_changed_count();
}

TEST_F(DetachableBaseHandlerTest, RemoveUserData) {
  const UserInfo second_user =
      CreateUser("user_2@foo.bar", "222222", UserType::kNormal);

  // Assume the user_1 has a last used base.
  base::RunLoop().RunUntilIdle();
  hammerd_client_->FirePairChallengeSucceededSignal({0x01, 0x02, 0x03, 0x04});
  handler_->SetPairedBaseAsLastUsedByUser(default_user_);
  handler_->SetPairedBaseAsLastUsedByUser(second_user);
  detachable_base_observer_.reset_pairing_status_changed_count();

  ChangePairedBase({0x04, 0x05, 0x06});

  EXPECT_EQ(DetachableBasePairingStatus::kAuthenticated,
            handler_->GetPairingStatus());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(second_user));
  detachable_base_observer_.reset_pairing_status_changed_count();

  // Remove the data for user_2, and verify that the paired base is reported
  // as authenticated when the paired base changes again.
  handler_->RemoveUserData(second_user);
  ChangePairedBase({0x07, 0x08, 0x09});

  EXPECT_EQ(DetachableBasePairingStatus::kAuthenticated,
            handler_->GetPairingStatus());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  EXPECT_TRUE(handler_->PairedBaseMatchesLastUsedByUser(second_user));
  detachable_base_observer_.reset_pairing_status_changed_count();

  // Verify that paired base will be properly set again for the previously
  // removed user.
  handler_->SetPairedBaseAsLastUsedByUser(second_user);
  ChangePairedBase({0x01, 0x02, 0x03, 0x04});

  EXPECT_EQ(DetachableBasePairingStatus::kAuthenticated,
            handler_->GetPairingStatus());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_TRUE(handler_->PairedBaseMatchesLastUsedByUser(default_user_));
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(second_user));
  detachable_base_observer_.reset_pairing_status_changed_count();
}

TEST_F(DetachableBaseHandlerTest, EphemeralUser) {
  base::RunLoop().RunUntilIdle();

  const UserInfo ephemeral_user =
      CreateUser("user_3@foo.bar", "333333", UserType::kEphemeral);
  hammerd_client_->FirePairChallengeSucceededSignal({0x01, 0x02, 0x03, 0x04});
  handler_->SetPairedBaseAsLastUsedByUser(ephemeral_user);
  detachable_base_observer_.reset_pairing_status_changed_count();

  ChangePairedBase({0x04, 0x05, 0x06});

  EXPECT_EQ(DetachableBasePairingStatus::kAuthenticated,
            handler_->GetPairingStatus());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_FALSE(handler_->PairedBaseMatchesLastUsedByUser(ephemeral_user));
  detachable_base_observer_.reset_pairing_status_changed_count();

  ChangePairedBase({0x01, 0x02, 0x03, 0x04});

  EXPECT_EQ(DetachableBasePairingStatus::kAuthenticated,
            handler_->GetPairingStatus());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_TRUE(handler_->PairedBaseMatchesLastUsedByUser(ephemeral_user));
  detachable_base_observer_.reset_pairing_status_changed_count();

  // Verify that the information about the last used base gets lost if the
  // detachable base handler is reset (given that the info was saved for an
  // ephemeral user).
  RestartHandler();
  base::RunLoop().RunUntilIdle();
  hammerd_client_->FirePairChallengeSucceededSignal({0x04, 0x05, 0x06, 0x07});

  EXPECT_EQ(DetachableBasePairingStatus::kAuthenticated,
            handler_->GetPairingStatus());
  EXPECT_EQ(1, detachable_base_observer_.pairing_status_changed_count());
  EXPECT_TRUE(handler_->PairedBaseMatchesLastUsedByUser(ephemeral_user));
  detachable_base_observer_.reset_pairing_status_changed_count();
}

TEST_F(DetachableBaseHandlerTest, NotifyObserversWhenBaseUpdateRequired) {
  hammerd_client_->FireBaseFirmwareNeedUpdateSignal();
  EXPECT_EQ(1, detachable_base_observer_.update_required_changed_count());
  EXPECT_TRUE(detachable_base_observer_.requires_update());

  hammerd_client_->FireBaseFirmwareUpdateSucceededSignal();
  EXPECT_EQ(2, detachable_base_observer_.update_required_changed_count());
  EXPECT_FALSE(detachable_base_observer_.requires_update());
}

TEST_F(DetachableBaseHandlerTest, NotifyNoUpdateRequiredOnBaseDetach) {
  hammerd_client_->FireBaseFirmwareNeedUpdateSignal();
  EXPECT_EQ(1, detachable_base_observer_.update_required_changed_count());
  EXPECT_TRUE(detachable_base_observer_.requires_update());

  chromeos::FakePowerManagerClient::Get()->SetTabletMode(
      chromeos::PowerManagerClient::TabletMode::ON, base::TimeTicks());
  EXPECT_EQ(2, detachable_base_observer_.update_required_changed_count());
  EXPECT_FALSE(detachable_base_observer_.requires_update());
}

}  // namespace ash
