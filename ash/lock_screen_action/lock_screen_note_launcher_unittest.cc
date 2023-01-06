// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lock_screen_action/lock_screen_note_launcher.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/tray_action/test_tray_action_client.h"
#include "ash/tray_action/tray_action.h"
#include "base/functional/bind.h"

namespace ash {

namespace {

enum class LaunchStatus { kUnknown, kSuccess, kFailure };

void HandleLaunchCallback(LaunchStatus* result, bool success) {
  *result = success ? LaunchStatus::kSuccess : LaunchStatus::kFailure;
}

}  // namespace

using LockScreenNoteLauncherTest = AshTestBase;

TEST_F(LockScreenNoteLauncherTest, LaunchSuccess) {
  TrayAction* tray_action = Shell::Get()->tray_action();
  TestTrayActionClient action_client;
  tray_action->SetClient(action_client.CreateRemoteAndBind(),
                         mojom::TrayActionState::kAvailable);

  EXPECT_TRUE(LockScreenNoteLauncher::CanAttemptLaunch());
  LockScreenNoteLauncher launcher;

  LaunchStatus launch_status = LaunchStatus::kUnknown;
  ASSERT_TRUE(
      launcher.Run(mojom::LockScreenNoteOrigin::kLockScreenButtonTap,
                   base::BindOnce(&HandleLaunchCallback, &launch_status)));

  // Verify a lock screen action was requested.
  tray_action->FlushMojoForTesting();
  EXPECT_EQ(std::vector<mojom::LockScreenNoteOrigin>(
                {mojom::LockScreenNoteOrigin::kLockScreenButtonTap}),
            action_client.note_origins());

  // Move note action to launching state, and verify the launch callback is not
  // run.
  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kLaunching);
  EXPECT_EQ(LaunchStatus::kUnknown, launch_status);

  // Move note action to active state and verify that the launch callback is
  // run.
  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kActive);
  EXPECT_EQ(LaunchStatus::kSuccess, launch_status);
}

TEST_F(LockScreenNoteLauncherTest, LaunchFailure) {
  TrayAction* tray_action = Shell::Get()->tray_action();
  TestTrayActionClient action_client;
  tray_action->SetClient(action_client.CreateRemoteAndBind(),
                         mojom::TrayActionState::kAvailable);

  EXPECT_TRUE(LockScreenNoteLauncher::CanAttemptLaunch());
  LockScreenNoteLauncher launcher;

  LaunchStatus launch_status = LaunchStatus::kUnknown;
  ASSERT_TRUE(
      launcher.Run(mojom::LockScreenNoteOrigin::kLockScreenButtonTap,
                   base::BindOnce(&HandleLaunchCallback, &launch_status)));

  // Verify a lock screen action was requested.
  tray_action->FlushMojoForTesting();
  EXPECT_EQ(std::vector<mojom::LockScreenNoteOrigin>(
                {mojom::LockScreenNoteOrigin::kLockScreenButtonTap}),
            action_client.note_origins());

  // Move note action to launching state, and verify the launch callback is not
  // run.
  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kLaunching);
  EXPECT_EQ(LaunchStatus::kUnknown, launch_status);

  // Move note action to active state and verify that the launch callback is
  // run.
  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kAvailable);
  EXPECT_EQ(LaunchStatus::kFailure, launch_status);
}

TEST_F(LockScreenNoteLauncherTest, LaunchNotRequestedInUnavailableStates) {
  TrayAction* tray_action = Shell::Get()->tray_action();
  TestTrayActionClient action_client;
  tray_action->SetClient(action_client.CreateRemoteAndBind(),
                         mojom::TrayActionState::kLaunching);

  EXPECT_FALSE(LockScreenNoteLauncher::CanAttemptLaunch());

  LockScreenNoteLauncher launcher;

  // Launch should not be requested if a lock screen note action is already
  // being launched.
  LaunchStatus launch_status = LaunchStatus::kUnknown;
  ASSERT_FALSE(
      launcher.Run(mojom::LockScreenNoteOrigin::kLockScreenButtonTap,
                   base::BindOnce(&HandleLaunchCallback, &launch_status)));

  // Verify a lock screen action was not requested.
  tray_action->FlushMojoForTesting();
  EXPECT_TRUE(action_client.note_origins().empty());

  // Move note action to active state and verify that the launch callback is
  // not run.
  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kActive);
  EXPECT_EQ(LaunchStatus::kUnknown, launch_status);

  // Launch request should fail if lock screen note is in active state.
  EXPECT_FALSE(LockScreenNoteLauncher::CanAttemptLaunch());
  ASSERT_FALSE(
      launcher.Run(mojom::LockScreenNoteOrigin::kLockScreenButtonTap,
                   base::BindOnce(&HandleLaunchCallback, &launch_status)));
  tray_action->FlushMojoForTesting();
  EXPECT_TRUE(action_client.note_origins().empty());

  // Move note action to not available state and verify that the launch callback
  // is not run.
  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kNotAvailable);
  EXPECT_EQ(LaunchStatus::kUnknown, launch_status);

  // Launch request should fail if lock screen note is in unavailable state.
  EXPECT_FALSE(LockScreenNoteLauncher::CanAttemptLaunch());
  ASSERT_FALSE(
      launcher.Run(mojom::LockScreenNoteOrigin::kLockScreenButtonTap,
                   base::BindOnce(&HandleLaunchCallback, &launch_status)));
  tray_action->FlushMojoForTesting();
  EXPECT_TRUE(action_client.note_origins().empty());

  // Move note action to available state and verify that the launch callback is
  // not run.
  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kAvailable);
  EXPECT_EQ(LaunchStatus::kUnknown, launch_status);
}

}  // namespace ash
