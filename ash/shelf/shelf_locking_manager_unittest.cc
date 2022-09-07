// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_locking_manager.h"

#include "ash/shelf/shelf.h"
#include "ash/test/ash_test_base.h"

namespace ash {
namespace {

// Tests the shelf behavior when the screen or session is locked.
class ShelfLockingManagerTest : public AshTestBase {
 public:
  ShelfLockingManagerTest() = default;

  ShelfLockingManagerTest(const ShelfLockingManagerTest&) = delete;
  ShelfLockingManagerTest& operator=(const ShelfLockingManagerTest&) = delete;

  ShelfLockingManager* GetShelfLockingManager() {
    return GetPrimaryShelf()->GetShelfLockingManagerForTesting();
  }

  void SetScreenLocked(bool locked) {
    GetShelfLockingManager()->OnLockStateChanged(locked);
  }

  void SetSessionState(session_manager::SessionState state) {
    GetShelfLockingManager()->OnSessionStateChanged(state);
  }
};

// Makes sure shelf alignment is correct for lock screen.
TEST_F(ShelfLockingManagerTest, AlignmentLockedWhileScreenLocked) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());

  shelf->SetAlignment(ShelfAlignment::kLeft);
  EXPECT_EQ(ShelfAlignment::kLeft, shelf->alignment());

  SetScreenLocked(true);
  EXPECT_EQ(ShelfAlignment::kBottomLocked, shelf->alignment());
  SetScreenLocked(false);
  EXPECT_EQ(ShelfAlignment::kLeft, shelf->alignment());
}

// Makes sure shelf alignment is correct for login and add user screens.
TEST_F(ShelfLockingManagerTest, AlignmentLockedWhileSessionLocked) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());

  shelf->SetAlignment(ShelfAlignment::kRight);
  EXPECT_EQ(ShelfAlignment::kRight, shelf->alignment());

  SetSessionState(session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(ShelfAlignment::kBottomLocked, shelf->alignment());
  SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(ShelfAlignment::kRight, shelf->alignment());

  SetSessionState(session_manager::SessionState::LOGIN_SECONDARY);
  EXPECT_EQ(ShelfAlignment::kBottomLocked, shelf->alignment());
  SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(ShelfAlignment::kRight, shelf->alignment());
}

// Makes sure shelf alignment changes are stored, not set, while locked.
TEST_F(ShelfLockingManagerTest, AlignmentChangesDeferredWhileLocked) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());

  SetScreenLocked(true);
  EXPECT_EQ(ShelfAlignment::kBottomLocked, shelf->alignment());
  shelf->SetAlignment(ShelfAlignment::kRight);
  EXPECT_EQ(ShelfAlignment::kBottomLocked, shelf->alignment());
  SetScreenLocked(false);
  EXPECT_EQ(ShelfAlignment::kRight, shelf->alignment());
}

}  // namespace
}  // namespace ash
