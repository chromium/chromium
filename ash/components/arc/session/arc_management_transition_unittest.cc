// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "ash/components/arc/session/arc_management_transition.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

std::string ConvertToString(ArcManagementTransition transition) {
  std::stringstream ss;
  ss << transition;
  return ss.str();
}

// Tests "<<" operator for ArcManagementTransition type.
TEST(ArcManagementTransitionTest, EnumTypeStreamOutput) {
  EXPECT_EQ(ConvertToString(ArcManagementTransition::NO_TRANSITION),
            "NO_TRANSITION");
  EXPECT_EQ(ConvertToString(ArcManagementTransition::CHILD_TO_REGULAR),
            "CHILD_TO_REGULAR");
  EXPECT_EQ(ConvertToString(ArcManagementTransition::REGULAR_TO_CHILD),
            "REGULAR_TO_CHILD");
  EXPECT_EQ(ConvertToString(ArcManagementTransition::UNMANAGED_TO_MANAGED),
            "UNMANAGED_TO_MANAGED");
}

}  // namespace
}  // namespace arc
