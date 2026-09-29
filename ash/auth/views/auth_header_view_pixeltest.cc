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
#include "components/account_id/account_id_literal.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr AccountId::Literal kTestAccountId =
    AccountId::Literal::FromUserEmailGaiaId("user1@gmail.com",
                                            GaiaId::Literal("fake_gaia"));

constexpr char16_t kTitle[] = u"Auth header view pixeltest title";
constexpr char16_t kErrorTitle[] = u"Auth header view pixeltest error";
constexpr char16_t kDescription[] = u"Auth header view pixeltest description";

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
    user_session_test_environment_ =
        std::make_unique<ash::test::UserSessionTestEnvironment>(local_state());
    ASSERT_TRUE(user_session_test_environment_->AddRegularUser(kTestAccountId));

    AshTestBase::SetUp();
    UpdateDisplay("600x800");

    widget_ = CreateFramelessTestWidget();

    std::unique_ptr<AuthHeaderView> header_view =
        std::make_unique<AuthHeaderView>(kTestAccountId, kTitle, kDescription);

    header_view->SetBackground(views::CreateRoundedRectBackground(
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
    user_session_test_environment_.reset();
  }

  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<AuthHeaderView> header_view_ = nullptr;
};

// Verify the header component look like in DayMode
TEST_F(AuthHeaderPixelTest, DayMode) {
  DarkLightModeControllerImpl::Get()->SetDarkModeEnabledForTest(false);
  //  Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "DayMode", /*revision_number=*/2, header_view_));
  // Verify the error.
  header_view_->SetErrorTitle(kErrorTitle);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "Error", /*revision_number=*/2, header_view_));
  // Verify the restore
  header_view_->RestoreTitle();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "Restore", /*revision_number=*/2, header_view_));
}

}  // namespace
}  // namespace ash
