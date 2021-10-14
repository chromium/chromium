// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_auth_factors_view.h"

#include "ash/constants/ash_features.h"
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

  void UpdateIcon(AuthIconView* icon_view) override {
    EXPECT_TRUE(icon_view);
    icon_view_ = icon_view;
  }

  void OnTapOrClickEvent() override { on_tap_or_click_event_called_ = true; }

  void set_auth_factor_state(AuthFactorState state) { state_ = state; }

  void set_should_announce_label(bool should_announce_label) {
    should_announce_label_ = should_announce_label;
  }

  AuthFactorType type_;
  AuthFactorState state_ = AuthFactorState::kReady;
  bool on_tap_or_click_event_called_ = false;
  bool should_announce_label_ = false;
  AuthIconView* icon_view_ = nullptr;
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
    AddAuthFactor();

    // We proxy |view_| inside of |container_| so we can control layout.
    // TODO(crbug.com/1233614): Add layout tests to check positioning/ordering
    // of icons.
    container_ = new views::View();
    container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    container_->AddChildView(view_);
    SetWidget(CreateWidgetWithContent(container_));
  }

  void AddAuthFactor() {
    auto auth_factor =
        std::make_unique<FakeAuthFactorModel>(AuthFactorType::kFingerprint);
    auth_factors_.push_back(auth_factor.get());
    view_->AddAuthFactor(std::move(auth_factor));
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
  EXPECT_TRUE(view_->GetVisible());

  LoginAuthFactorsView::TestApi test_api(view_);
  auto& auth_factors = test_api.auth_factors();
  auth_factors.clear();
  test_api.UpdateState();

  EXPECT_FALSE(view_->GetVisible());
}

TEST_F(LoginAuthFactorsViewUnittest, NotVisibleIfAuthFactorsUnavailable) {
  EXPECT_TRUE(view_->GetVisible());

  for (auto* factor : auth_factors_) {
    factor->set_auth_factor_state(AuthFactorState::kUnavailable);
  }
  LoginAuthFactorsView::TestApi test_api(view_);
  test_api.UpdateState();

  EXPECT_FALSE(view_->GetVisible());
}

TEST_F(LoginAuthFactorsViewUnittest, TapOrClickCalled) {
  LoginAuthFactorsView::TestApi test_api(view_);
  test_api.UpdateState();
  auto* factor = auth_factors_[0];

  EXPECT_FALSE(factor->on_tap_or_click_event_called_);
  const gfx::Point point(0, 0);
  factor->icon_view_->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, point, point, base::TimeTicks::Now(), 0, 0));
  EXPECT_TRUE(factor->on_tap_or_click_event_called_);
}

TEST_F(LoginAuthFactorsViewUnittest, ShouldAnnounceLabel) {
  auto* factor = auth_factors_[0];
  LoginAuthFactorsView::TestApi test_api(view_);
  views::Label* label = test_api.label();
  ScopedAXEventObserver alert_observer(label, ax::mojom::Event::kAlert);

  ASSERT_FALSE(factor->ShouldAnnounceLabel());
  ASSERT_FALSE(alert_observer.event_called);

  test_api.UpdateState();
  ASSERT_FALSE(alert_observer.event_called);

  factor->set_should_announce_label(true);
  test_api.UpdateState();
  EXPECT_TRUE(alert_observer.event_called);
}

// TODO(crbug.com/1233614): Test adding multiple auth factors and switching
// between them once the functionality has been implemented.

}  // namespace ash
