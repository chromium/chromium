// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>

#include "ash/auth/views/auth_header_view.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

const std::string kUserEmail("user1@gmail.com");

const std::u16string kTitle(u"Auth header view pixeltest title");
const std::u16string kErrorTitle(u"Auth header view pixeltest error");

const std::u16string kDescription(u"Auth header view pixeltest description");

class AuthHeaderPixelTest : public AshTestBase {
 public:
  AuthHeaderPixelTest() = default;
  AuthHeaderPixelTest(const AuthHeaderPixelTest&) = delete;
  AuthHeaderPixelTest& operator=(const AuthHeaderPixelTest&) = delete;
  ~AuthHeaderPixelTest() override = default;

 protected:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    UpdateDisplay("600x800");

    widget_ = CreateFramelessTestWidget();

    AccountId account_id = AccountId::FromUserEmail(kUserEmail);
    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    fake_user_manager->AddUser(account_id);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    std::unique_ptr<AuthHeaderView> header_view =
        std::make_unique<AuthHeaderView>(account_id, kTitle, kDescription);

    header_view->SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSystemBaseElevated, 0));

    widget_->SetSize(header_view->GetPreferredSize());
    widget_->Show();

    header_view_ = widget_->SetContentsView(std::move(header_view));

    auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
    dark_light_mode_controller->SetAutoScheduleEnabled(false);
    // Test Base should setup the dark mode.
    EXPECT_TRUE(dark_light_mode_controller->IsDarkModeEnabled());
  }

  void TearDown() override {
    header_view_ = nullptr;
    widget_.reset();
    AshTestBase::TearDown();
  }

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<AuthHeaderView> header_view_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

// Verify the header component look like in DayMode
TEST_F(AuthHeaderPixelTest, DayMode) {
  DarkLightModeControllerImpl::Get()->SetDarkModeEnabledForTest(false);
  //  Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "DayMode", /*revision_number=*/1, header_view_));
  // Verify the error.
  header_view_->SetErrorTitle(kErrorTitle);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "Error", /*revision_number=*/1, header_view_));
  // Verify the restore
  header_view_->RestoreTitle();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "Restore", /*revision_number=*/1, header_view_));
}

}  // namespace
}  // namespace ash
