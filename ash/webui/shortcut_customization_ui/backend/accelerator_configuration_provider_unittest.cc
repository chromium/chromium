// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_configuration_provider.h"

#include <memory>
#include <vector>

#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/public/cpp/accelerator_configuration.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/mojom/accelerator_keys.mojom.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_suite.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {

namespace {

bool CompareAccelerators(const ash::AcceleratorData& expected_data,
                         const ash::AcceleratorInfo& actual_info) {
  ui::Accelerator expected_accel(expected_data.keycode,
                                 expected_data.modifiers);
  ash::AcceleratorInfo expected_info(
      actual_info.type, expected_accel,
      ash::KeycodeToKeyString(expected_data.keycode),
      /*locked=*/true);

  const bool type_equals = expected_info.type == actual_info.type;
  const bool accelerator_equals =
      expected_info.accelerator == actual_info.accelerator;
  const bool locked_equals = expected_info.locked == actual_info.locked;
  return type_equals && accelerator_equals && locked_equals;
}

void ExpectAllAcceleratorsEqual(
    const base::span<const ash::AcceleratorData>& expected,
    const std::vector<ash::AcceleratorInfo>& actual) {
  EXPECT_EQ(std::size(expected), actual.size());

  for (const auto& actual_info : actual) {
    bool found_match = false;
    for (const auto& expected_data : expected) {
      found_match = CompareAccelerators(expected_data, actual_info);
      if (found_match) {
        break;
      }
    }
    EXPECT_TRUE(found_match);
  }
}

void ExpectMojomAcceleratorsEqual(
    ash::mojom::AcceleratorSource source,
    const base::span<const ash::AcceleratorData>& expected,
    ash::shortcut_ui::AcceleratorConfigurationProvider::
        AcceleratorConfigurationMap actual_config) {
  // Flatten the map into a vector of `AcceleratorInfo`'s and verify it against
  // the expected data.
  std::vector<AcceleratorInfo> actual_infos;
  for (const auto& iter : actual_config[source]) {
    for (const auto& mojo_info : iter.second) {
      AcceleratorInfo accelerator(mojo_info->type, mojo_info->accelerator,
                                  mojo_info->key_display, mojo_info->locked);
      actual_infos.push_back(std::move(accelerator));
    }
  }
  ExpectAllAcceleratorsEqual(expected, actual_infos);
}

}  // namespace

namespace shortcut_ui {

class AcceleratorConfigurationProviderTest : public AshTestBase {
 public:
  AcceleratorConfigurationProviderTest() = default;
  ~AcceleratorConfigurationProviderTest() override = default;

  // AshTestBase:
  void SetUp() override {
    // ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);

    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();

    provider_ = std::make_unique<AcceleratorConfigurationProvider>();
  }

  void TearDown() override { AshTestBase::TearDown(); }

 protected:
  std::vector<AcceleratorInfo> GetAshAccelerators() {
    const std::map<AcceleratorActionId, std::vector<AcceleratorInfo>>&
        ash_accel_map = provider_->ash_accelerator_mapping_;
    std::vector<AcceleratorInfo> accelerators;
    for (const auto& iter : ash_accel_map) {
      for (const auto& accel : iter.second) {
        accelerators.push_back(accel);
      }
    }
    return accelerators;
  }

  void GetAshConfigAndExpectEquals(
      const base::span<const ash::AcceleratorData>& expected) {
    provider_->GetAccelerators(base::BindOnce(&ExpectMojomAcceleratorsEqual,
                                              mojom::AcceleratorSource::kAsh,
                                              expected));
  }

  std::unique_ptr<AcceleratorConfigurationProvider> provider_;
};

TEST_F(AcceleratorConfigurationProviderTest, BrowserIsMutable) {
  // Verify that requesting IsMutable state for Browser accelerators returns
  // false.
  provider_->IsMutable(ash::mojom::AcceleratorSource::kBrowser,
                       base::BindLambdaForTesting([&](bool is_mutable) {
                         // Browser accelerators are not mutable.
                         EXPECT_FALSE(is_mutable);
                       }));
  base::RunLoop().RunUntilIdle();
}

TEST_F(AcceleratorConfigurationProviderTest, AshIsMutable) {
  // Verify that requesting IsMutable state for Ash accelerators returns true.
  provider_->IsMutable(ash::mojom::AcceleratorSource::kAsh,
                       base::BindLambdaForTesting([&](bool is_mutable) {
                         // Ash accelerators are mutable.
                         EXPECT_TRUE(is_mutable);
                       }));
  base::RunLoop().RunUntilIdle();
}

TEST_F(AcceleratorConfigurationProviderTest, AshAcceleratorsUpdated) {
  const AcceleratorData test_data[] = {
      {/**trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/**trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
      {/**trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       SHOW_TASK_MANAGER},
  };
  Shell::Get()->ash_accelerator_configuration()->Initialize(test_data);
  base::RunLoop().RunUntilIdle();
  ExpectAllAcceleratorsEqual(test_data, GetAshAccelerators());

  // Initialize with a new set of accelerators.
  const AcceleratorData updated_test_data[] = {
      {/**trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN,
       TOGGLE_MIRROR_MODE},
      {/**trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_ALT_DOWN,
       SWAP_PRIMARY_DISPLAY},
      {/**trigger_on_press=*/true, ui::VKEY_MEDIA_LAUNCH_APP1,
       ui::EF_CONTROL_DOWN, TAKE_SCREENSHOT},
  };
  Shell::Get()->ash_accelerator_configuration()->Initialize(updated_test_data);
  base::RunLoop().RunUntilIdle();
  ExpectAllAcceleratorsEqual(updated_test_data, GetAshAccelerators());
}

TEST_F(AcceleratorConfigurationProviderTest, GetAcceleratorConfigAsh) {
  const AcceleratorData test_data[] = {
      {/**trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/**trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
      {/**trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       SHOW_TASK_MANAGER},
  };
  Shell::Get()->ash_accelerator_configuration()->Initialize(test_data);
  base::RunLoop().RunUntilIdle();
  ExpectAllAcceleratorsEqual(test_data, GetAshAccelerators());

  GetAshConfigAndExpectEquals(test_data);
  base::RunLoop().RunUntilIdle();
}

}  // namespace shortcut_ui

}  // namespace ash
