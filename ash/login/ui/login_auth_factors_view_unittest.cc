// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_auth_factors_view.h"

#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/auth_factor_model.h"
#include "ash/login/ui/auth_icon_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/ax_event_observer.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr base::TimeDelta kErrorTimeout = base::Seconds(3);

using AuthFactorState = AuthFactorModel::AuthFactorState;

class FakeAuthFactorModel : public AuthFactorModel {
 public:
  explicit FakeAuthFactorModel(AuthFactorType type) : type_(type) {}

  FakeAuthFactorModel(const FakeAuthFactorModel&) = delete;
  FakeAuthFactorModel& operator=(const FakeAuthFactorModel&) = delete;
  ~FakeAuthFactorModel() override = default;

  // AuthFactorModel:
  AuthFactorState GetAuthFactorState() const override { return state_; }

  // AuthFactorModel:
  AuthFactorType GetType() const override { return type_; }

  // AuthFactorModel:
  int GetLabelId() const override {
    return IDS_SMART_LOCK_LABEL_LOOKING_FOR_PHONE;
  }

  // AuthFactorModel:
  bool ShouldAnnounceLabel() const override { return should_announce_label_; }

  // AuthFactorModel:
  int GetAccessibleNameId() const override {
    return IDS_SMART_LOCK_LABEL_LOOKING_FOR_PHONE;
  }

  // AuthFactorModel:
  void DoHandleTapOrClick() override { do_handle_tap_or_click_called_ = true; }

  // AuthFactorModel:
  void DoHandleErrorTimeout() override { do_handle_error_timeout_num_calls_++; }

  // AuthFactorModel:
  void UpdateIcon(AuthIconView* icon) override {
    ASSERT_TRUE(icon);
    icon_ = icon;
  }

  using AuthFactorModel::has_permanent_error_display_timed_out_;
  using AuthFactorModel::RefreshUI;

  AuthFactorType type_;
  AuthFactorState state_ = AuthFactorState::kReady;
  raw_ptr<AuthIconView> icon_ = nullptr;
  bool do_handle_tap_or_click_called_ = false;
  bool should_announce_label_ = false;
  int do_handle_error_timeout_num_calls_ = 0;
};

class ScopedAXEventObserver : public views::AXEventObserver {
 public:
  ScopedAXEventObserver(views::View* view, ax::mojom::Event event_type)
      : view_(view), event_type_(event_type) {
    views::AXEventManager::Get()->AddObserver(this);
  }
  ScopedAXEventObserver(const ScopedAXEventObserver&) = delete;
  ScopedAXEventObserver& operator=(const ScopedAXEventObserver&) = delete;
  ~ScopedAXEventObserver() override {
    views::AXEventManager::Get()->RemoveObserver(this);
  }

  bool event_called = false;

 private:
  // views::AXEventObserver:
  void OnViewEvent(views::View* view, ax::mojom::Event event_type) override {
    if (view == view_ && event_type == event_type_) {
      event_called = true;
    }
  }

  raw_ptr<views::View> view_;
  ax::mojom::Event event_type_;
};

}  // namespace

class LoginAuthFactorsViewUnittest : public LoginTestBase {
 public:
  LoginAuthFactorsViewUnittest(const LoginAuthFactorsViewUnittest&) = delete;
  LoginAuthFactorsViewUnittest& operator=(const LoginAuthFactorsViewUnittest&) =
      delete;

 protected:
  LoginAuthFactorsViewUnittest() = default;
  ~LoginAuthFactorsViewUnittest() override = default;

  // LoginTestBase:
  void SetUp() override {
    LoginTestBase::SetUp();

    // We proxy |view_| inside of |container_| so we can control layout.
    // TODO(crbug.com/1233614): Add layout tests to check
    // positioning/ordering of icons.
    container_ = new views::View();
    container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));

    view_ = container_->AddChildView(std::make_unique<LoginAuthFactorsView>(
        base::BindRepeating(
            &LoginAuthFactorsViewUnittest::set_click_to_enter_called,
            base::Unretained(this), /*click_to_enter_called=*/true),
        base::BindRepeating(
            &LoginAuthFactorsViewUnittest::set_auth_factor_is_hiding_password,
            base::Unretained(this))));
    SetWidget(CreateWidgetWithContent(container_));
  }

  void TearDown() override {
    container_ = nullptr;
    view_ = nullptr;
    LoginTestBase::TearDown();
  }

  void AddAuthFactors(std::vector<AuthFactorType> types) {
    for (AuthFactorType type : types) {
      auto auth_factor = std::make_unique<FakeAuthFactorModel>(type);
      auth_factors_.push_back(auth_factor.get());
      view_->AddAuthFactor(std::move(auth_factor));
    }
  }

  size_t GetVisibleIconCount() {
    LoginAuthFactorsView::TestApi test_api(view_);
    size_t count = 0;
    for (views::View* icon : test_api.auth_factor_icon_row()->children()) {
      if (icon->GetVisible()) {
        count++;
      }
    }
    return count;
  }

  bool ShouldHidePasswordField() { return view_->ShouldHidePasswordField(); }

  void set_click_to_enter_called(bool click_to_enter_called) {
    click_to_enter_called_ = click_to_enter_called;
  }

  void set_auth_factor_is_hiding_password(bool auth_factor_is_hiding_password) {
    auth_factor_is_hiding_password_ = auth_factor_is_hiding_password;
  }

  void VerifyAuthenticatedUiState(
      bool is_lock_screen,
      LoginAuthFactorsView::TestApi& test_api,
      bool should_hide_password_field_when_authenticated) {
    EXPECT_TRUE(test_api.checkmark_icon()->GetVisible());
    EXPECT_FALSE(test_api.arrow_button()->GetVisible());
    EXPECT_FALSE(test_api.arrow_nudge_animation()->GetVisible());
    EXPECT_FALSE(test_api.auth_factor_icon_row()->GetVisible());
    EXPECT_EQ(l10n_util::GetStringUTF16(is_lock_screen
                                            ? IDS_AUTH_FACTOR_LABEL_UNLOCKED
                                            : IDS_AUTH_FACTOR_LABEL_SIGNED_IN),
              test_api.label()->GetText());
    EXPECT_EQ(should_hide_password_field_when_authenticated,
              ShouldHidePasswordField());
  }

  void TestArrowButtonClearsFocus(AuthFactorState state_after_click_required) {
    ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
        ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
    AddAuthFactors({AuthFactorType::kFingerprint, AuthFactorType::kSmartLock});

    LoginAuthFactorsView::TestApi test_api(view_);
    auth_factors_[0]->state_ = AuthFactorState::kReady;
    auth_factors_[1]->state_ = AuthFactorState::kClickRequired;
    test_api.UpdateState();

    EXPECT_TRUE(view_->GetFocusManager()->GetFocusedView());
    EXPECT_TRUE(test_api.arrow_button()->HasFocus());

    auth_factors_[1]->state_ = state_after_click_required;
    test_api.UpdateState();

    EXPECT_FALSE(view_->GetFocusManager()->GetFocusedView());
  }

  raw_ptr<views::View> container_ = nullptr;
  raw_ptr<LoginAuthFactorsView> view_ = nullptr;  // Owned by container.
  std::vector<raw_ptr<FakeAuthFactorModel, VectorExperimental>> auth_factors_;
  bool click_to_enter_called_ = false;
  bool auth_factor_is_hiding_password_ = false;
};

TEST_F(LoginAuthFactorsViewUnittest, TapOrClickCalled) {
  AddAuthFactors({AuthFactorType::kFingerprint, AuthFactorType::kSmartLock});
  auto* factor = auth_factors_[0].get();

  // RefreshUI() calls UpdateIcon(), which captures a pointer to the
  // icon.
  factor->RefreshUI();

  EXPECT_FALSE(factor->do_handle_tap_or_click_called_);
  const gfx::Point point(0, 0);
  factor->icon_->OnMousePressed(ui::MouseEvent(ui::EventType::kMousePressed,
                                               point, point,
                                               base::TimeTicks::Now(), 0, 0));
  EXPECT_TRUE(factor->do_handle_tap_or_click_called_);
}

TEST_F(LoginAuthFactorsViewUnittest, ShouldAnnounceLabel) {
  AddAuthFactors({AuthFactorType::kFingerprint, AuthFactorType::kSmartLock});
  LoginAuthFactorsView::TestApi test_api(view_);
  views::Label* label = test_api.label();
  ScopedAXEventObserver alert_observer(label, ax::mojom::Event::kAlert);
  for (FakeAuthFactorModel* factor : auth_factors_) {
    factor->state_ = AuthFactorState::kAvailable;
  }

  auto* factor = auth_factors_[0].get();
  ASSERT_FALSE(factor->ShouldAnnounceLabel());
  ASSERT_FALSE(alert_observer.event_called);

  test_api.UpdateState();
  ASSERT_FALSE(alert_observer.event_called);

  factor->should_announce_label_ = true;
  test_api.UpdateState();
  EXPECT_TRUE(alert_observer.event_called);
}

TEST_F(LoginAuthFactorsViewUnittest, SingleIconInAvailableState) {
  // For test purposes only. No two auth factors should have the same type
  // ordinarily.
  AddAuthFactors({AuthFactorType::kSmartLock, AuthFactorType::kFingerprint,
                  AuthFactorType::kSmartLock});
  LoginAuthFactorsView::TestApi test_api(view_);
  auth_factors_[0]->state_ = AuthFactorState::kAvailable;
  auth_factors_[1]->state_ = AuthFactorState::kAvailable;
  auth_factors_[2]->state_ = AuthFactorState::kUnavailable;
  test_api.UpdateState();

  EXPECT_TRUE(test_api.auth_factor_icon_row()->GetVisible());
  EXPECT_FALSE(test_api.checkmark_icon()->GetVisible());
  EXPECT_FALSE(test_api.arrow_button()->GetVisible());
  EXPECT_FALSE(test_api.arrow_nudge_animation()->GetVisible());

  // The number of icons should match the number of auth factors initialized.
  EXPECT_EQ(auth_factors_.size(),
            test_api.auth_factor_icon_row()->children().size());

  // Only a single icon should be visible in the Available state.
  EXPECT_EQ(1u, GetVisibleIconCount());
}

TEST_F(LoginAuthFactorsViewUnittest, MultipleAuthFactorsInReadyState) {
  // For test purposes only. No two auth factors should have the same type
  // ordinarily.
  AddAuthFactors({AuthFactorType::kSmartLock, AuthFactorType::kFingerprint,
                  AuthFactorType::kSmartLock});
  LoginAuthFactorsView::TestApi test_api(view_);
  auth_factors_[0]->state_ = AuthFactorState::kAvailable;
  auth_factors_[1]->state_ = AuthFactorState::kReady;
  auth_factors_[2]->state_ = AuthFactorState::kReady;
  test_api.UpdateState();

  EXPECT_TRUE(test_api.auth_factor_icon_row()->GetVisible());
  EXPECT_FALSE(test_api.checkmark_icon()->GetVisible());
  EXPECT_FALSE(test_api.arrow_button()->GetVisible());
  EXPECT_FALSE(test_api.arrow_nudge_animation()->GetVisible());

  // The number of icons should match the number of auth factors initialized.
  EXPECT_EQ(auth_factors_.size(),
            test_api.auth_factor_icon_row()->children().size());

  // Only the two ready auth factors should be visible.
  EXPECT_EQ(2u, GetVisibleIconCount());

  // Check that the combined label for Smart Lock and Fingerprint is chosen.
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_AUTH_FACTOR_LABEL_UNLOCK_METHOD_SELECTION),
      test_api.label()->GetText());
}

// Note: At the moment, Smart Lock is the only auth factor that uses state
// kClickRequired (hence no similar test for Fingerprint).
TEST_F(LoginAuthFactorsViewUnittest, ClickRequired_SmartLock) {
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  AddAuthFactors({AuthFactorType::kFingerprint, AuthFactorType::kSmartLock});
  ASSERT_FALSE(ShouldHidePasswordField());

  LoginAuthFactorsView::TestApi test_api(view_);
  auth_factors_[0]->state_ = AuthFactorState::kReady;
  auth_factors_[1]->state_ = AuthFactorState::kClickRequired;
  test_api.UpdateState();

  // Allow icon time to finish drawing/painting.
  task_environment()->FastForwardBy(base::Seconds(1));

  // Check that the arrow button and arrow nudge animation is shown and that the
  // label has been updated.
  EXPECT_TRUE(test_api.arrow_button()->GetVisible());
  EXPECT_TRUE(test_api.arrow_nudge_animation()->GetVisible());
  EXPECT_FALSE(test_api.checkmark_icon()->GetVisible());
  EXPECT_FALSE(test_api.auth_factor_icon_row()->GetVisible());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTH_FACTOR_LABEL_CLICK_TO_ENTER),
            test_api.label()->GetText());
  EXPECT_TRUE(ShouldHidePasswordField());

  auth_factors_[1]->state_ = AuthFactorState::kReady;
  test_api.UpdateState();

  EXPECT_FALSE(ShouldHidePasswordField());
}

TEST_F(LoginAuthFactorsViewUnittest, ClickingArrowButton) {
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  AddAuthFactors({AuthFactorType::kFingerprint, AuthFactorType::kSmartLock});
  LoginAuthFactorsView::TestApi test_api(view_);
  auth_factors_[0]->state_ = AuthFactorState::kReady;
  auth_factors_[1]->state_ = AuthFactorState::kClickRequired;
  test_api.UpdateState();

  // Check that the arrow button and arrow nudge animation is shown.
  EXPECT_TRUE(test_api.arrow_button()->GetVisible());
  EXPECT_TRUE(test_api.arrow_nudge_animation()->GetVisible());

  // Simulate clicking arrow nudge animation, which sits on top of arrow button
  // and should relay arrow button click.
  const gfx::Point point(0, 0);
  test_api.arrow_nudge_animation()->OnMousePressed(
      ui::MouseEvent(ui::EventType::kMousePressed, point, point,
                     base::TimeTicks::Now(), 0, 0));

  // Check that arrow button is still visible and that arrow nudge animation is
  // no longer shown.
  EXPECT_TRUE(test_api.arrow_button()->GetVisible());
  EXPECT_FALSE(test_api.arrow_nudge_animation()->GetVisible());
}

// Present all possible auth factors on the lock screen, and verify behavior
// when only Fingerprint is in state kAuthenticated. When Fingerprint is in
// kAuthenticated state, it should not request to hide the password field, and
// that is its final state as the screen becomes unlocked (it doesn't
// transition to any further states).
TEST_F(LoginAuthFactorsViewUnittest, Authenticated_LockScreen_Fingerprint) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  Shell::Get()->login_screen_controller()->ShowLockScreen();

  AddAuthFactors({AuthFactorType::kSmartLock, AuthFactorType::kFingerprint});
  ASSERT_FALSE(ShouldHidePasswordField());

  LoginAuthFactorsView::TestApi test_api(view_);
  auth_factors_[0]->state_ = AuthFactorState::kReady;
  auth_factors_[1]->state_ = AuthFactorState::kAuthenticated;
  test_api.UpdateState();

  // Fingerprint should not request to hide the password field when
  // authenticated.
  VerifyAuthenticatedUiState(
      /*is_lock_screen=*/true, test_api,
      /*should_hide_password_field_when_authenticated=*/false);

  // Fingerprint does not leave the kAuthenticated state once entering it.
}

// Present all possible auth factors on the lock screen, and verify behavior
// when only Smart Lock is in state kAuthenticated.
//
// When Smart Lock is in kAuthenticated state, it should request to hide the
// password field.
//
// On the lock screen, kAuthenticated is its final state as the screen becomes
// unlocked (it doesn't transition to any further states).
TEST_F(LoginAuthFactorsViewUnittest, Authenticated_LockScreen_SmartLock) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  Shell::Get()->login_screen_controller()->ShowLockScreen();

  AddAuthFactors({AuthFactorType::kFingerprint, AuthFactorType::kSmartLock});
  ASSERT_FALSE(ShouldHidePasswordField());

  LoginAuthFactorsView::TestApi test_api(view_);
  auth_factors_[0]->state_ = AuthFactorState::kReady;
  auth_factors_[1]->state_ = AuthFactorState::kAuthenticated;
  test_api.UpdateState();

  // Smart Lock is authenticated -- it should request to hide the password
  // field.
  VerifyAuthenticatedUiState(
      /*is_lock_screen=*/true, test_api,
      /*should_hide_password_field_when_authenticated=*/true);

  // On the lock screen, Smart Lock does not leave the kAuthenticated state
  // once entering it.
}

// Present all possible auth factors on the login screen, and verify behavior
// when Smart Lock is in state kAuthenticated.
//
// At the moment, Smart Lock is the only auth factor which can be present on the
// login screen.
//
// When Smart Lock is in kAuthenticated state, it should request to hide the
// password field.
//
// On the login screen, Smart Lock may transition the kErrorPermanent
// state (if Cryptohome fails to decrypt the user directory with the
// phone-provided decryption key). When Smart Lock transitions out of
// kAuthenticated, it should no longer request to hide the password field.
TEST_F(LoginAuthFactorsViewUnittest, Authenticated_LoginScreen_SmartLock) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  Shell::Get()->login_screen_controller()->ShowLoginScreen();

  AddAuthFactors({AuthFactorType::kSmartLock});
  ASSERT_FALSE(ShouldHidePasswordField());

  LoginAuthFactorsView::TestApi test_api(view_);
  auth_factors_[0]->state_ = AuthFactorState::kAuthenticated;
  test_api.UpdateState();

  // Smart Lock is authenticated -- it should request to hide the password
  // field.
  VerifyAuthenticatedUiState(
      /*is_lock_screen=*/false, test_api,
      /*should_hide_password_field_when_authenticated=*/true);

  // Simulate Cryptohome failure. Smart Lock should no longer request to hide
  // the password field.
  auth_factors_[0]->state_ = AuthFactorState::kErrorPermanent;
  test_api.UpdateState();
  EXPECT_FALSE(ShouldHidePasswordField());
}

TEST_F(LoginAuthFactorsViewUnittest, ErrorTemporary) {
  AddAuthFactors({AuthFactorType::kFingerprint, AuthFactorType::kSmartLock});
  LoginAuthFactorsView::TestApi test_api(view_);
  auth_factors_[0]->state_ = AuthFactorState::kErrorTemporary;
  auth_factors_[1]->state_ = AuthFactorState::kReady;
  test_api.UpdateState();

  EXPECT_TRUE(test_api.auth_factor_icon_row()->GetVisible());
  EXPECT_FALSE(test_api.checkmark_icon()->GetVisible());
  EXPECT_FALSE(test_api.arrow_button()->GetVisible());
  EXPECT_FALSE(test_api.arrow_nudge_animation()->GetVisible());

  // Only the error should be visible for the first three seconds after the
  // state update.
  EXPECT_EQ(1u, GetVisibleIconCount());

  ASSERT_EQ(0, auth_factors_[0]->do_handle_error_timeout_num_calls_);
  task_environment()->FastForwardBy(kErrorTimeout);
  EXPECT_EQ(1, auth_factors_[0]->do_handle_error_timeout_num_calls_);
}

TEST_F(LoginAuthFactorsViewUnittest, ErrorPermanent) {
  AddAuthFactors({AuthFactorType::kFingerprint, AuthFactorType::kSmartLock});
  LoginAuthFactorsView::TestApi test_api(view_);
  auth_factors_[0]->state_ = AuthFactorState::kErrorPermanent;
  auth_factors_[1]->state_ = AuthFactorState::kReady;
  test_api.UpdateState();
  auto* factor = auth_factors_[0].get();

  EXPECT_TRUE(test_api.auth_factor_icon_row()->GetVisible());
  EXPECT_FALSE(test_api.checkmark_icon()->GetVisible());
  EXPECT_FALSE(test_api.arrow_button()->GetVisible());
  EXPECT_FALSE(test_api.arrow_nudge_animation()->GetVisible());

  // Only the error should be visible for the first three seconds after the
  // state update.
  EXPECT_EQ(1u, GetVisibleIconCount());

  // Fast-forward four seconds. Ensure that the OnErrorTimeout() callback gets
  // called, and |has_permanent_error_display_timed_out_| correctly reflects
  // whether the most recent timeout has expired.
  ASSERT_EQ(0, factor->do_handle_error_timeout_num_calls_);
  EXPECT_FALSE(factor->has_permanent_error_display_timed_out_);
  task_environment()->FastForwardBy(base::Seconds(4));
  EXPECT_EQ(1, factor->do_handle_error_timeout_num_calls_);
  EXPECT_TRUE(factor->has_permanent_error_display_timed_out_);

  // After timeout, permanent errors are shown alongside ready auth factors.
  test_api.UpdateState();
  EXPECT_EQ(2u, GetVisibleIconCount());

  // Simulate a click event.
  EXPECT_FALSE(factor->do_handle_tap_or_click_called_);
  factor->RefreshUI();
  const gfx::Point point(0, 0);
  factor->icon_->OnMousePressed(ui::MouseEvent(ui::EventType::kMousePressed,
                                               point, point,
                                               base::TimeTicks::Now(), 0, 0));
  EXPECT_TRUE(factor->do_handle_tap_or_click_called_);

  // Clicking causes only the error to be visible.
  test_api.UpdateState();
  EXPECT_EQ(1u, GetVisibleIconCount());

  // Fast-forward four seconds. Ensure that the OnErrorTimeout() callback gets
  // called, and |has_permanent_error_display_timed_out_| correctly reflects
  // whether the most recent timeout has expired.
  ASSERT_EQ(1, factor->do_handle_error_timeout_num_calls_);
  EXPECT_FALSE(factor->has_permanent_error_display_timed_out_);
  task_environment()->FastForwardBy(base::Seconds(4));
  EXPECT_EQ(2, factor->do_handle_error_timeout_num_calls_);
  EXPECT_TRUE(factor->has_permanent_error_display_timed_out_);

  // After timeout, permanent errors are shown alongside ready auth factors.
  test_api.UpdateState();
  EXPECT_EQ(2u, GetVisibleIconCount());
}

TEST_F(LoginAuthFactorsViewUnittest, CanUsePin) {
  AddAuthFactors({AuthFactorType::kSmartLock, AuthFactorType::kFingerprint});

  for (bool can_use_pin : {true, false}) {
    view_->SetCanUsePin(can_use_pin);
    EXPECT_EQ(can_use_pin, AuthFactorModel::can_use_pin());
    EXPECT_EQ(can_use_pin, auth_factors_[0]->can_use_pin());
    EXPECT_EQ(can_use_pin, auth_factors_[1]->can_use_pin());
  }
}

// Ensure that when Smart Lock state is kClickRequired, the arrow button
// automatically becomes focused.
TEST_F(LoginAuthFactorsViewUnittest, ArrowButtonRequestsFocus) {
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  AddAuthFactors({AuthFactorType::kFingerprint, AuthFactorType::kSmartLock});
  LoginAuthFactorsView::TestApi test_api(view_);
  auth_factors_[0]->state_ = AuthFactorState::kReady;
  auth_factors_[1]->state_ = AuthFactorState::kReady;
  test_api.UpdateState();

  // Check that there is no focus initially.
  EXPECT_FALSE(view_->GetFocusManager()->GetFocusedView());
  EXPECT_FALSE(test_api.arrow_button()->HasFocus());

  auth_factors_[1]->state_ = AuthFactorState::kClickRequired;
  test_api.UpdateState();

  // Check that the arrow button becomes focused.
  EXPECT_TRUE(view_->GetFocusManager()->GetFocusedView());
  EXPECT_TRUE(test_api.arrow_button()->HasFocus());
}

// Regression test for b/215754583.
// The arrow button automatically becomes focused when Smart Lock state is
// kClickRequired. After the state changes, the button loses visibility and the
// entire view should have its focus cleared.
TEST_F(LoginAuthFactorsViewUnittest, ArrowButtonClearsFocus_Authenticated) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  Shell::Get()->login_screen_controller()->ShowLockScreen();

  TestArrowButtonClearsFocus(
      /*state_after_click_required=*/AuthFactorState::kAuthenticated);
}

// When the state transitions from kClickRequired to kReady, kErrorTemporary
// or kErrorPermanent, the focus is also cleared. The actual experience seems
// as though the focus is not cleared and instead jumps to the password input.
// This focus happens inside LoginAuthUserView when the state change causes
// the password input to become visible.
TEST_F(LoginAuthFactorsViewUnittest, ArrowButtonClearsFocus_Ready) {
  TestArrowButtonClearsFocus(
      /*state_after_click_required=*/AuthFactorState::kReady);
}

TEST_F(LoginAuthFactorsViewUnittest, ArrowButtonClearsFocus_Error) {
  TestArrowButtonClearsFocus(
      /*state_after_click_required=*/AuthFactorState::kErrorTemporary);
  TestArrowButtonClearsFocus(
      /*state_after_click_required=*/AuthFactorState::kErrorPermanent);
}

}  // namespace ash
