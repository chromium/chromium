// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/security_token_request_controller.h"

#include <memory>

#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/pin_request_view.h"
#include "ash/login/ui/pin_request_widget.h"
#include "ash/public/cpp/login_types.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/button_test_api.h"

namespace ash {

class SecurityTokenRequestControllerTest : public LoginTestBase {
 protected:
  SecurityTokenRequestControllerTest() = default;
  SecurityTokenRequestControllerTest(
      const SecurityTokenRequestControllerTest&) = delete;
  SecurityTokenRequestControllerTest& operator=(
      const SecurityTokenRequestControllerTest&) = delete;
  ~SecurityTokenRequestControllerTest() override = default;

  // LoginScreenTest:
  void SetUp() override {
    LoginTestBase::SetUp();
    controller_ = std::make_unique<SecurityTokenRequestController>();
  }

  void TearDown() override {
    controller_.reset();
    LoginTestBase::TearDown();
  }

  // Simulates mouse press event on a |button|.
  void SimulateButtonPress(views::Button* button) {
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                         gfx::Point(), ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi(button).NotifyClick(event);
  }

  void StartRequest(int attempts_left = -1) {
    SecurityTokenPinRequest request;
    request.pin_entered_callback =
        base::BindOnce(&SecurityTokenRequestControllerTest::OnPinEntered,
                       base::Unretained(this));
    request.pin_ui_closed_callback =
        base::BindOnce(&SecurityTokenRequestControllerTest::OnUiClosedByUser,
                       base::Unretained(this));
    request.attempts_left = attempts_left;
    request.enable_user_input = attempts_left != 0;
    controller_->SetPinUiState(std::move(request));
    EXPECT_TRUE(PinRequestWidget::Get());
    view_ =
        PinRequestWidget::TestApi(PinRequestWidget::Get()).pin_request_view();
  }

  // Simulates entering a PIN (012345).
  void SimulateValidation() {
    ui::test::EventGenerator* generator = GetEventGenerator();
    for (int i = 0; i < 6; ++i) {
      generator->PressKey(ui::KeyboardCode(ui::KeyboardCode::VKEY_0 + i),
                          ui::EF_NONE);
    }
    if (PinRequestView::TestApi(view_).submit_button()->GetEnabled()) {
      SimulateButtonPress(PinRequestView::TestApi(view_).submit_button());
    }
  }

  std::unique_ptr<SecurityTokenRequestController> controller_;

  // Number of times a PIN was entered
  int pin_entered_calls_ = 0;

  // Number of times the UI was closed.
  int ui_closed_by_user_calls_ = 0;

  raw_ptr<PinRequestView, DanglingUntriaged> view_ =
      nullptr;  // Owned by test widget view hierarchy.

 private:
  void OnPinEntered(const std::string& user_input) { ++pin_entered_calls_; }
  void OnUiClosedByUser() { ++ui_closed_by_user_calls_; }
};

// Tests successful PIN validation flow.
TEST_F(SecurityTokenRequestControllerTest, SecurityTokenSuccessfulValidation) {
  StartRequest();
  SimulateValidation();
  EXPECT_EQ(1, pin_entered_calls_);
  controller_->ClosePinUi();
  EXPECT_FALSE(PinRequestWidget::Get());
}

// Tests unsuccessful PIN flow, including cancelling the request.
TEST_F(SecurityTokenRequestControllerTest,
       SecurityTokenUnsuccessfulValidation) {
  StartRequest();
  SimulateValidation();
  EXPECT_EQ(1, pin_entered_calls_);
  EXPECT_TRUE(PinRequestWidget::Get());

  // Simulate wrong PIN response.
  StartRequest(/*attempts_left=*/1);
  SimulateValidation();
  EXPECT_EQ(2, pin_entered_calls_);
  EXPECT_TRUE(PinRequestWidget::Get());

  // Wrong PIN again. UI should not allow PIN input/submission when there are no
  // attempts left.
  StartRequest(/*attempts_left=*/0);
  SimulateValidation();
  EXPECT_EQ(2, pin_entered_calls_);
  EXPECT_TRUE(PinRequestWidget::Get());

  // User should still be able to close the PIN widget.
  SimulateButtonPress(PinRequestView::TestApi(view_).back_button());
  EXPECT_EQ(1, ui_closed_by_user_calls_);
  EXPECT_FALSE(PinRequestWidget::Get());
}

}  // namespace ash
