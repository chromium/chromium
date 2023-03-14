// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_upgrade_params.h"

#include "ash/components/arc/arc_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

TEST(ArcUpgradeParamsTest, Constructor_WithTtsCacheDisableSwitch) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kArcDisableTtsCache);
  UpgradeParams upgradeParams;
  EXPECT_TRUE(upgradeParams.skip_tts_cache);
}

TEST(ArcUpgradeParamsTest, Constructor_DefaultTtsState) {
  UpgradeParams upgradeParams;
  EXPECT_FALSE(upgradeParams.skip_tts_cache);
}

}  // namespace
}  // namespace arc
