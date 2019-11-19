// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_controller.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "ash/system/unified/notification_hidden_view.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/network/network_handler.h"
#include "components/prefs/testing_pref_service.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/view_observer.h"

namespace ash {

namespace {

void SetSessionState(const session_manager::SessionState& state) {
  SessionInfo info;
  info.state = state;
  Shell::Get()->session_controller()->SetSessionInfo(info);
}

}  // anonymous namespace

class UnifiedSystemTrayControllerTest : public AshTestBase,
                                        public views::ViewObserver {
 public:
  UnifiedSystemTrayControllerTest() = default;
  ~UnifiedSystemTrayControllerTest() override = default;

  // testing::Test:
  void SetUp() override {
    chromeos::shill_clients::InitializeFakes();
    // Initializing NetworkHandler before ash is more like production.
    chromeos::NetworkHandler::Initialize();
    AshTestBase::SetUp();
    chromeos::NetworkHandler::Get()->InitializePrefServices(&profile_prefs_,
                                                            &local_state_);
    // Networking stubs may have asynchronous initialization.
    base::RunLoop().RunUntilIdle();

    model_ = std::make_unique<UnifiedSystemTrayModel>(nullptr);
    controller_ = std::make_unique<UnifiedSystemTrayController>(model());
  }

  void TearDown() override {
    DCHECK(view_) << "Must call InitializeView() during the tests";

    view_->RemoveObserver(this);

    view_.reset();
    controller_.reset();
    model_.reset();

    // This roughly matches production shutdown order.
    chromeos::NetworkHandler::Get()->ShutdownPrefServices();
    AshTestBase::TearDown();
    chromeos::NetworkHandler::Shutdown();
    chromeos::shill_clients::Shutdown();
  }

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* observed_view) override {
    view_->SetBoundsRect(gfx::Rect(view_->GetPreferredSize()));
    view_->Layout();
    ++preferred_size_changed_count_;
  }

 protected:
  void WaitForAnimation() {
    while (controller()->animation_->is_animating())
      base::RunLoop().RunUntilIdle();
  }

  int preferred_size_changed_count() const {
    return preferred_size_changed_count_;
  }

  void InitializeView() {
    view_.reset(controller_->CreateView());

    view_->AddObserver(this);
    OnViewPreferredSizeChanged(view());

    preferred_size_changed_count_ = 0;
  }

  UnifiedSystemTrayModel* model() { return model_.get(); }
  UnifiedSystemTrayController* controller() { return controller_.get(); }
  UnifiedSystemTrayView* view() { return view_.get(); }

 private:
  std::unique_ptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<UnifiedSystemTrayController> controller_;
  std::unique_ptr<UnifiedSystemTrayView> view_;

  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple local_state_;

  int preferred_size_changed_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(UnifiedSystemTrayControllerTest);
};

TEST_F(UnifiedSystemTrayControllerTest, ToggleExpanded) {
  InitializeView();

  EXPECT_TRUE(model()->IsExpandedOnOpen());
  const int expanded_height = view()->GetPreferredSize().height();

  controller()->ToggleExpanded();
  WaitForAnimation();

  const int collapsed_height = view()->GetPreferredSize().height();
  EXPECT_LT(collapsed_height, expanded_height);
  EXPECT_FALSE(model()->IsExpandedOnOpen());

  EXPECT_EQ(expanded_height, view()->GetExpandedSystemTrayHeight());
}

TEST_F(UnifiedSystemTrayControllerTest, EnsureExpanded_UserChooserShown) {
  InitializeView();
  EXPECT_FALSE(view()->detailed_view_for_testing()->GetVisible());

  // Show the user chooser view.
  controller()->ShowUserChooserView();
  EXPECT_TRUE(view()->detailed_view_for_testing()->GetVisible());

  // Calling EnsureExpanded() should hide the detailed view (e.g. this can
  // happen when changing the brightness or volume).
  controller()->EnsureExpanded();
  EXPECT_FALSE(view()->detailed_view_for_testing()->GetVisible());
}

TEST_F(UnifiedSystemTrayControllerTest, PreferredSizeChanged) {
  InitializeView();

  // Checks PreferredSizeChanged is not called too frequently.
  EXPECT_EQ(0, preferred_size_changed_count());
  view()->SetExpandedAmount(0.0);
  EXPECT_EQ(1, preferred_size_changed_count());
  view()->SetExpandedAmount(0.25);
  EXPECT_EQ(2, preferred_size_changed_count());
  view()->SetExpandedAmount(0.75);
  EXPECT_EQ(3, preferred_size_changed_count());
  view()->SetExpandedAmount(1.0);
  EXPECT_EQ(4, preferred_size_changed_count());
}

TEST_F(UnifiedSystemTrayControllerTest, NotificationHiddenView_ModeShow) {
  AshMessageCenterLockScreenController::OverrideModeForTest(
      AshMessageCenterLockScreenController::Mode::SHOW);
  SetSessionState(session_manager::SessionState::LOCKED);
  InitializeView();

  EXPECT_TRUE(AshMessageCenterLockScreenController::IsAllowed());
  EXPECT_TRUE(AshMessageCenterLockScreenController::IsEnabled());
  EXPECT_FALSE(view()->notification_hidden_view_for_testing()->GetVisible());
}

TEST_F(UnifiedSystemTrayControllerTest, NotificationHiddenView_ModeHide) {
  AshMessageCenterLockScreenController::OverrideModeForTest(
      AshMessageCenterLockScreenController::Mode::HIDE);
  SetSessionState(session_manager::SessionState::LOCKED);
  InitializeView();

  EXPECT_TRUE(AshMessageCenterLockScreenController::IsAllowed());
  EXPECT_FALSE(AshMessageCenterLockScreenController::IsEnabled());
  EXPECT_TRUE(view()->notification_hidden_view_for_testing()->GetVisible());
  EXPECT_NE(nullptr, view()
                         ->notification_hidden_view_for_testing()
                         ->change_button_for_testing());
}

TEST_F(UnifiedSystemTrayControllerTest,
       NotificationHiddenView_ModeHideSensitive) {
  AshMessageCenterLockScreenController::OverrideModeForTest(
      AshMessageCenterLockScreenController::Mode::HIDE_SENSITIVE);
  SetSessionState(session_manager::SessionState::LOCKED);
  InitializeView();

  EXPECT_TRUE(AshMessageCenterLockScreenController::IsAllowed());
  EXPECT_TRUE(AshMessageCenterLockScreenController::IsEnabled());
  EXPECT_FALSE(view()->notification_hidden_view_for_testing()->GetVisible());
}

TEST_F(UnifiedSystemTrayControllerTest, NotificationHiddenView_ModeProhibited) {
  AshMessageCenterLockScreenController::OverrideModeForTest(
      AshMessageCenterLockScreenController::Mode::PROHIBITED);
  SetSessionState(session_manager::SessionState::LOCKED);
  InitializeView();

  EXPECT_FALSE(AshMessageCenterLockScreenController::IsAllowed());
  EXPECT_FALSE(AshMessageCenterLockScreenController::IsEnabled());
  EXPECT_TRUE(view()->notification_hidden_view_for_testing()->GetVisible());
  EXPECT_EQ(nullptr, view()
                         ->notification_hidden_view_for_testing()
                         ->change_button_for_testing());
}

}  // namespace ash
