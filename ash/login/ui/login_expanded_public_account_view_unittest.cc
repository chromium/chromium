// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_expanded_public_account_view.h"

#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/login/ui/public_account_warning_dialog.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/login_types.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind_helpers.h"
#include "base/ranges/algorithm.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Total width of the expanded view.
constexpr int kBubbleTotalWidthDp = 628;
// Total height of the expanded view.
constexpr int kBubbleTotalHeightDp = 324;

// Fake language and keyboard information.
constexpr char kEnglishLanguageCode[] = "language_code1";
constexpr char kEnglishLanguageName[] = "English";
constexpr char kFrenchLanguageCode[] = "language_code2";
constexpr char kFrenchLanguageName[] = "French";

constexpr char kKeyboardIdForItem1[] = "keyboard_id1";
constexpr char kKeyboardNameForItem1[] = "keyboard1";
constexpr char kKeyboardIdForItem2[] = "keyboard_id2";
constexpr char kKeyboardNameForItem2[] = "keyboard2";

class LoginExpandedPublicAccountViewTest
    : public LoginTestBase,
      public ::testing::WithParamInterface<const char*> {
 protected:
  LoginExpandedPublicAccountViewTest() = default;
  ~LoginExpandedPublicAccountViewTest() override = default;

  // LoginTestBase:
  void SetUp() override {
    LoginTestBase::SetUp();
    user_ = CreatePublicAccountUser("user@domain.com");
    SetupLanguageInfo();
    SetupKeyboardInfo();

    public_account_ = new LoginExpandedPublicAccountView(base::DoNothing());
    public_account_->UpdateForUser(user_);

    other_view_ = new views::View();

    container_ = new views::View();
    container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));
    container_->AddChildView(public_account_);
    container_->AddChildView(other_view_);
    SetWidget(CreateWidgetWithContent(container_));
  }

  // Add two fake language items, the first item is selected by default.
  void SetupLanguageInfo() {
    std::vector<LocaleItem> result;
    LocaleItem locale_item1;
    locale_item1.language_code = kEnglishLanguageCode;
    locale_item1.title = kEnglishLanguageName;

    LocaleItem locale_item2;
    locale_item2.language_code = kFrenchLanguageCode;
    locale_item2.title = kFrenchLanguageName;
    result.push_back(std::move(locale_item1));
    result.push_back(std::move(locale_item2));
    user_.public_account_info->available_locales = std::move(result);
    user_.public_account_info->default_locale = kEnglishLanguageCode;
  }

  // Add two fake keyboard items, the second item is selected by default.
  void SetupKeyboardInfo() {
    std::vector<InputMethodItem> result;
    InputMethodItem keyboard_item1;
    keyboard_item1.ime_id = kKeyboardIdForItem1;
    keyboard_item1.title = kKeyboardNameForItem1;

    InputMethodItem keyboard_item2;
    keyboard_item2.ime_id = kKeyboardIdForItem2;
    keyboard_item2.title = kKeyboardNameForItem2;
    keyboard_item2.selected = true;
    result.push_back(std::move(keyboard_item1));
    result.push_back(std::move(keyboard_item2));

    user_.public_account_info->keyboard_layouts = std::move(result);
  }

  void TapOnView(views::View* tap_target) {
    if (GetParam() == std::string("mouse")) {
      GetEventGenerator()->MoveMouseTo(
          tap_target->GetBoundsInScreen().CenterPoint());
      GetEventGenerator()->ClickLeftButton();
    } else {
      GetEventGenerator()->MoveTouch(
          tap_target->GetBoundsInScreen().CenterPoint());
      GetEventGenerator()->PressTouch();
      GetEventGenerator()->ReleaseTouch();
    }
  }

  LoginUserInfo user_;

  // Owned by test widget view hierarchy.
  views::View* container_ = nullptr;
  LoginExpandedPublicAccountView* public_account_ = nullptr;
  views::View* other_view_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginExpandedPublicAccountViewTest);
};

}  // namespace

// Verifies toggle advanced view will update the layout correctly.
TEST_P(LoginExpandedPublicAccountViewTest, ToggleAdvancedView) {
  public_account_->SizeToPreferredSize();
  EXPECT_EQ(public_account_->width(), kBubbleTotalWidthDp);
  EXPECT_EQ(public_account_->height(), kBubbleTotalHeightDp);

  LoginExpandedPublicAccountView::TestApi test_api(public_account_);
  EXPECT_FALSE(user_.public_account_info->show_advanced_view);
  EXPECT_FALSE(test_api.advanced_view()->GetVisible());

  // Toggle show_advanced_view.
  user_.public_account_info->show_advanced_view = true;
  public_account_->UpdateForUser(user_);

  // Advanced view is shown and the overall size does not change.
  EXPECT_TRUE(test_api.advanced_view()->GetVisible());
  EXPECT_EQ(public_account_->width(), kBubbleTotalWidthDp);
  EXPECT_EQ(public_account_->height(), kBubbleTotalHeightDp);

  // Click on the show advanced button.
  TapOnView(test_api.advanced_view_button());

  // Advanced view is hidden and the overall size does not change.
  EXPECT_FALSE(test_api.advanced_view()->GetVisible());
  EXPECT_EQ(public_account_->width(), kBubbleTotalWidthDp);
  EXPECT_EQ(public_account_->height(), kBubbleTotalHeightDp);
}

// Verifies warning dialog shows up correctly.
TEST_P(LoginExpandedPublicAccountViewTest, ShowWarningDialog) {
  LoginExpandedPublicAccountView::TestApi test_api(public_account_);
  EXPECT_EQ(test_api.warning_dialog(), nullptr);

  // Tap on the learn more link.
  const auto& children = test_api.learn_more_label()->children();
  const auto it = base::ranges::find(children, views::Link::kViewClassName,
                                     &views::View::GetClassName);
  DCHECK(it != children.cend());
  TapOnView(*it);
  ASSERT_NE(test_api.warning_dialog(), nullptr);
  EXPECT_TRUE(test_api.warning_dialog()->GetVisible());

  // When warning dialog is shown, tap outside of public account expanded view
  // should not hide it.
  TapOnView(other_view_);
  EXPECT_TRUE(public_account_->GetVisible());
  ASSERT_NE(test_api.warning_dialog(), nullptr);
  EXPECT_TRUE(test_api.warning_dialog()->GetVisible());

  // If the warning dialog is shown, escape key should close the waring dialog,
  // but not the public account view.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_ESCAPE, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(test_api.warning_dialog(), nullptr);
  EXPECT_TRUE(public_account_->GetVisible());

  // Press escape again should hide the public account expanded view.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_ESCAPE, 0);
  EXPECT_FALSE(public_account_->GetVisible());

  // Warning icon is shown only if full management disclosure flag is set.
  public_account_->SetShowFullManagementDisclosure(true);
  EXPECT_TRUE(test_api.monitoring_warning_icon()->GetVisible());
  public_account_->SetShowFullManagementDisclosure(false);
  EXPECT_FALSE(test_api.monitoring_warning_icon()->GetVisible());
}

// Verifies tap on submit button will try to launch public session.
TEST_P(LoginExpandedPublicAccountViewTest, LaunchPublicSession) {
  LoginExpandedPublicAccountView::TestApi test_api(public_account_);

  // Verify the language and keyboard information is populated correctly.
  std::string selected_language = test_api.selected_language_item().value;
  std::string selected_keyboard = test_api.selected_keyboard_item().value;
  EXPECT_EQ(selected_language, kEnglishLanguageCode);
  EXPECT_EQ(selected_keyboard, kKeyboardIdForItem2);

  // Expect LanuchPublicSession mojo call when the submit button is clicked.
  auto client = std::make_unique<MockLoginScreenClient>();
  EXPECT_CALL(*client,
              LaunchPublicSession(user_.basic_user_info.account_id,
                                  selected_language, selected_keyboard));

  // Click on the submit button.
  TapOnView(test_api.submit_button());
  base::RunLoop().RunUntilIdle();
}

// Verifies both language and keyboard menus shows up correctly.
TEST_P(LoginExpandedPublicAccountViewTest, ShowLanguageAndKeyboardMenu) {
  LoginExpandedPublicAccountView::TestApi test_api(public_account_);
  EXPECT_FALSE(user_.public_account_info->show_advanced_view);
  EXPECT_FALSE(test_api.advanced_view()->GetVisible());

  // Toggle show_advanced_view.
  user_.public_account_info->show_advanced_view = true;
  public_account_->UpdateForUser(user_);
  EXPECT_TRUE(test_api.advanced_view()->GetVisible());

  // Tap on language selection button should bring up the language menu.
  TapOnView(test_api.language_selection_button());
  EXPECT_TRUE(test_api.language_menu_view()->GetVisible());

  // First language item is selected, and selected item should have focus.
  EXPECT_EQ(test_api.selected_language_item().value, kEnglishLanguageCode);
  LoginMenuView::TestApi language_test_api(test_api.language_menu_view());
  ASSERT_EQ(2u, language_test_api.contents()->children().size());
  EXPECT_TRUE(language_test_api.contents()->children()[0]->HasFocus());

  // Select language item should close the language menu.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  EXPECT_FALSE(test_api.language_menu_view()->GetVisible());

  // Tap on keyboard selection button should bring up the keyboard menu.
  TapOnView(test_api.keyboard_selection_button());
  EXPECT_TRUE(test_api.keyboard_menu_view()->GetVisible());

  // Second keyboard item is selected, and selected item should have focus.
  EXPECT_EQ(test_api.selected_keyboard_item().value, kKeyboardIdForItem2);
  LoginMenuView::TestApi keyboard_test_api(test_api.keyboard_menu_view());
  ASSERT_EQ(2u, keyboard_test_api.contents()->children().size());
  EXPECT_TRUE(keyboard_test_api.contents()->children()[1]->HasFocus());

  // Select keyboard item should close the keyboard menu.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  EXPECT_FALSE(test_api.keyboard_menu_view()->GetVisible());
}

TEST_P(LoginExpandedPublicAccountViewTest, ChangeMenuSelection) {
  LoginExpandedPublicAccountView::TestApi test_api(public_account_);
  user_.public_account_info->show_advanced_view = true;
  public_account_->UpdateForUser(user_);
  EXPECT_TRUE(test_api.advanced_view()->GetVisible());

  // Try to change language selection.
  // Open language menu.
  TapOnView(test_api.language_selection_button());
  EXPECT_TRUE(test_api.language_menu_view()->GetVisible());

  // Select second language item:
  // 1. Language menu will be closed automatically.
  // 2. Selected language item will change.
  // 3. Expect RequestPublicSessionKeyboardLayouts mojo call with the selected
  // language item.
  auto client = std::make_unique<MockLoginScreenClient>();
  EXPECT_CALL(*client,
              RequestPublicSessionKeyboardLayouts(
                  user_.basic_user_info.account_id, kFrenchLanguageCode));

  EXPECT_EQ(test_api.selected_language_item().value, kEnglishLanguageCode);
  LoginMenuView::TestApi language_test_api(test_api.language_menu_view());
  TapOnView(language_test_api.contents()->children()[1]);
  EXPECT_FALSE(test_api.language_menu_view()->GetVisible());
  EXPECT_EQ(test_api.selected_language_item().value, kFrenchLanguageCode);
  base::RunLoop().RunUntilIdle();

  // Try to change keyboard selection.
  // Open keyboard menu.
  TapOnView(test_api.keyboard_selection_button());
  EXPECT_TRUE(test_api.keyboard_menu_view()->GetVisible());

  // Select first keyboard item:
  // 1. Keyboard menu will be closed automatically.
  // 2. Selected keyboard item will change.
  EXPECT_EQ(test_api.selected_keyboard_item().value, kKeyboardIdForItem2);
  LoginMenuView::TestApi keyboard_test_api(test_api.keyboard_menu_view());
  TapOnView(keyboard_test_api.contents()->children()[0]);
  EXPECT_FALSE(test_api.keyboard_menu_view()->GetVisible());
  EXPECT_EQ(test_api.selected_keyboard_item().value, kKeyboardIdForItem1);
}

TEST_P(LoginExpandedPublicAccountViewTest, ChangeWarningLabel) {
  LoginExpandedPublicAccountView::TestApi test_api(public_account_);
  views::Label* label = test_api.monitoring_warning_label();
  test_api.ResetUserForTest();
  const base::string16 default_warning = l10n_util::GetStringUTF16(
      IDS_ASH_LOGIN_PUBLIC_ACCOUNT_MONITORING_WARNING);
  EXPECT_EQ(label->GetText(), default_warning);

  public_account_->SetShowFullManagementDisclosure(false);
  EXPECT_EQ(label->GetText(), default_warning);
  const std::string domain =
      user_.public_account_info->device_enterprise_domain.value();
  public_account_->UpdateForUser(user_);
  const base::string16 soft_warning = l10n_util::GetStringFUTF16(
      IDS_ASH_LOGIN_MANAGED_SESSION_MONITORING_SOFT_WARNING,
      base::UTF8ToUTF16(domain));
  EXPECT_EQ(label->GetText(), soft_warning);

  public_account_->SetShowFullManagementDisclosure(true);
  const base::string16 full_warning = l10n_util::GetStringFUTF16(
      IDS_ASH_LOGIN_MANAGED_SESSION_MONITORING_FULL_WARNING,
      base::UTF8ToUTF16(domain));
  EXPECT_EQ(label->GetText(), full_warning);
}

INSTANTIATE_TEST_SUITE_P(All,
                         LoginExpandedPublicAccountViewTest,
                         ::testing::Values("mouse", "touch"));

}  // namespace ash
