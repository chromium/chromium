// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/tray_action/tray_action.h"

#include <memory>
#include <vector>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/tray_action/test_tray_action_client.h"
#include "ash/tray_action/tray_action_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"

using ash::mojom::TrayActionState;

namespace ash {

namespace {

class ScopedTestStateObserver : public TrayActionObserver {
 public:
  explicit ScopedTestStateObserver(TrayAction* tray_action)
      : tray_action_(tray_action) {
    tray_action_->AddObserver(this);
  }

  ScopedTestStateObserver(const ScopedTestStateObserver&) = delete;
  ScopedTestStateObserver& operator=(const ScopedTestStateObserver&) = delete;

  ~ScopedTestStateObserver() override { tray_action_->RemoveObserver(this); }

  // TrayActionObserver:
  void OnLockScreenNoteStateChanged(TrayActionState state) override {
    observed_states_.push_back(state);
  }

  const std::vector<TrayActionState>& observed_states() const {
    return observed_states_;
  }

  void ClearObservedStates() { observed_states_.clear(); }

 private:
  raw_ptr<TrayAction> tray_action_;

  std::vector<TrayActionState> observed_states_;
};

using TrayActionTest = AshTestBase;

}  // namespace

TEST_F(TrayActionTest, NoTrayActionClient) {
  TrayAction* tray_action = Shell::Get()->tray_action();
  ScopedTestStateObserver observer(tray_action);

  EXPECT_EQ(TrayActionState::kNotAvailable,
            tray_action->GetLockScreenNoteState());

  tray_action->UpdateLockScreenNoteState(TrayActionState::kAvailable);

  // The effective state should be |kNotAvailable| as long as an action handler
  // is not set.
  EXPECT_EQ(TrayActionState::kNotAvailable,
            tray_action->GetLockScreenNoteState());
  EXPECT_EQ(0u, observer.observed_states().size());

  std::unique_ptr<TestTrayActionClient> action_client =
      std::make_unique<TestTrayActionClient>();
  tray_action->SetClient(action_client->CreateRemoteAndBind(),
                         TrayActionState::kLaunching);

  EXPECT_EQ(TrayActionState::kLaunching, tray_action->GetLockScreenNoteState());
  ASSERT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kLaunching, observer.observed_states()[0]);
  observer.ClearObservedStates();

  action_client.reset();
  tray_action->FlushMojoForTesting();

  EXPECT_EQ(TrayActionState::kNotAvailable,
            tray_action->GetLockScreenNoteState());
  EXPECT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kNotAvailable, observer.observed_states()[0]);
}

TEST_F(TrayActionTest, SettingInitialState) {
  TrayAction* tray_action = Shell::Get()->tray_action();

  ScopedTestStateObserver observer(tray_action);
  TestTrayActionClient action_client;
  tray_action->SetClient(action_client.CreateRemoteAndBind(),
                         TrayActionState::kAvailable);

  EXPECT_EQ(TrayActionState::kAvailable, tray_action->GetLockScreenNoteState());
  ASSERT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, observer.observed_states()[0]);
}

TEST_F(TrayActionTest, StateChangeNotificationOnConnectionLoss) {
  TrayAction* tray_action = Shell::Get()->tray_action();

  ScopedTestStateObserver observer(tray_action);
  std::unique_ptr<TestTrayActionClient> action_client(
      new TestTrayActionClient());
  tray_action->SetClient(action_client->CreateRemoteAndBind(),
                         TrayActionState::kAvailable);

  EXPECT_EQ(TrayActionState::kAvailable, tray_action->GetLockScreenNoteState());
  ASSERT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, observer.observed_states()[0]);
  observer.ClearObservedStates();

  action_client.reset();
  tray_action->FlushMojoForTesting();

  EXPECT_EQ(TrayActionState::kNotAvailable,
            tray_action->GetLockScreenNoteState());
  ASSERT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kNotAvailable, observer.observed_states()[0]);
}

TEST_F(TrayActionTest, NormalStateProgression) {
  TrayAction* tray_action = Shell::Get()->tray_action();

  ScopedTestStateObserver observer(tray_action);
  TestTrayActionClient action_client;
  tray_action->SetClient(action_client.CreateRemoteAndBind(),
                         TrayActionState::kNotAvailable);

  tray_action->UpdateLockScreenNoteState(TrayActionState::kAvailable);
  EXPECT_EQ(TrayActionState::kAvailable, tray_action->GetLockScreenNoteState());
  EXPECT_FALSE(tray_action->IsLockScreenNoteActive());
  ASSERT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, observer.observed_states()[0]);
  observer.ClearObservedStates();

  tray_action->UpdateLockScreenNoteState(TrayActionState::kLaunching);
  EXPECT_EQ(TrayActionState::kLaunching, tray_action->GetLockScreenNoteState());
  EXPECT_FALSE(tray_action->IsLockScreenNoteActive());
  ASSERT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kLaunching, observer.observed_states()[0]);
  observer.ClearObservedStates();

  tray_action->UpdateLockScreenNoteState(TrayActionState::kActive);
  EXPECT_EQ(TrayActionState::kActive, tray_action->GetLockScreenNoteState());
  EXPECT_TRUE(tray_action->IsLockScreenNoteActive());
  ASSERT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kActive, observer.observed_states()[0]);
  observer.ClearObservedStates();

  tray_action->UpdateLockScreenNoteState(TrayActionState::kNotAvailable);
  EXPECT_EQ(TrayActionState::kNotAvailable,
            tray_action->GetLockScreenNoteState());
  EXPECT_FALSE(tray_action->IsLockScreenNoteActive());
  ASSERT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kNotAvailable, observer.observed_states()[0]);
}

TEST_F(TrayActionTest, ObserversNotNotifiedOnDuplicateState) {
  TrayAction* tray_action = Shell::Get()->tray_action();

  ScopedTestStateObserver observer(tray_action);
  TestTrayActionClient action_client;
  tray_action->SetClient(action_client.CreateRemoteAndBind(),
                         TrayActionState::kNotAvailable);

  tray_action->UpdateLockScreenNoteState(TrayActionState::kAvailable);
  EXPECT_EQ(TrayActionState::kAvailable, tray_action->GetLockScreenNoteState());
  ASSERT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, observer.observed_states()[0]);
  observer.ClearObservedStates();

  tray_action->UpdateLockScreenNoteState(TrayActionState::kAvailable);
  EXPECT_EQ(TrayActionState::kAvailable, tray_action->GetLockScreenNoteState());
  ASSERT_EQ(0u, observer.observed_states().size());
}

TEST_F(TrayActionTest, RequestAction) {
  TrayAction* tray_action = Shell::Get()->tray_action();

  TestTrayActionClient action_client;
  tray_action->SetClient(action_client.CreateRemoteAndBind(),
                         TrayActionState::kNotAvailable);

  EXPECT_TRUE(action_client.note_origins().empty());
  tray_action->RequestNewLockScreenNote(
      mojom::LockScreenNoteOrigin::kLockScreenButtonTap);
  tray_action->FlushMojoForTesting();
  EXPECT_TRUE(action_client.note_origins().empty());

  tray_action->UpdateLockScreenNoteState(TrayActionState::kAvailable);
  tray_action->RequestNewLockScreenNote(
      mojom::LockScreenNoteOrigin::kTrayAction);
  tray_action->FlushMojoForTesting();
  EXPECT_EQ(std::vector<mojom::LockScreenNoteOrigin>(
                {mojom::LockScreenNoteOrigin::kTrayAction}),
            action_client.note_origins());
}

// Tests that there is no crash if handler is not set.
TEST_F(TrayActionTest, RequestActionWithNoHandler) {
  TrayAction* tray_action = Shell::Get()->tray_action();
  tray_action->RequestNewLockScreenNote(
      mojom::LockScreenNoteOrigin::kLockScreenButtonTap);
  tray_action->FlushMojoForTesting();
}

TEST_F(TrayActionTest, CloseLockScreenNote) {
  TrayAction* tray_action = Shell::Get()->tray_action();

  TestTrayActionClient action_client;
  tray_action->SetClient(action_client.CreateRemoteAndBind(),
                         TrayActionState::kNotAvailable);

  tray_action->UpdateLockScreenNoteState(TrayActionState::kActive);
  EXPECT_TRUE(action_client.close_note_reasons().empty());
  tray_action->CloseLockScreenNote(
      mojom::CloseLockScreenNoteReason::kUnlockButtonPressed);
  tray_action->FlushMojoForTesting();
  EXPECT_EQ(std::vector<mojom::CloseLockScreenNoteReason>(
                {mojom::CloseLockScreenNoteReason::kUnlockButtonPressed}),
            action_client.close_note_reasons());
}

// Tests that there is no crash if handler is not set.
TEST_F(TrayActionTest, CloseLockScreenNoteWithNoHandler) {
  TrayAction* tray_action = Shell::Get()->tray_action();
  tray_action->CloseLockScreenNote(
      mojom::CloseLockScreenNoteReason::kUnlockButtonPressed);
  tray_action->FlushMojoForTesting();
}

}  // namespace ash
