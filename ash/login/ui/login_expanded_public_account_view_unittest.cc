// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_expanded_public_account_view.h"

#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/login/ui/public_account_monitoring_info_dialog.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/login_types.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/link_fragment.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/test/combobox_test_api.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Fake language and keyboard information.
constexpr char kEnglishLanguageCode[] = "language_code1";
constexpr char kEnglishLanguageName[] = "English";
constexpr char kFrenchLanguageCode[] = "language_code2";
constexpr char kFrenchLanguageName[] = "French";

constexpr char kKeyboardIdForItem1[] = "keyboard_id1";
constexpr char kKeyboardNameForItem1[] = "keyboard1";
constexpr char kKeyboardIdForItem2[] = "keyboard_id2";
constexpr char kKeyboardNameForItem2[] = "keyboard2";

enum class InputMethod {
  kMouse,
  kTouch,
};

enum class Orientation {
  kLandscape,
  kPortrait,
};

struct TestParams {
  const InputMethod input_method;
  const Orientation orientation;
};

class LoginExpandedPublicAccountViewTest
    : public LoginTestBase,
      public ::testing::WithParamInterface<TestParams> {
 public:
  LoginExpandedPublicAccountViewTest(
      const LoginExpandedPublicAccountViewTest&) = delete;
  LoginExpandedPublicAccountViewTest& operator=(
      const LoginExpandedPublicAccountViewTest&) = delete;

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

    container_ = new views::BoxLayoutView();
    container_->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kStart);
    container_->AddChildView(public_account_.get());
    container_->AddChildView(other_view_.get());
    auto widget = CreateWidgetWithContent(container_);
    switch (GetParam().orientation) {
      case Orientation::kLandscape:
        widget->SetSize({800, 600});
        break;
      case Orientation::kPortrait:
        widget->SetSize({600, 800});
        break;
    }
    widget->LayoutRootViewIfNecessary();
    SetWidget(std::move(widget));
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
    // Layout test view container first, giving the expanded view the
    // opportunity to assign `tap_target` non-zero size.
    widget()->LayoutRootViewIfNecessary();
    switch (GetParam().input_method) {
      case InputMethod::kMouse:
        GetEventGenerator()->MoveMouseTo(
            tap_target->GetBoundsInScreen().CenterPoint());
        GetEventGenerator()->ClickLeftButton();
        break;
      case InputMethod::kTouch:
        GetEventGenerator()->MoveTouch(
            tap_target->GetBoundsInScreen().CenterPoint());
        GetEventGenerator()->PressTouch();
        GetEventGenerator()->ReleaseTouch();
        break;
    }
  }

  LoginUserInfo user_;

  // Owned by test widget view hierarchy.
  raw_ptr<views::BoxLayoutView, DanglingUntriaged> container_ = nullptr;
  raw_ptr<LoginExpandedPublicAccountView, DanglingUntriaged> public_account_ =
      nullptr;
  raw_ptr<views::View, DanglingUntriaged> other_view_ = nullptr;
};

}  // namespace

// Verifies toggle advanced view will update the layout correctly.
TEST_P(LoginExpandedPublicAccountViewTest, ToggleAdvancedView) {
  const int initial_width = public_account_->width();
  const int initial_height = public_account_->height();

  LoginExpandedPublicAccountView::TestApi test_api(public_account_);
  EXPECT_FALSE(user_.public_account_info->show_advanced_view);
  EXPECT_FALSE(test_api.advanced_view()->GetVisible());

  // Toggle show_advanced_view.
  user_.public_account_info->show_advanced_view = true;
  public_account_->UpdateForUser(user_);

  // Advanced view is shown and the overall size does not change.
  EXPECT_TRUE(test_api.advanced_view()->GetVisible());
  ui::MouseEvent fake_event(ui::EventType::kMouseMoved, gfx::Point(),
                            gfx::Point(), base::TimeTicks(), 0, 0);
  EXPECT_EQ(test_api.advanced_view_button()->GetCursor(fake_event),
            ui::mojom::CursorType::kHand);
  EXPECT_EQ(public_account_->width(), initial_width);
  EXPECT_EQ(public_account_->height(), initial_height);

  // Click on the show advanced button.
  TapOnView(test_api.advanced_view_button());

  // Advanced view is hidden and the overall size does not change.
  EXPECT_FALSE(test_api.advanced_view()->GetVisible());
  EXPECT_EQ(public_account_->width(), initial_width);
  EXPECT_EQ(public_account_->height(), initial_height);
}

// Verifies learn more dialog shows up correctly.
TEST_P(LoginExpandedPublicAccountViewTest, ShowLearnMoreDialog) {
  LoginExpandedPublicAccountView::TestApi test_api(public_account_);
  EXPECT_EQ(test_api.learn_more_dialog(), nullptr);

  // Tap on the learn more link.
  const auto& children = test_api.learn_more_label()->children();
  const auto it = base::ranges::find_if(children, [](views::View* child) {
    return views::IsViewClass<views::LinkFragment>(child);
  });
  DCHECK(it != children.cend());
  TapOnView(*it);
  ASSERT_NE(test_api.learn_more_dialog(), nullptr);
  EXPECT_TRUE(test_api.learn_more_dialog()->GetVisible());

  // When learn more dialog is shown, tap outside of public account expanded
  // view should not hide it.
  TapOnView(other_view_);
  EXPECT_TRUE(public_account_->GetVisible());
  ASSERT_NE(test_api.learn_more_dialog(), nullptr);
  EXPECT_TRUE(test_api.learn_more_dialog()->GetVisible());

  // If the learn more dialog is shown, escape key should close the learn more
  // dialog, but not the public account view.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_ESCAPE, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(test_api.learn_more_dialog(), nullptr);
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
  std::string selected_language_item_value =
      test_api.selected_language_item_value();
  EXPECT_EQ(selected_language_item_value, kEnglishLanguageCode);
  std::string selected_keyboard_item_value =
      test_api.selected_keyboard_item_value();
  EXPECT_EQ(selected_keyboard_item_value, kKeyboardIdForItem2);

  // Expect LaunchPublicSession mojo call when the submit button is clicked.
  auto client = std::make_unique<MockLoginScreenClient>();
  EXPECT_CALL(*client, LaunchPublicSession(user_.basic_user_info.account_id,
                                           selected_language_item_value,
                                           selected_keyboard_item_value));

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

  // Open language menu.
  PublicAccountMenuView* language_menu_view = test_api.language_menu_view();
  EXPECT_FALSE(language_menu_view->IsMenuRunning());
  TapOnView(language_menu_view);
  EXPECT_TRUE(language_menu_view->IsMenuRunning());

  // First language item is selected.
  EXPECT_EQ(test_api.selected_language_item_value(), kEnglishLanguageCode);
  ASSERT_EQ(2u, language_menu_view->GetRowCount());
  EXPECT_EQ(0u, language_menu_view->GetSelectedRow());

  // Once the menu is open, the focus is set on the entire view.
  // The first key press will set the focus on the language item, the second
  // press will select it and should close the language menu.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  EXPECT_FALSE(language_menu_view->IsMenuRunning());

  // Open keyboard menu.
  PublicAccountMenuView* keyboard_menu_view = test_api.keyboard_menu_view();
  EXPECT_FALSE(keyboard_menu_view->IsMenuRunning());
  TapOnView(keyboard_menu_view);
  EXPECT_TRUE(keyboard_menu_view->IsMenuRunning());

  // Second keyboard item is selected.
  EXPECT_EQ(test_api.selected_keyboard_item_value(), kKeyboardIdForItem2);
  ASSERT_EQ(2u, keyboard_menu_view->GetRowCount());
  EXPECT_EQ(1u, keyboard_menu_view->GetSelectedRow());

  // Once the menu is open, the focus is set on the entire view.
  // The first key press will set the focus on the keyboard item, the second
  // press will select it and should close the keyboard menu.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  EXPECT_FALSE(keyboard_menu_view->IsMenuRunning());
}

TEST_P(LoginExpandedPublicAccountViewTest, ChangeMenuSelection) {
  LoginExpandedPublicAccountView::TestApi test_api(public_account_);
  user_.public_account_info->show_advanced_view = true;
  public_account_->UpdateForUser(user_);
  EXPECT_TRUE(test_api.advanced_view()->GetVisible());

  // Open language menu.
  PublicAccountMenuView* language_menu_view = test_api.language_menu_view();
  EXPECT_FALSE(language_menu_view->IsMenuRunning());
  TapOnView(language_menu_view);
  EXPECT_TRUE(language_menu_view->IsMenuRunning());

  // Select second language item:
  // 1. Selected language will change.
  // 2. Expect RequestPublicSessionKeyboardLayouts mojo call with the selected
  // language item.
  auto client = std::make_unique<MockLoginScreenClient>();
  EXPECT_CALL(*client,
              RequestPublicSessionKeyboardLayouts(
                  user_.basic_user_info.account_id, kFrenchLanguageCode));

  EXPECT_EQ(test_api.selected_language_item_value(), kEnglishLanguageCode);
  std::unique_ptr<views::test::ComboboxTestApi> language_test_api =
      std::make_unique<views::test::ComboboxTestApi>(language_menu_view);
  // Note that we do not need to open the menu to make this test API call.
  language_test_api->PerformActionAt(1);
  EXPECT_EQ(test_api.selected_language_item_value(), kFrenchLanguageCode);
  base::RunLoop().RunUntilIdle();

  PublicAccountMenuView* keyboard_menu_view = test_api.keyboard_menu_view();
  // Close language menu.
  // `PerformActionAt` does not close the menu automatically, make a click
  // outside of language menu boundaries to close the menu.
  EXPECT_TRUE(language_menu_view->IsMenuRunning());
  TapOnView(keyboard_menu_view);
  EXPECT_FALSE(language_menu_view->IsMenuRunning());
  // Open keyboard menu.
  EXPECT_FALSE(keyboard_menu_view->IsMenuRunning());
  TapOnView(keyboard_menu_view);
  EXPECT_TRUE(keyboard_menu_view->IsMenuRunning());

  // Select first keyboard item, selected keyboard will change.
  EXPECT_EQ(test_api.selected_keyboard_item_value(), kKeyboardIdForItem2);
  std::unique_ptr<views::test::ComboboxTestApi> keyboard_test_api =
      std::make_unique<views::test::ComboboxTestApi>(
          test_api.keyboard_menu_view());
  keyboard_test_api->PerformActionAt(0);
  EXPECT_EQ(test_api.selected_keyboard_item_value(), kKeyboardIdForItem1);
}

TEST_P(LoginExpandedPublicAccountViewTest, ChangeWarningLabel) {
  LoginExpandedPublicAccountView::TestApi test_api(public_account_);
  views::Label* label = test_api.monitoring_warning_label();
  test_api.ResetUserForTest();
  const std::u16string default_warning = l10n_util::GetStringUTF16(
      IDS_ASH_LOGIN_PUBLIC_ACCOUNT_MONITORING_WARNING);
  EXPECT_EQ(label->GetText(), default_warning);

  public_account_->SetShowFullManagementDisclosure(false);
  EXPECT_EQ(label->GetText(), default_warning);
  const std::string domain =
      user_.public_account_info->device_enterprise_manager.value();
  public_account_->UpdateForUser(user_);
  const std::u16string soft_warning = l10n_util::GetStringFUTF16(
      IDS_ASH_LOGIN_MANAGED_SESSION_MONITORING_SOFT_WARNING,
      base::UTF8ToUTF16(domain));
  EXPECT_EQ(label->GetText(), soft_warning);

  public_account_->SetShowFullManagementDisclosure(true);
  const std::u16string full_warning = l10n_util::GetStringFUTF16(
      IDS_ASH_LOGIN_MANAGED_SESSION_MONITORING_FULL_WARNING,
      base::UTF8ToUTF16(domain));
  EXPECT_EQ(label->GetText(), full_warning);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    LoginExpandedPublicAccountViewTest,
    ::testing::Values(TestParams{InputMethod::kMouse, Orientation::kLandscape},
                      TestParams{InputMethod::kTouch, Orientation::kLandscape},
                      TestParams{InputMethod::kTouch, Orientation::kPortrait}));

}  // namespace ash
