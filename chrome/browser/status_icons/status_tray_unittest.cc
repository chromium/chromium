// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/status_icons/status_tray.h"

#include "base/compiler_specific.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace {

class MockStatusIcon : public StatusIcon {
  void SetImage(const gfx::ImageSkia& image) override {}
  void SetToolTip(const std::u16string& tool_tip) override {}
  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const std::u16string& title,
                      const std::u16string& contents,
                      const message_center::NotifierId& notifier_id) override {}
  void UpdatePlatformContextMenu(StatusIconMenuModel* menu) override {}
};

class TestStatusTray : public StatusTray {
 public:
  std::unique_ptr<StatusIcon> CreatePlatformStatusIcon(
      StatusIconType type,
      const gfx::ImageSkia& image,
      const std::u16string& tool_tip) override {
    return std::make_unique<MockStatusIcon>();
  }

  const StatusIcons& GetStatusIconsForTest() const { return status_icons(); }
};

StatusIcon* CreateStatusIcon(StatusTray* tray) {
  // Just create a dummy icon image; the actual image is irrelevant.
  return tray->CreateStatusIcon(StatusTray::OTHER_ICON,
                                gfx::test::CreateImageSkia(16, 16),
                                std::u16string());
}

}  // namespace

TEST(StatusTrayTest, Create) {
  // Check for creation and leaks.
  TestStatusTray tray;
  CreateStatusIcon(&tray);
  EXPECT_EQ(1U, tray.GetStatusIconsForTest().size());
}

// Make sure that removing an icon removes it from the list.
TEST(StatusTrayTest, CreateRemove) {
  TestStatusTray tray;
  StatusIcon* icon = CreateStatusIcon(&tray);
  EXPECT_EQ(1U, tray.GetStatusIconsForTest().size());
  tray.RemoveStatusIcon(icon);
  EXPECT_EQ(0U, tray.GetStatusIconsForTest().size());
}
