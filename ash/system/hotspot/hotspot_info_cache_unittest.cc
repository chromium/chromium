// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hotspot/hotspot_info_cache.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/run_loop.h"
#include "chromeos/ash/services/hotspot_config/public/cpp/cros_hotspot_config_test_helper.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"

namespace ash {

using hotspot_config::mojom::HotspotInfo;
using hotspot_config::mojom::HotspotState;

class HotspotInfoCacheTest : public AshTestBase {
 public:
  HotspotInfoCacheTest() = default;
  ~HotspotInfoCacheTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Spin the runloop to have HotspotInfoCache finish querying the hotspot
    // info.
    base::RunLoop().RunUntilIdle();
  }

  void LogIn() { SimulateUserLogin("user1@test.com"); }

  void LogOut() { ClearLogin(); }
};

TEST_F(HotspotInfoCacheTest, HotspotInfo) {
  EXPECT_EQ(hotspot_config::mojom::HotspotState::kDisabled,
            Shell::Get()->hotspot_info_cache()->GetHotspotInfo()->state);

  auto hotspot_info = HotspotInfo::New();
  hotspot_info->state = HotspotState::kEnabled;
  ash_test_helper()->cros_hotspot_config_test_helper()->SetFakeHotspotInfo(
      std::move(hotspot_info));
  // Spin the runloop to observe the hotspot info change.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hotspot_config::mojom::HotspotState::kEnabled,
            Shell::Get()->hotspot_info_cache()->GetHotspotInfo()->state);
}

TEST_F(HotspotInfoCacheTest, HasHotspotUsedBefore) {
  EXPECT_FALSE(Shell::Get()->hotspot_info_cache()->HasHotspotUsedBefore());

  LogIn();
  EXPECT_FALSE(Shell::Get()->hotspot_info_cache()->HasHotspotUsedBefore());
  auto hotspot_info = HotspotInfo::New();
  hotspot_info->state = HotspotState::kEnabled;
  ash_test_helper()->cros_hotspot_config_test_helper()->SetFakeHotspotInfo(
      mojo::Clone(hotspot_info));
  // Spin the runloop to observe the hotspot info change.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Shell::Get()->hotspot_info_cache()->HasHotspotUsedBefore());

  hotspot_info->state = HotspotState::kDisabled;
  ash_test_helper()->cros_hotspot_config_test_helper()->SetFakeHotspotInfo(
      mojo::Clone(hotspot_info));
  // Spin the runloop to observe the hotspot info change.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Shell::Get()->hotspot_info_cache()->HasHotspotUsedBefore());

  LogOut();
  EXPECT_FALSE(Shell::Get()->hotspot_info_cache()->HasHotspotUsedBefore());
}

}  // namespace ash
