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
  const gfx::VectorIcon& icon =
      hotspot_icon::GetIconForHotspot(HotspotState::kEnabled);
  EXPECT_STREQ(kHotspotOnIcon.name, icon.name);
}

TEST_F(HotspotIconTest, HotspotDisabledIcon) {
  const gfx::VectorIcon& icon =
      hotspot_icon::GetIconForHotspot(HotspotState::kDisabled);
  EXPECT_STREQ(kHotspotOffIcon.name, icon.name);
}

TEST_F(HotspotIconTest, HotspotEnablingIcon) {
  const gfx::VectorIcon& icon =
      hotspot_icon::GetIconForHotspot(HotspotState::kEnabling);
  EXPECT_STREQ(kHotspotDotIcon.name, icon.name);
  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_STREQ(kHotspotOneArcIcon.name,
               hotspot_icon::GetIconForHotspot(HotspotState::kEnabling).name);
  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_STREQ(kHotspotOnIcon.name,
               hotspot_icon::GetIconForHotspot(HotspotState::kEnabling).name);
}

TEST_F(HotspotIconTest, HotspotDisablingIcon) {
  const gfx::VectorIcon& icon =
      hotspot_icon::GetIconForHotspot(HotspotState::kDisabling);
  EXPECT_STREQ(kHotspotOffIcon.name, icon.name);
}

}  // namespace ash::hotspot_icon
