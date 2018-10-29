// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_expanded_public_account_view.h"
#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/login_bubble.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/login/ui/public_account_warning_dialog.h"
#include "ash/login/ui/views_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Total width of the expanded view.
constexpr int kBubbleTotalWidthDp = 600;
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

class LoginExpandedPublicAccountViewTest : public LoginTestBase {
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
    container_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal));
    container_->AddChildView(public_account_);
    container_->AddChildView(other_view_);
    SetWidget(CreateWidgetWithContent(container_));
  }

  // Add two fake language items, the first item is selected by default.
  void SetupLanguageInfo() {
    std::vector<ash::mojom::LocaleItemPtr> result;
    ash::mojom::LocaleItemPtr locale_item1 = ash::mojom::LocaleItem::New();
    locale_item1->language_code = kEnglishLanguageCode;
    locale_item1->title = kEnglishLanguageName;

    ash::mojom::LocaleItemPtr locale_item2 = ash::mojom::LocaleItem::New();
    locale_item2->language_code = kFrenchLanguageCode;
    locale_item2->title = kFrenchLanguageName;
    result.push_back(std::move(locale_item1));
    result.push_back(std::move(locale_item2));
    user_->public_account_info->available_locales = std::move(result);
    user_->public_account_info->default_locale = kEnglishLanguageCode;
  }

  // Add two fake keyboard items, the second item is selected by default.
  void SetupKeyboardInfo() {
    std::vector<ash::mojom::InputMethodItemPtr> result;
    ash::mojom::InputMethodItemPtr keyboard_item1 =
        ash::mojom::InputMethodItem::New();
    keyboard_item1->ime_id = kKeyboardIdForItem1;
    keyboard_item1->title = kKeyboardNameForItem1;

    ash::mojom::InputMethodItemPtr keyboard_item2 =
        ash::mojom::InputMethodItem::New();
    keyboard_item2->ime_id = kKeyboardIdForItem2;
    keyboard_item2->title = kKeyboardNameForItem2;
    keyboard_item2->selected = true;
    result.push_back(std::move(keyboard_item1));
    result.push_back(std::move(keyboard_item2));

    user_->public_account_info->keyboard_layouts = std::move(result);
  }

  void TapOnView(views::View* tap_target) {
    GetEventGenerator()->MoveMouseTo(
        tap_target->GetBoundsInScreen().CenterPoint());
    GetEventGenerator()->ClickLeftButton();
  }

  mojom::LoginUserInfoPtr user_;

  // Owned by test widget view hierarchy.
  views::View* container_ = nullptr;
  LoginExpandedPublicAccountView* public_account_ = nullptr;
  views::View* other_view_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginExpandedPublicAccountViewTest);
};

}  // namespace

// Verifies toggle advanced view will update the layout correctly.
TEST_F(LoginExpandedPublicAccountViewTest, ToggleAdvancedView) {
  public_account_->SizeToPreferredSize();
  EXPECT_EQ(public_account_->width(), kBubbleTotalWidthDp);
  EXPECT_EQ(public_account_->height(), kBubbleTotalHeightDp);

  LoginExpandedPublicAccountView::TestApi test_api(public_account_);
  EXPECT_FALSE(user_->public_account_info->show_advanced_view);
  EXPECT_FALSE(test_api.advanced_view()->visible());

  // Toggle show_advanced_view.
  user_->public_account_info->show_advanced_view = true;
  public_account_->UpdateForUser(user_);

  // Advanced view is shown and the overall size does not change.
  EXPECT_TRUE(test_api.advanced_view()->visible());
  EXPECT_EQ(public_account_->width(), kBubbleTotalWidthDp);
  EXPECT_EQ(public_account_->height(), kBubbleTotalHeightDp);

  // Click on the show advanced button.
  TapOnView(test_api.advanced_view_button());

  // Advanced view is hidden and the overall size does not change.
  EXPECT_FALSE(test_api.advanced_view()->visible());
  EXPECT_EQ(public_account_->width(), kBubbleTotalWidthDp);
  EXPECT_EQ(public_account_->height(), kBubbleTotalHeightDp);
}

// Verifies warning dialog shows up correctly.
TEST_F(LoginExpandedPublicAccountViewTest, ShowWarningDialog) {
  LoginExpandedPublicAccountView::TestApi test_api(public_account_);
  views::StyledLabel::TestApi styled_label_test(test_api.learn_more_label());
  EXPECT_EQ(test_api.warning_dialog(), nullptr);
  EXPECT_EQ(styled_label_test.link_targets().size(), 1U);

  // Tap on the learn more link.
  views::View* link_view = styled_label_test.link_targets().begin()->first;
  GetEventGenerator()->MoveMouseTo(
      link_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_NE(test_api.warning_dialog(), nullptr);
  EXPECT_TRUE(test_api.warning_dialog()->IsVisible());

  // When warning dialog is shown, tap outside of public account expanded view
  // should not hide it.
  GetEventGenerator()->MoveMouseTo(
      other_view_->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_TRUE(public_account_->visible());
  EXPECT_NE(test_api.warning_dialog(), nullptr);
  EXPECT_TRUE(test_api.warning_dialog()->IsVisible());

  // If the warning dialog is shown, escape key should close the waring dialog,
  // but not the public account view.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_ESCAPE, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(test_api.warning_dialog(), nullptr);
  EXPECT_TRUE(public_account_->visible());

  // Press escape again should hide the public account expanded view.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_ESCAPE, 0);
  EXPECT_FALSE(public_account_->visible());
}

// Verifies tap on submit button will try to launch public session.
TEST_F(LoginExpandedPublicAccountViewTest, LaunchPublicSession) {
  LoginExpandedPublicAccountView::TestApi test_api(public_account_);

  // Verify the language and keyboard information is populated correctly.
  std::string selected_language = test_api.selected_language_item().value;
  std::string selected_keyboard = test_api.selected_keyboard_item().value;
  EXPECT_EQ(selected_language, kEnglishLanguageCode);
  EXPECT_EQ(selected_keyboard, kKeyboardIdForItem2);

  // Expect LanuchPublicSession mojo call when the submit button is clicked.
  std::unique_ptr<MockLoginScreenClient> client = BindMockLoginScreenClient();
  EXPECT_CALL(*client,
              LaunchPublicSession(user_->basic_user_info->account_id,
                                  selected_language, selected_keyboard));

  // Click on the submit button.
  TapOnView(test_api.submit_button());
  base::RunLoop().RunUntilIdle();
}

// Verifies both language and keyboard menus shows up correctly.
TEST_F(LoginExpandedPublicAccountViewTest, ShowLanguageAndKeyboardMenu) {
  LoginExpandedPublicAccountView::TestApi test_api(public_account_);
  EXPECT_FALSE(user_->public_account_info->show_advanced_view);
  EXPECT_FALSE(test_api.advanced_view()->visible());

  // Toggle show_advanced_view.
  user_->public_account_info->show_advanced_view = true;
  public_account_->UpdateForUser(user_);
  EXPECT_TRUE(test_api.advanced_view()->visible());

  // Tap on language selection button should bring up the language menu.
  EXPECT_FALSE(test_api.language_menu()->IsVisible());
  TapOnView(test_api.language_selection_button());
  EXPECT_TRUE(test_api.language_menu()->IsVisible());

  // First language item is selected, and selected item should have focus.
  EXPECT_EQ(test_api.selected_language_item().value, kEnglishLanguageCode);
  auto* language_menu_view =
      static_cast<LoginMenuView*>(test_api.language_menu()->bubble_view());
  LoginMenuView::TestApi language_test_api(language_menu_view);
  EXPECT_TRUE(language_test_api.contents()->child_count() == 2);
  EXPECT_TRUE(language_test_api.contents()->child_at(0)->HasFocus());

  // Select language item should close the language menu.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  EXPECT_FALSE(test_api.language_menu()->IsVisible());

  // Tap on keyboard selection button should bring up the keyboard menu.
  EXPECT_FALSE(test_api.keyboard_menu()->IsVisible());
  TapOnView(test_api.keyboard_selection_button());
  EXPECT_TRUE(test_api.keyboard_menu()->IsVisible());

  // Second keyboard item is selected, and selected item should have focus.
  EXPECT_EQ(test_api.selected_keyboard_item().value, kKeyboardIdForItem2);
  auto* keyboard_menu_view =
      static_cast<LoginMenuView*>(test_api.keyboard_menu()->bubble_view());
  LoginMenuView::TestApi keyboard_test_api(keyboard_menu_view);
  EXPECT_TRUE(keyboard_test_api.contents()->child_count() == 2);
  EXPECT_TRUE(keyboard_test_api.contents()->child_at(1)->HasFocus());

  // Select keyboard item should close the keyboard menu.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  EXPECT_FALSE(test_api.keyboard_menu()->IsVisible());
}

TEST_F(LoginExpandedPublicAccountViewTest, ChangeMenuSelection) {
  LoginExpandedPublicAccountView::TestApi test_api(public_account_);
  user_->public_account_info->show_advanced_view = true;
  public_account_->UpdateForUser(user_);
  EXPECT_TRUE(test_api.advanced_view()->visible());

  // Try to change language selection.
  // Open language menu.
  TapOnView(test_api.language_selection_button());
  EXPECT_TRUE(test_api.language_menu()->IsVisible());

  // Select second language item:
  // 1. Language menu will be closed automatically.
  // 2. Selected language item will change.
  // 3. Expect RequestPublicSessionKeyboardLayouts mojo call with the selected
  // language item.
  std::unique_ptr<MockLoginScreenClient> client = BindMockLoginScreenClient();
  EXPECT_CALL(*client,
              RequestPublicSessionKeyboardLayouts(
                  user_->basic_user_info->account_id, kFrenchLanguageCode));

  EXPECT_EQ(test_api.selected_language_item().value, kEnglishLanguageCode);
  auto* language_menu_view =
      static_cast<LoginMenuView*>(test_api.language_menu()->bubble_view());
  LoginMenuView::TestApi language_test_api(language_menu_view);
  TapOnView(language_test_api.contents()->child_at(1));
  EXPECT_FALSE(test_api.language_menu()->IsVisible());
  EXPECT_EQ(test_api.selected_language_item().value, kFrenchLanguageCode);
  base::RunLoop().RunUntilIdle();

  // Try to change keyboard selection.
  // Open keyboard menu.
  TapOnView(test_api.keyboard_selection_button());
  EXPECT_TRUE(test_api.keyboard_menu()->IsVisible());

  // Select first keyboard item:
  // 1. Keyboard menu will be closed automatically.
  // 2. Selected keyboard item will change.
  EXPECT_EQ(test_api.selected_keyboard_item().value, kKeyboardIdForItem2);
  auto* keyboard_menu_view =
      static_cast<LoginMenuView*>(test_api.keyboard_menu()->bubble_view());
  LoginMenuView::TestApi keyboard_test_api(keyboard_menu_view);
  TapOnView(keyboard_test_api.contents()->child_at(0));
  EXPECT_FALSE(test_api.keyboard_menu()->IsVisible());
  EXPECT_EQ(test_api.selected_keyboard_item().value, kKeyboardIdForItem1);
}

}  // namespace ash
