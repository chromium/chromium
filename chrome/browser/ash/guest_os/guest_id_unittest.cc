// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_id.h"

#include "base/values.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace guest_os {

using GuestIdTest = testing::Test;

TEST_F(GuestIdTest, GuestIdEquality) {
  auto container1 = GuestId{"test1", "test2"};
  auto container2 = GuestId{"test1", "test2"};
  auto container3 = GuestId{"test2", "test1"};

  ASSERT_TRUE(container1 == container2);
  ASSERT_FALSE(container1 == container3);
  ASSERT_FALSE(container2 == container3);
}

TEST_F(GuestIdTest, GuestIdFromDictValue) {
  base::Value dict(base::Value::Type::DICT);
  dict.SetStringKey(crostini::prefs::kVmKey, "foo");
  dict.SetStringKey(crostini::prefs::kContainerKey, "bar");
  EXPECT_TRUE(GuestId(dict) == GuestId("foo", "bar"));
}

TEST_F(GuestIdTest, GuestIdFromNonDictValue) {
  base::Value non_dict("not a dict value");
  EXPECT_TRUE(GuestId(non_dict) == GuestId("", ""));
}

}  // namespace guest_os
