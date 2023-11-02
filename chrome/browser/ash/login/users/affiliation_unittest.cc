// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/affiliation.h"

#include <set>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(AffiliationTest, HaveCommonElementEmptySet) {
  // Empty sets don't have common elements.
  EXPECT_FALSE(HaveCommonElement(AffiliationIDSet(), AffiliationIDSet()));

  AffiliationIDSet not_empty_set;
  not_empty_set.insert("a");

  // Only first set is empty. No common elements and no crash.
  EXPECT_FALSE(HaveCommonElement(AffiliationIDSet(), not_empty_set));
  // Now the second set is empty.
  EXPECT_FALSE(HaveCommonElement(not_empty_set, AffiliationIDSet()));
}

TEST(AffiliationTest, HaveCommonElementNoOverlap) {
  AffiliationIDSet set1;
  AffiliationIDSet set2;

  // No common elements.
  set1.insert("a");
  set1.insert("b");
  set1.insert("c");

  set2.insert("d");
  set2.insert("e");
  set2.insert("f");
  EXPECT_FALSE(HaveCommonElement(set1, set2));
}

TEST(AffiliationTest, HaveCommonElementFirstAndLastIsCommon) {
  AffiliationIDSet set1;
  AffiliationIDSet set2;

  // The common element is last in set1 and first in set2.
  set1.insert("a");
  set1.insert("b");
  set1.insert("c");
  set1.insert("d");  // String "d" is common.

  set2.insert("d");
  set2.insert("e");
  set2.insert("f");

  EXPECT_TRUE(HaveCommonElement(set1, set2));
  EXPECT_TRUE(HaveCommonElement(set2, set1));
}

TEST(AffiliationTest, HaveCommonElementCommonInTheMiddle) {
  AffiliationIDSet set1;
  AffiliationIDSet set2;

  // The common element is in the middle of the two sets.
  set1.insert("b");
  set1.insert("f");  // String "f" is common.
  set1.insert("k");

  set2.insert("c");
  set2.insert("f");
  set2.insert("j");

  EXPECT_TRUE(HaveCommonElement(set1, set2));
  EXPECT_TRUE(HaveCommonElement(set2, set1));
}

TEST(AffiliationTest, Generic) {
  AffiliationIDSet user_ids;    // User affiliation IDs.
  AffiliationIDSet device_ids;  // Device affiliation IDs.

  // Empty affiliation IDs.
  EXPECT_FALSE(IsUserAffiliated(user_ids, device_ids, "user@managed.com"));

  user_ids.insert("aaaa");  // Only user affiliation IDs present.
  EXPECT_FALSE(IsUserAffiliated(user_ids, device_ids, "user@managed.com"));

  device_ids.insert("bbbb");  // Device and user IDs do not overlap.
  EXPECT_FALSE(IsUserAffiliated(user_ids, device_ids, "user@managed.com"));

  user_ids.insert("cccc");  // Device and user IDs do overlap.
  device_ids.insert("cccc");
  EXPECT_TRUE(IsUserAffiliated(user_ids, device_ids, "user@managed.com"));

  // Invalid email overrides match of affiliation IDs.
  EXPECT_FALSE(IsUserAffiliated(user_ids, device_ids, ""));
  EXPECT_FALSE(IsUserAffiliated(user_ids, device_ids, "user"));
}

}  // namespace ash
