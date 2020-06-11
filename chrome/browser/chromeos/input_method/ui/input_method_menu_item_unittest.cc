// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ui/input_method_menu_item.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace ime {

TEST(InputMethodMenuItemTest, TestOperatorEqual) {
  InputMethodMenuItem empty;
  InputMethodMenuItem reference("key", "label", true, true);

  InputMethodMenuItem p1("X", "label", true, true);
  InputMethodMenuItem p2("key", "X", true, true);
  InputMethodMenuItem p3("key", "label", false, true);
  InputMethodMenuItem p4("key", "label", true, false);

  EXPECT_EQ(empty, empty);
  EXPECT_EQ(reference, reference);
  EXPECT_NE(reference, empty);
  EXPECT_NE(reference, p1);
  EXPECT_NE(reference, p2);
  EXPECT_NE(reference, p3);
  EXPECT_NE(reference, p4);
}

}  // namespace ime
}  // namespace ui
