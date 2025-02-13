// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hotspot/hotspot_icon.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/test/ash_test_base.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash::hotspot_icon {

using hotspot_config::mojom::HotspotState;

class HotspotIconTest : public AshTestBase {
 public:
  HotspotIconTest()
      : AshTestBase(std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {}
  ~HotspotIconTest() override = default;
};

TEST_F(HotspotIconTest, HotspotEnabledIcon) {
  EXPECT_EQ(&kHotspotOnIcon,
            &hotspot_icon::GetIconForHotspot(HotspotState::kEnabled));
}

TEST_F(HotspotIconTest, HotspotDisabledIcon) {
  EXPECT_EQ(&kHotspotOffIcon,
            &hotspot_icon::GetIconForHotspot(HotspotState::kDisabled));
}

TEST_F(HotspotIconTest, HotspotEnablingIcon) {
  EXPECT_EQ(&kHotspotDotIcon,
            &hotspot_icon::GetIconForHotspot(HotspotState::kEnabling));
  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(&kHotspotOneArcIcon,
            &hotspot_icon::GetIconForHotspot(HotspotState::kEnabling));
  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(&kHotspotOnIcon,
            &hotspot_icon::GetIconForHotspot(HotspotState::kEnabling));
}

TEST_F(HotspotIconTest, HotspotDisablingIcon) {
  EXPECT_EQ(&kHotspotOffIcon,
            &hotspot_icon::GetIconForHotspot(HotspotState::kDisabling));
}

}  // namespace ash::hotspot_icon
