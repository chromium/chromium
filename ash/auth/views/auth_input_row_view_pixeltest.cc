// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "ash/auth/views/auth_input_row_view.h"
#include "ash/auth/views/test_support/mock_auth_input_row_view_observer.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/files/file_path.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr std::u16string kPassword = u"password";

class InputRowWithPasswordPixelTest : public AshTestBase {
 public:
  InputRowWithPasswordPixelTest() = default;
  InputRowWithPasswordPixelTest(const InputRowWithPasswordPixelTest&) = delete;
  InputRowWithPasswordPixelTest& operator=(
      const InputRowWithPasswordPixelTest&) = delete;
  ~InputRowWithPasswordPixelTest() override = default;

  void SetInputRowToFocus() { auth_input_->RequestFocus(); }

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

    std::unique_ptr<views::View> container_view =
        std::make_unique<views::View>();

    auto* layout =
        container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);

    container_view->SetPreferredSize(gfx::Size({500, 400}));

    container_view->SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSystemBaseElevated, 0));

    auth_input_ =
        container_view->AddChildView(std::make_unique<ash::AuthInputRowView>(
            ash::AuthInputRowView::AuthType::kPassword));

    widget_->SetBounds({500, 400});
    widget_->Show();

    container_view_ = widget_->SetContentsView(std::move(container_view));
    test_api_ = std::make_unique<AuthInputRowView::TestApi>(auth_input_);
    // Initialize the textfield with some text
    auth_input_->RequestFocus();
    CHECK(test_api_->GetTextfield()->HasFocus());
    for (const char16_t c : kPassword) {
      PressAndReleaseKey(ui::DomCodeToUsLayoutNonLocatedKeyboardCode(
          ui::UsLayoutDomKeyToDomCode(ui::DomKey::FromCharacter(c))));
    }
    CHECK_EQ(test_api_->GetTextfield()->GetText(), kPassword);

    // Add Observer
    mock_observer_ = std::make_unique<MockAuthInputRowViewObserver>();
    auth_input_->AddObserver(mock_observer_.get());

    auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
    dark_light_mode_controller->SetAutoScheduleEnabled(false);
    // Test Base should setup the dark mode.
    EXPECT_TRUE(dark_light_mode_controller->IsDarkModeEnabled());

    // Hide cursor to avoid flakiness
    views::TextfieldTestApi(test_api_->GetTextfield())
        .SetCursorLayerOpacity(0.f);
  }

  void TearDown() override {
    test_api_.reset();
    auth_input_->RemoveObserver(mock_observer_.get());
    mock_observer_.reset();
    container_view_ = nullptr;
    auth_input_ = nullptr;
    widget_.reset();
    AshTestBase::TearDown();
  }

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<MockAuthInputRowViewObserver> mock_observer_;
  std::unique_ptr<AuthInputRowView::TestApi> test_api_;
  raw_ptr<AuthInputRowView> auth_input_ = nullptr;
  raw_ptr<views::View> container_view_ = nullptr;
};

// Verify the input row component look like in DayMode
TEST_F(InputRowWithPasswordPixelTest, DayMode) {
  DarkLightModeControllerImpl::Get()->SetDarkModeEnabledForTest(false);
  //  Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "DayMode", /*revision_number=*/1, container_view_));
}

TEST_F(InputRowWithPasswordPixelTest, VisibleText) {
  DarkLightModeControllerImpl::Get()->SetDarkModeEnabledForTest(false);
  LeftClickOn(test_api_->GetDisplayTextButton());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "VisibleText", /*revision_number=*/1, container_view_));
}

TEST_F(InputRowWithPasswordPixelTest, FocusDisplayTextButton) {
  auth_input_->GetFocusManager()->SetFocusedView(
      test_api_->GetDisplayTextButton());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "FocusDisplayTextButton", /*revision_number=*/1, container_view_));
}

TEST_F(InputRowWithPasswordPixelTest, FocusSubmitButton) {
  DarkLightModeControllerImpl::Get()->SetDarkModeEnabledForTest(false);
  auth_input_->GetFocusManager()->SetFocusedView(test_api_->GetSubmitButton());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "FocusSubmitButton", /*revision_number=*/1, container_view_));
}

}  // namespace
}  // namespace ash
