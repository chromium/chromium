// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/auth_dialog_contents_view.h"

#include "ash/in_session_auth/mock_in_session_auth_dialog_client.h"
#include "ash/login/ui/login_password_view.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "base/check.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

class AuthDialogContentsViewTest : public AshTestBase {
 public:
  AuthDialogContentsViewTest() = default;

  AuthDialogContentsViewTest(const AuthDialogContentsViewTest&) = delete;
  AuthDialogContentsViewTest& operator=(const AuthDialogContentsViewTest&) =
      delete;

  ~AuthDialogContentsViewTest() override = default;

  // AshTestBase
  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<views::Widget> CreateDialogWidget(
      const display::Display& display,
      int32_t auth_methods,
      uint32_t pin_len = 0) const;

 private:
  std::unique_ptr<MockInSessionAuthDialogClient> client_;
};

void AuthDialogContentsViewTest::SetUp() {
  AshTestBase::SetUp();
  client_ = std::make_unique<MockInSessionAuthDialogClient>();
}

void AuthDialogContentsViewTest::TearDown() {
  client_.reset();
  AshTestBase::TearDown();
}

std::unique_ptr<views::Widget> AuthDialogContentsViewTest::CreateDialogWidget(
    const display::Display& display,
    int32_t auth_methods,
    uint32_t pin_len /*= 0*/) const {
  AuthDialogContentsView::AuthMethodsMetadata auth_metadata;
  auth_metadata.autosubmit_pin_length = pin_len;

  UserAvatar avatar;
  avatar.image = *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_LOGIN_DEFAULT_USER);

  std::unique_ptr<AuthDialogContentsView> dialog =
      std::make_unique<AuthDialogContentsView>(
          auth_methods, "sample_origin_name", auth_metadata, avatar);

  // Hide cursor
  AuthDialogContentsView::TestApi dialog_api(dialog.get());

  if (const raw_ptr<LoginPasswordView> password_view =
          dialog_api.GetPasswordView()) {
    views::TextfieldTestApi(
        LoginPasswordView::TestApi(password_view).textfield())
        .SetCursorLayerOpacity(0.f);
  }

  if (const raw_ptr<LoginPasswordView> pin_view =
          dialog_api.GetPinTextInputView()) {
    views::TextfieldTestApi(LoginPasswordView::TestApi(pin_view).textfield())
        .SetCursorLayerOpacity(0.f);
  }

  // Find the root window for the specified display.
  const int64_t display_id = display.id();
  aura::Window* const root_window =
      Shell::Get()->GetRootWindowForDisplayId(display_id);
  CHECK(root_window);

  // Disable mouse events to hide the mouse cursor, and to avoid
  // Ui response to mouse events (e.g hoover).
  aura::client::GetCursorClient(root_window)->DisableMouseEvents();

  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.delegate = new views::WidgetDelegate();
  params.show_state = ui::mojom::WindowShowState::kNormal;
  params.parent = root_window;
  params.name = "AuthDialogWidget";
  params.delegate->SetInitiallyFocusedView(dialog.get());
  params.delegate->SetOwnedByWidget(true);
  params.bounds = gfx::Rect(dialog->GetPreferredSize());

  std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  widget->Show();
  widget->SetContentsView(std::move(dialog));

  return widget;
}

TEST_F(AuthDialogContentsViewTest, AccessibleProperties) {
  std::unique_ptr<views::Widget> widget = CreateDialogWidget(
      GetPrimaryDisplay(),
      AuthDialogContentsView::AuthMethods::kAuthPassword |
          AuthDialogContentsView::AuthMethods::kAuthFingerprint);
  AuthDialogContentsView* dialog =
      static_cast<AuthDialogContentsView*>(widget->GetContentsView());
  AuthDialogContentsView::TestApi dialog_api(
      static_cast<AuthDialogContentsView*>(dialog));
  dialog_api.PasswordOrPinAuthComplete(
      /*authenticated_by_pin=*/false, /*success=*/false, /*can_use_pin=*/false);
  ui::AXNodeData data;

  // AuthDialogContentsView accessible properties test.
  dialog->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kDialog);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(IDS_ASH_IN_SESSION_AUTH_ACCESSIBLE_TITLE,
                                       u"sample_origin_name"));

  // FingerprintLabel accessible properties test.
  data = ui::AXNodeData();
  dialog_api.GetDialogFingerprintLabel()
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kStaticText);
}

}  // namespace

}  // namespace ash
