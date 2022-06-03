// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_auth_factors_view.h"

#include "ash/constants/ash_features.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/auth_factor_model.h"
#include "ash/login/ui/auth_icon_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/ax_event_observer.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

using AuthFactorState = AuthFactorModel::AuthFactorState;

class FakeAuthFactorModel : public AuthFactorModel {
 public:
  explicit FakeAuthFactorModel(AuthFactorType type) : type_(type) {}

  FakeAuthFactorModel(const FakeAuthFactorModel&) = delete;
  FakeAuthFactorModel& operator=(const FakeAuthFactorModel&) = delete;
  ~FakeAuthFactorModel() override = default;

  AuthFactorState GetAuthFactorState() override { return state_; }

  AuthFactorType GetType() override { return type_; }

  int GetLabelId() override { return IDS_SMART_LOCK_LABEL_LOOKING_FOR_PHONE; }

  bool ShouldAnnounceLabel() override { return should_announce_label_; }

  int GetAccessibleNameId() override {
    return IDS_SMART_LOCK_LABEL_LOOKING_FOR_PHONE;
  }

  void OnTapOrClickEvent() override { on_tap_or_click_event_called_ = true; }

  void UpdateIcon(AuthIconView* icon) override {
    ASSERT_TRUE(icon);
    icon_ = icon;
  }

  using AuthFactorModel::NotifyOnStateChanged;

  AuthFactorType type_;
  AuthFactorState state_ = AuthFactorState::kReady;
  AuthIconView* icon_ = nullptr;
  bool on_tap_or_click_event_called_ = false;
  bool should_announce_label_ = false;
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

  views::View* view_;
  ax::mojom::Event event_type_;
};

}  // namespace

class LoginAuthFactorsViewUnittest : public LoginTestBase {
 public:
  LoginAuthFactorsViewUnittest(const LoginAuthFactorsViewUnittest&) = delete;
  LoginAuthFactorsViewUnittest& operator=(const LoginAuthFactorsViewUnittest&) =
      delete;

 protected:
  LoginAuthFactorsViewUnittest() {
    feature_list_.InitAndEnableFeature(features::kSmartLockUIRevamp);
  }

  ~LoginAuthFactorsViewUnittest() override = default;

  // LoginTestBase:
  void SetUp() override {
    LoginTestBase::SetUp();
    user_ = CreateUser("user@domain.com");

    view_ = new LoginAuthFactorsView(base::BindRepeating(
        &LoginAuthFactorsViewUnittest::set_click_to_enter_called,
        base::Unretained(this), true));

    // We proxy |view_| inside of |container_| so we can control layout.
    // TODO(crbug.com/1233614): Add layout tests to check positioning/ordering
    // of icons.
    container_ = new views::View();
    container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    container_->AddChildView(view_);
    SetWidget(CreateWidgetWithContent(container_));
  }

  void AddAuthFactors(std::vector<AuthFactorType> types) {
    for (AuthFactorType type : types) {
      auto auth_factor = std::make_unique<FakeAuthFactorModel>(type);
      auth_factors_.push_back(auth_factor.get());
      view_->AddAuthFactor(std::move(auth_factor));
    }
  }

  void set_click_to_enter_called(bool called) {
    click_to_enter_called_ = called;
  }

  base::test::ScopedFeatureList feature_list_;
  LoginUserInfo user_;
  views::View* container_ = nullptr;  // Owned by test widget view hierarchy.
  LoginAuthFactorsView* view_ =
      nullptr;  // Owned by test widget view hierarchy.
  std::vector<FakeAuthFactorModel*> auth_factors_;
  bool click_to_enter_called_ = false;
};

TEST_F(LoginAuthFactorsViewUnittest, NotVisibleIfNoAuthFactors) {
  AddAuthFactors({AuthFactorType::kFingerprint, AuthFactorType::kSmartLock});
  EXPECT_TRUE(view_->GetVisible());

  LoginAuthFactorsView::TestApi test_api(view_);
  auto& auth_factors = test_api.auth_factors();
  auth_factors.clear();
  test_api.UpdateState();

  EXPECT_FALSE(view_->GetVisible());
}

TEST_F(LoginAuthFactorsViewUnittest, NotVisibleIfAuthFactorsUnavailable) {
  AddAuthFactors({AuthFactorType::kFingerprint, AuthFactorType::kSmartLock});
  EXPECT_TRUE(view_->GetVisible());

  for (auto* factor : auth_factors_) {
    factor->state_ = AuthFactorState::kUnavailable;
  }
  LoginAuthFactorsView::TestApi test_api(view_);
  test_api.UpdateState();

  EXPECT_FALSE(view_->GetVisible());
}

TEST_F(LoginAuthFactorsViewUnittest, TapOrClickCalled) {
  AddAuthFactors({AuthFactorType::kFingerprint, AuthFactorType::kSmartLock});
  auto* factor = auth_factors_[0];

  // NotifyOnStateChanged() calls UpdateIcon(), which captures a pointer to the
  // icon.
  factor->NotifyOnStateChanged();

  EXPECT_FALSE(factor->on_tap_or_click_event_called_);
  const gfx::Point point(0, 0);
  factor->icon_->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, point, point, base::TimeTicks::Now(), 0, 0));
  EXPECT_TRUE(factor->on_tap_or_click_event_called_);
}

TEST_F(LoginAuthFactorsViewUnittest, ShouldAnnounceLabel) {
  AddAuthFactors({AuthFactorType::kFingerprint, AuthFactorType::kSmartLock});
  auto* factor = auth_factors_[0];
  LoginAuthFactorsView::TestApi test_api(view_);
  views::Label* label = test_api.label();
  ScopedAXEventObserver alert_observer(label, ax::mojom::Event::kAlert);
  for (auto* factor : auth_factors_) {
    factor->state_ = AuthFactorState::kAvailable;
  }

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

  EXPECT_EQ(true, test_api.auth_factor_icon_row()->GetVisible());
  EXPECT_EQ(false, test_api.checkmark_icon()->GetVisible());
  EXPECT_EQ(false, test_api.arrow_button()->GetVisible());

  // The number of icons should match the number of auth factors initialized.
  EXPECT_EQ(auth_factors_.size(),
            test_api.auth_factor_icon_row()->children().size());

  // Only a single icon should be visible in the Available state.
  size_t visible_icon_count = 0;
  for (auto* icon : test_api.auth_factor_icon_row()->children()) {
    if (icon->GetVisible()) {
      visible_icon_count++;
    }
  }
  EXPECT_EQ(1u, visible_icon_count);
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

  EXPECT_EQ(true, test_api.auth_factor_icon_row()->GetVisible());
  EXPECT_EQ(false, test_api.checkmark_icon()->GetVisible());
  EXPECT_EQ(false, test_api.arrow_button()->GetVisible());

  // The number of icons should match the number of auth factors initialized.
  EXPECT_EQ(auth_factors_.size(),
            test_api.auth_factor_icon_row()->children().size());

  // Only the two ready auth factors should be visible.
  size_t visible_icon_count = 0;
  for (auto* icon : test_api.auth_factor_icon_row()->children()) {
    if (icon->GetVisible()) {
      visible_icon_count++;
    }
  }
  EXPECT_EQ(2u, visible_icon_count);

  // Check that the combined label for Smart Lock and Fingerprint is chosen.
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_AUTH_FACTOR_LABEL_UNLOCK_METHOD_SELECTION),
      test_api.label()->GetText());
}

TEST_F(LoginAuthFactorsViewUnittest, ClickRequired) {
  AddAuthFactors({AuthFactorType::kFingerprint, AuthFactorType::kSmartLock});
  LoginAuthFactorsView::TestApi test_api(view_);
  auth_factors_[0]->state_ = AuthFactorState::kReady;
  auth_factors_[1]->state_ = AuthFactorState::kClickRequired;
  test_api.UpdateState();

  // Check that only the arrow button is shown and that the label has been
  // updated.
  EXPECT_EQ(true, test_api.arrow_button()->GetVisible());
  EXPECT_EQ(false, test_api.checkmark_icon()->GetVisible());
  EXPECT_EQ(false, test_api.auth_factor_icon_row()->GetVisible());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTH_FACTOR_LABEL_CLICK_TO_ENTER),
            test_api.label()->GetText());
}

TEST_F(LoginAuthFactorsViewUnittest, Authenticated) {
  AddAuthFactors({AuthFactorType::kFingerprint, AuthFactorType::kSmartLock});
  LoginAuthFactorsView::TestApi test_api(view_);
  auth_factors_[0]->state_ = AuthFactorState::kAuthenticated;
  auth_factors_[1]->state_ = AuthFactorState::kClickRequired;
  test_api.UpdateState();

  // Check that only the arrow button is shown and that the label has been
  // updated.
  EXPECT_EQ(true, test_api.checkmark_icon()->GetVisible());
  EXPECT_EQ(false, test_api.arrow_button()->GetVisible());
  EXPECT_EQ(false, test_api.auth_factor_icon_row()->GetVisible());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTH_FACTOR_LABEL_UNLOCKED),
            test_api.label()->GetText());
}

// TODO(crbug.com/1233614): Test adding multiple auth factors and switching
// between them once the functionality has been implemented.

}  // namespace ash
