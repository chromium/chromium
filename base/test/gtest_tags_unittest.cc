// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_tags.h"

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(GtestTagsTest, AddInvalidName) {
  EXPECT_DCHECK_DEATH(AddTagToTestResult("", "value"));
}

TEST(GtestTagsTest, AddValidTag) {
  AddTagToTestResult("name", "value");
}

}  // namespace base
