// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_instance_mode.h"

#include <optional>
#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

// Test that ArcInstanceMode can be logged.
TEST(ArcInstanceModeTest, TestLogging) {
  std::ostringstream mini;
  mini << ArcInstanceMode::MINI_INSTANCE;
  EXPECT_FALSE(mini.str().empty());

  std::ostringstream full;
  full << ArcInstanceMode::FULL_INSTANCE;
  EXPECT_FALSE(full.str().empty());
  EXPECT_NE(mini.str(), full.str());

  std::ostringstream invalid;
  invalid << static_cast<ArcInstanceMode>(-1);
  EXPECT_TRUE(invalid.str().empty()) << invalid.str();
}

// Test that std::optional<ArcInstanceMode> can be logged too.
TEST(ArcInstanceModeTest, TestLoggingWithOptional) {
  std::ostringstream nullopt;
  nullopt << std::optional<ArcInstanceMode>();
  EXPECT_FALSE(nullopt.str().empty());

  std::ostringstream non_nullopt;
  non_nullopt << std::optional<ArcInstanceMode>(ArcInstanceMode::MINI_INSTANCE);
  EXPECT_FALSE(non_nullopt.str().empty());
  EXPECT_NE(nullopt.str(), non_nullopt.str());

  std::ostringstream value;
  value << ArcInstanceMode::MINI_INSTANCE;
  EXPECT_FALSE(value.str().empty());
  EXPECT_EQ(non_nullopt.str(), value.str());
}

}  // namespace
}  // namespace arc
