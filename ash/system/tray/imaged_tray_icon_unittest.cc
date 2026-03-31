// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/imaged_tray_icon.h"

#include <string>
#include <utility>

#include "ash/constants/tray_background_view_catalog.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_container.h"
#include "ash/test/ash_test_base.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"

namespace ash {

class ImagedTrayIconTest : public AshTestBase {
 public:
  ImagedTrayIconTest() = default;

  ImagedTrayIconTest(const ImagedTrayIconTest&) = delete;
  ImagedTrayIconTest& operator=(const ImagedTrayIconTest&) = delete;

  ~ImagedTrayIconTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Create a dummy image model.
    image_model_ = ui::ImageModel::FromImageSkia(
        gfx::test::CreateImageSkia(/*width=*/20, /*height=*/20));
  }

 protected:
  ui::ImageModel image_model_;
};

TEST_F(ImagedTrayIconTest, InitialState) {
  const std::u16string tooltip = u"Tooltip";
  const std::u16string accessibility_name = u"Accessibility Name";

  auto tray_icon = std::make_unique<ImagedTrayIcon>(
      GetPrimaryShelf(), image_model_, tooltip, accessibility_name,
      TrayBackgroundViewCatalogName::kTestCatalogName);

  EXPECT_EQ(tray_icon->image_view()->GetTooltipText(), tooltip);
  EXPECT_EQ(tray_icon->GetViewAccessibility().GetCachedName(),
            accessibility_name);

  // Verify image.
  EXPECT_FALSE(tray_icon->image_view()->GetImageModel().IsEmpty());
}

TEST_F(ImagedTrayIconTest, SetTooltipAndAccessibilityName) {
  auto tray_icon = std::make_unique<ImagedTrayIcon>(
      GetPrimaryShelf(), image_model_, u"Initial Tooltip",
      u"Initial Accessibility Name",
      TrayBackgroundViewCatalogName::kTestCatalogName);

  const std::u16string new_tooltip = u"New Tooltip";
  tray_icon->SetTooltip(new_tooltip);
  EXPECT_EQ(tray_icon->image_view()->GetTooltipText(), new_tooltip);

  const std::u16string new_accessibility_name = u"New Accessibility Name";
  tray_icon->SetAccessibilityName(new_accessibility_name);
  EXPECT_EQ(tray_icon->GetViewAccessibility().GetCachedName(),
            new_accessibility_name);
}

TEST_F(ImagedTrayIconTest, SessionVisibility) {
  auto tray_icon = std::make_unique<ImagedTrayIcon>(
      GetPrimaryShelf(), image_model_, u"Tooltip", u"Accessibility Name",
      TrayBackgroundViewCatalogName::kTestCatalogName);

  // Set visibility callback that shows only in ACTIVE state.
  tray_icon->set_icon_visibility_callback(
      base::BindRepeating([](session_manager::SessionState state) {
        return state == session_manager::SessionState::ACTIVE;
      }));

  // Initial state is ACTIVE in AshTestBase.
  EXPECT_TRUE(tray_icon->visible_preferred());

  GetSessionControllerClient()->LockScreen();
  EXPECT_FALSE(tray_icon->visible_preferred());

  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(tray_icon->visible_preferred());
}

TEST_F(ImagedTrayIconTest, HandleLocaleChange) {
  const int tooltip_id = IDS_ASH_SHELF_ACCESSIBLE_NAME;
  const int accessibility_id =
      IDS_ASH_STATUS_TRAY_NOTIFICATIONS_ICONS_ACCESSIBLE_NAME;

  auto tray_icon = std::make_unique<ImagedTrayIcon>(
      GetPrimaryShelf(), image_model_, tooltip_id, accessibility_id,
      TrayBackgroundViewCatalogName::kTestCatalogName);

  // Trigger locale change.
  tray_icon->HandleLocaleChange();

  // The content should be updated (though in tests it might be the same string
  // if locale didn't actually change, but it verifies the code path).
  EXPECT_EQ(tray_icon->image_view()->GetTooltipText(),
            l10n_util::GetStringUTF16(tooltip_id));
  EXPECT_EQ(tray_icon->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(accessibility_id));
}

}  // namespace ash
