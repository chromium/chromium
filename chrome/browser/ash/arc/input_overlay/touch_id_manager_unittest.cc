// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/touch_id_manager.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace arc::input_overlay {

class TouchIdManagerTest : public testing::Test {
 protected:
  TouchIdManagerTest() = default;
  bool IsTouchIdUsed(int touch_id) {
    if (touch_id == 0 && TouchIdManager::GetInstance()->touch_ids_ == 1) {
      return true;
    }
    return TouchIdManager::GetInstance()->touch_ids_ & (1 << touch_id);
  }

  bool IsTouchIDsEqual(int val) {
    return TouchIdManager::GetInstance()->touch_ids_ == val;
  }
};

TEST_F(TouchIdManagerTest, TestIdManage) {
  EXPECT_FALSE(IsTouchIdUsed(0));
  int id = TouchIdManager::GetInstance()->ObtainTouchID();
  EXPECT_EQ(0, id);
  EXPECT_TRUE(IsTouchIdUsed(id));

  EXPECT_FALSE(IsTouchIdUsed(1));
  id = TouchIdManager::GetInstance()->ObtainTouchID();
  EXPECT_EQ(1, id);
  EXPECT_TRUE(IsTouchIdUsed(id));

  TouchIdManager::GetInstance()->ReleaseTouchID(0);
  EXPECT_FALSE(IsTouchIdUsed(0));

  id = TouchIdManager::GetInstance()->ObtainTouchID();
  EXPECT_EQ(0, id);
  EXPECT_TRUE(IsTouchIdUsed(id));

  id = TouchIdManager::GetInstance()->ObtainTouchID();
  EXPECT_EQ(2, id);
  EXPECT_TRUE(IsTouchIdUsed(id));

  id = TouchIdManager::GetInstance()->ObtainTouchID();
  EXPECT_EQ(3, id);
  EXPECT_TRUE(IsTouchIdUsed(id));

  id = TouchIdManager::GetInstance()->ObtainTouchID();
  EXPECT_EQ(4, id);
  EXPECT_TRUE(IsTouchIdUsed(id));
  EXPECT_TRUE(IsTouchIDsEqual(31));

  TouchIdManager::GetInstance()->ReleaseTouchID(1);
  EXPECT_FALSE(IsTouchIdUsed(1));
  TouchIdManager::GetInstance()->ReleaseTouchID(0);
  EXPECT_FALSE(IsTouchIdUsed(0));
  TouchIdManager::GetInstance()->ReleaseTouchID(2);
  EXPECT_FALSE(IsTouchIdUsed(2));
  TouchIdManager::GetInstance()->ReleaseTouchID(3);
  EXPECT_FALSE(IsTouchIdUsed(3));
  TouchIdManager::GetInstance()->ReleaseTouchID(4);
  EXPECT_FALSE(IsTouchIdUsed(4));
  EXPECT_TRUE(IsTouchIDsEqual(0));
}

}  // namespace arc::input_overlay
