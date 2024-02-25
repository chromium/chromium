// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_public_account_user_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/login/ui/login_user_view.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "ui/compositor/layer.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Used to trigger hover event in LoginPublicAccountUserView.
constexpr int kNonEmptySize = 20;

class LoginPublicAccountUserViewTest : public LoginTestBase {
 public:
  LoginPublicAccountUserViewTest(const LoginPublicAccountUserViewTest&) =
      delete;
  LoginPublicAccountUserViewTest& operator=(
      const LoginPublicAccountUserViewTest&) = delete;

 protected:
  LoginPublicAccountUserViewTest() = default;
  ~LoginPublicAccountUserViewTest() override = default;

  // LoginTestBase:
  void SetUp() override {
    LoginTestBase::SetUp();

    user_ = CreatePublicAccountUser("user@domain.com");

    LoginPublicAccountUserView::Callbacks public_account_callbacks;
    public_account_callbacks.on_tap =
        base::BindRepeating(&LoginPublicAccountUserViewTest::OnUserViewTapped,
                            base::Unretained(this));
    public_account_callbacks.on_public_account_tapped = base::BindRepeating(
        &LoginPublicAccountUserViewTest::OnPublicAccountTapped,
        base::Unretained(this));
    public_account_view_ =
        new LoginPublicAccountUserView(user_, public_account_callbacks);

    focusable_view_ = new views::View();
    focusable_view_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    focusable_view_->SetPreferredSize(gfx::Size(kNonEmptySize, kNonEmptySize));

    // We proxy |public_account_view_| inside of |container| so we can control
    // layout.
    auto* container = new views::View();
    container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    container->AddChildView(public_account_view_.get());
    container->AddChildView(focusable_view_.get());
    SetWidget(CreateWidgetWithContent(container));
  }

  LoginUserInfo user_;

  raw_ptr<LoginPublicAccountUserView, DanglingUntriaged> public_account_view_ =
      nullptr;
  raw_ptr<views::View, DanglingUntriaged> focusable_view_ = nullptr;

  int user_view_tap_count_ = 0;
  int public_account_tap_count_ = 0;

 private:
  void OnUserViewTapped() {
    ++user_view_tap_count_;
    // Simulate the pod becoming active as it does when it is a part of
    // LoginAuthUserView.
    public_account_view_->SetAuthEnabled(/*enabled=*/true, /*animate=*/false);
    // 'on_tap' must run before 'public_account_tap'
    ASSERT_TRUE(public_account_tap_count_ == 0);
  }

  void OnPublicAccountTapped() { ++public_account_tap_count_; }
};

}  // namespace

// Verifies that an auth enabled public account user is opaque.
// Verifies that hovered view is opaque.
TEST_F(LoginPublicAccountUserViewTest, EnableAuthAndHoverOpaque) {
  LoginPublicAccountUserView::TestApi public_account_test(public_account_view_);
  LoginUserView::TestApi user_test(public_account_view_->user_view());

  // Make focusable_view_ to hold focus. The user view cannot have focus
  // since focus will keep it opaque.
  focusable_view_->RequestFocus();
  EXPECT_FALSE(public_account_view_->user_view()->HasFocus());
  EXPECT_FALSE(user_test.is_opaque());
  EXPECT_NE(public_account_test.arrow_button()->layer()->opacity(), 1);

  // Auth enabled view must be opaque.
  public_account_view_->SetAuthEnabled(true /*enabled*/, false /*animate*/);
  EXPECT_TRUE(user_test.is_opaque());
  EXPECT_EQ(public_account_test.arrow_button()->layer()->opacity(), 1);

  // Auth enabled view stays opaque regardless of hover.
  GetEventGenerator()->MoveMouseTo(
      public_account_view_->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(user_test.is_opaque());
  EXPECT_EQ(public_account_test.arrow_button()->layer()->opacity(), 1);
  GetEventGenerator()->MoveMouseTo(
      focusable_view_->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(user_test.is_opaque());
  EXPECT_EQ(public_account_test.arrow_button()->layer()->opacity(), 1);

  // Disable auth makes view non-opaque.
  public_account_view_->SetAuthEnabled(false /*enabled*/, false /*animate*/);
  EXPECT_FALSE(user_test.is_opaque());
  EXPECT_NE(public_account_test.arrow_button()->layer()->opacity(), 1);

  // Move mouse over the view makes it opaque.
  GetEventGenerator()->MoveMouseTo(
      public_account_view_->user_view()->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(user_test.is_opaque());
  EXPECT_EQ(public_account_test.arrow_button()->layer()->opacity(), 1);

  // Move out the view makes it non-opaque.
  GetEventGenerator()->MoveMouseTo(
      focusable_view_->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(user_test.is_opaque());
  EXPECT_NE(public_account_test.arrow_button()->layer()->opacity(), 1);
}

// Verifies that LoginPublicAccountUserView::Callbacks run successfully.
TEST_F(LoginPublicAccountUserViewTest, VerifyCallackRun) {
  LoginPublicAccountUserView::TestApi public_account_test(public_account_view_);

  // Tap on the user view and verify on_tap callback is run.
  GetEventGenerator()->MoveMouseTo(
      public_account_view_->user_view()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_EQ(user_view_tap_count_, 1);

  // Now that the pod is active, clicking on the arrow should not trigger
  // another 'on_tap' callback. Only on_public_account_tapped should be called.
  user_view_tap_count_ = 0;

  GetEventGenerator()->MoveMouseTo(
      public_account_test.arrow_button()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_EQ(public_account_tap_count_, 1);
  EXPECT_EQ(user_view_tap_count_, 0);
}

// Verifies that tapping on the arrow when the pod isn't active will first call
// the 'on_tap' callback to make the pod active, and then run the public
// account tap callback.
TEST_F(LoginPublicAccountUserViewTest, OnTapIsCalledWhenNotActive) {
  LoginPublicAccountUserView::TestApi public_account_test(public_account_view_);

  // Disable auth to simulate the pod being inactive.
  public_account_view_->SetAuthEnabled(false, /*animate=*/false);

  // Tap on the arrow and verify that 'on_tap' will be called before
  // 'on_public_account_tapped' is called. The temporal aspect of this
  // expectation is verified in OnUserViewTapped.
  GetEventGenerator()->MoveMouseTo(
      public_account_test.arrow_button()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_EQ(user_view_tap_count_, 1);
  EXPECT_EQ(public_account_tap_count_, 1);
}

// Verifies that displaying the arrow button does not change the view bounds.
// Also arrow button should always have the same horizontal centering as user
// view.
TEST_F(LoginPublicAccountUserViewTest, ArrowButtonDoesNotChangeViewBounds) {
  LoginPublicAccountUserView::TestApi public_account_test(public_account_view_);
  EXPECT_FALSE(public_account_view_->auth_enabled());
  const gfx::Rect auth_disabled_bounds =
      public_account_view_->GetBoundsInScreen();
  EXPECT_NE(auth_disabled_bounds, gfx::Rect());

  auto* arrow_button = public_account_test.arrow_button();
  // Move mouse over the view, the arrow button becomes opaque.
  // View bounds should not change.
  GetEventGenerator()->MoveMouseTo(
      public_account_view_->user_view()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(arrow_button->layer()->opacity(), 1);
  EXPECT_EQ(public_account_view_->GetBoundsInScreen(), auth_disabled_bounds);
  int arrow_left_margin = arrow_button->GetBoundsInScreen().x();
  int arrow_right_margin = public_account_view_->GetBoundsInScreen().width() -
                           arrow_button->GetBoundsInScreen().x() -
                           arrow_button->GetBoundsInScreen().width();
  EXPECT_NEAR(arrow_left_margin, arrow_right_margin, 1);

  // Move out the view makes it non-opaque. View bounds should stay the same.
  GetEventGenerator()->MoveMouseTo(
      focusable_view_->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(arrow_button->layer()->opacity(), 0);
  EXPECT_EQ(public_account_view_->GetBoundsInScreen(), auth_disabled_bounds);

  // Set auth enable forces arrow button to be opaque.
  // View bounds should not change.
  public_account_view_->SetAuthEnabled(true /*enabled*/, false /*animate*/);
  EXPECT_TRUE(public_account_view_->auth_enabled());
  EXPECT_EQ(arrow_button->layer()->opacity(), 1);
  EXPECT_EQ(public_account_view_->GetBoundsInScreen(), auth_disabled_bounds);
  arrow_left_margin = arrow_button->GetBoundsInScreen().x();
  arrow_right_margin = public_account_view_->GetBoundsInScreen().width() -
                       arrow_button->GetBoundsInScreen().x() -
                       arrow_button->GetBoundsInScreen().width();
  EXPECT_NEAR(arrow_left_margin, arrow_right_margin, 1);
}

}  // namespace ash
