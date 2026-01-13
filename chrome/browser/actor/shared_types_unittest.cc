// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/shared_types.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

TEST(SharedTypesTest, PageTarget) {
  auto target1 = PageTarget(gfx::Point(9, 10));
  auto target2 =
      PageTarget(DomNode{.node_id = 222, .document_identifier = "foo"});
  EXPECT_EQ(DebugString(target1), "9,10");
  EXPECT_EQ(DebugString(target2), "DomNode[id=222 doc_id=foo]");

  std::ostringstream ss;
  ss << target1 << " -- " << target2;
  EXPECT_EQ(ss.str(), "9,10 -- DomNode[id=222 doc_id=foo]");
}

}  // namespace
}  // namespace actor
