// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/fake_login_detachable_base_model.h"
#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_contents_view_test_api.h"
#include "ash/login/ui/login_detachable_base_model.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/management_disclosure_dialog.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

class ManagementDisclosureDialogTestBase : public LoginTestBase {
 public:
  ManagementDisclosureDialogTestBase(
      const ManagementDisclosureDialogTestBase&) = delete;
  ManagementDisclosureDialogTestBase& operator=(
      const ManagementDisclosureDialogTestBase&) = delete;

 protected:
  ManagementDisclosureDialogTestBase() = default;
  ~ManagementDisclosureDialogTestBase() override = default;

  // LoginTestBase:
  void SetUp() override { LoginTestBase::SetUp(); }

  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }
};

class ManagementDisclosureDialogPixeltest
    : public ManagementDisclosureDialogTestBase {
 public:
  ManagementDisclosureDialogPixeltest(
      const ManagementDisclosureDialogPixeltest&) = delete;
  ManagementDisclosureDialogPixeltest& operator=(
      const ManagementDisclosureDialogPixeltest&) = delete;

 protected:
  ManagementDisclosureDialogPixeltest() = default;
  ~ManagementDisclosureDialogPixeltest() override = default;

  // LoginTestBase:
  void SetUp() override { ManagementDisclosureDialogTestBase::SetUp(); }
};

TEST_F(ManagementDisclosureDialogPixeltest, CheckManagementDisclosure) {
  // If the device is enrolled, bottom_status_indicator should be visible.
  Shell::Get()->system_tray_model()->SetDeviceEnterpriseInfo(
      DeviceEnterpriseInfo{"BestCompanyEver", ManagementDeviceMode::kNone});

  auto* view_ = new LockContentsView(
      LockScreen::ScreenType::kLock, DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);

  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(view_);
  LockContentsViewTestApi test_api(view_);

  view_->ShowManagementDisclosureDialog();
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

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "ManagementDisclosureDialogOpen", /*revision_number=*/0, view_));
}
}  // namespace ash
