// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_state_cache.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/run_loop.h"
#include "chromeos/ash/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-shared.h"

namespace ash {

using BluetoothStateCacheTest = AshTestBase;

TEST_F(BluetoothStateCacheTest, Basics) {
  auto* state_controller = ash_test_helper()
                               ->bluetooth_config_test_helper()
                               ->fake_adapter_state_controller();

  state_controller->SetSystemState(
      bluetooth_config::mojom::BluetoothSystemState::kDisabled);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(bluetooth_config::mojom::BluetoothSystemState::kDisabled,
            Shell::Get()->bluetooth_state_cache()->system_state());

  state_controller->SetSystemState(
      bluetooth_config::mojom::BluetoothSystemState::kEnabled);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(bluetooth_config::mojom::BluetoothSystemState::kEnabled,
            Shell::Get()->bluetooth_state_cache()->system_state());
}

}  // namespace ash
