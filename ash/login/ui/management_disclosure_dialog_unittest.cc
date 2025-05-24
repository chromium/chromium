// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/management_disclosure_dialog.h"

#include <string>

#include "ash/login/ui/bottom_status_indicator.h"
#include "ash/login/ui/fake_login_detachable_base_model.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/test/views_test_utils.h"

namespace ash {

class ManagementDisclosureDialogTest : public LoginTestBase {
 public:
  ManagementDisclosureDialogTest() = default;

  ManagementDisclosureDialogTest(ManagementDisclosureDialogTest&) = delete;
  ManagementDisclosureDialogTest& operator=(ManagementDisclosureDialogTest&) =
      delete;
  ~ManagementDisclosureDialogTest() override = default;

  void SetUp() override { LoginTestBase::SetUp(); }
};

TEST_F(ManagementDisclosureDialogTest, CheckStaticTextTest) {
  // If the device is enrolled, bottom_status_indicator should be visible.
  Shell::Get()->system_tray_model()->SetDeviceEnterpriseInfo(
      DeviceEnterpriseInfo{"BestCompanyEver", ManagementDeviceMode::kNone});

  auto* contents = new LockContentsView(
      LockScreen::ScreenType::kLock, DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);

  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LockContentsViewTestApi test_api(contents);

  contents->ShowManagementDisclosureDialog();
  auto dialog = test_api.management_disclosure_dialog();
  ASSERT_TRUE(dialog);

  EXPECT_FALSE(dialog->ShouldShowWindowTitle());
  EXPECT_EQ(ui::mojom::ModalType::kSystem, dialog->GetModalType());

  // Buttons.
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
            dialog->GetOkButton()->GetText());
  EXPECT_EQ(static_cast<int>(ui::mojom::DialogButton::kOk), dialog->buttons());
  EXPECT_FALSE(dialog->ShouldShowCloseButton());

  // Labels.
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_PROXY_SERVER_PRIVACY_DISCLOSURE),
      static_cast<views::Label*>(
          dialog->GetViewByID(IDS_MANAGEMENT_PROXY_SERVER_PRIVACY_DISCLOSURE))
          ->GetText());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MANAGEMENT_DEVICE_CONFIGURATION),
            static_cast<views::Label*>(
                dialog->GetViewByID(IDS_MANAGEMENT_DEVICE_CONFIGURATION))
                ->GetText());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MANAGEMENT_OPEN_CHROME_MANAGEMENT),
            static_cast<views::Label*>(
                dialog->GetViewByID(IDS_MANAGEMENT_OPEN_CHROME_MANAGEMENT))
                ->GetText());
}

}  // namespace ash
