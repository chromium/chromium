// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/mock_manual_filling_controller.h"
#include "chrome/browser/password_manager/android/password_generation_element_data.h"
#include "chrome/browser/touch_to_fill/password_generation/android/mock_touch_to_fill_password_generation_bridge.h"
#include "chrome/browser/touch_to_fill/password_generation/android/touch_to_fill_password_generation_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "content/public/test/text_input_test_utils.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"
#include "ui/base/ime/text_input_type.h"

using testing::_;
using testing::Eq;
using ShouldShowAction = ManualFillingController::ShouldShowAction;

class TouchToFillPasswordGenerationControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    password_manager_driver_ =
        std::make_unique<password_manager::ContentPasswordManagerDriver>(
            main_rfh(), &client_);
  }

  base::WeakPtr<password_manager::ContentPasswordManagerDriver>
  password_mananger_driver() {
    return base::AsWeakPtr(password_manager_driver_.get());
  }

  base::MockCallback<base::OnceCallback<void()>> on_dismissed_callback_;
  const std::string test_user_account_ = "test@email.com";
  MockManualFillingController mock_manual_filling_controller_;

 private:
  std::unique_ptr<password_manager::ContentPasswordManagerDriver>
      password_manager_driver_;
  password_manager::StubPasswordManagerClient client_;
};

TEST_F(TouchToFillPasswordGenerationControllerTest,
       KeyboardIsSuppressedWhileTheBottomSheetIsShown) {
  auto bridge = std::make_unique<MockTouchToFillPasswordGenerationBridge>();
  MockTouchToFillPasswordGenerationBridge* bridge_ptr = bridge.get();
  auto controller = std::make_unique<TouchToFillPasswordGenerationController>(
      password_mananger_driver(), web_contents(),
      PasswordGenerationElementData(), std::move(bridge),
      on_dismissed_callback_.Get(),
      mock_manual_filling_controller_.AsWeakPtr());
  EXPECT_CALL(*bridge_ptr, Show);
  controller->ShowTouchToFill(test_user_account_);

  ui::mojom::TextInputStatePtr initial_state = ui::mojom::TextInputState::New();
  initial_state->type = ui::TEXT_INPUT_TYPE_PASSWORD;
  // Simulate the TextInputStateChanged call, which triggers the keyboard.
  SendTextInputStateChangedToWidget(rvh()->GetWidget(),
                                    std::move(initial_state));
  // Keyboard is expected to be suppressed.
  EXPECT_TRUE(content::GetTextInputStateFromWebContents(web_contents())
                  ->always_hide_ime);

  controller.reset();

  initial_state = ui::mojom::TextInputState::New();
  initial_state->type = ui::TEXT_INPUT_TYPE_PASSWORD;
  // Simulate the TextInputStateChanged call, which triggers the keyboard.
  SendTextInputStateChangedToWidget(rvh()->GetWidget(),
                                    std::move(initial_state));
  // Keyboard is expected to be shown again after resetting the controller.
  EXPECT_FALSE(content::GetTextInputStateFromWebContents(web_contents())
                   ->always_hide_ime);
}

TEST_F(TouchToFillPasswordGenerationControllerTest,
       OnDismissedCallbackIsTriggeredWhenBottomSheetDismissed) {
  auto controller = std::make_unique<TouchToFillPasswordGenerationController>(
      password_mananger_driver(), web_contents(),
      PasswordGenerationElementData(),
      std::make_unique<MockTouchToFillPasswordGenerationBridge>(),
      on_dismissed_callback_.Get(),
      mock_manual_filling_controller_.AsWeakPtr());

  controller->ShowTouchToFill(test_user_account_);

  EXPECT_CALL(on_dismissed_callback_, Run);
  controller->OnDismissed();
}

TEST_F(TouchToFillPasswordGenerationControllerTest,
       CallsHideOnBridgeWhenTouchToFillPasswordGenerationControllerDestroyed) {
  auto bridge = std::make_unique<MockTouchToFillPasswordGenerationBridge>();
  MockTouchToFillPasswordGenerationBridge* bridge_ptr = bridge.get();
  auto controller = std::make_unique<TouchToFillPasswordGenerationController>(
      password_mananger_driver(), web_contents(),
      PasswordGenerationElementData(), std::move(bridge),
      on_dismissed_callback_.Get(),
      mock_manual_filling_controller_.AsWeakPtr());

  EXPECT_CALL(*bridge_ptr, Show(_, _, _, Eq(test_user_account_)));
  controller->ShowTouchToFill(test_user_account_);

  EXPECT_CALL(*bridge_ptr, Hide);
  controller.reset();
}

TEST_F(TouchToFillPasswordGenerationControllerTest,
       TriggersKeyboardAccessoryWhenGeneratedPasswordRejected) {
  auto controller = std::make_unique<TouchToFillPasswordGenerationController>(
      password_mananger_driver(), web_contents(),
      PasswordGenerationElementData(),
      std::make_unique<MockTouchToFillPasswordGenerationBridge>(),
      on_dismissed_callback_.Get(),
      mock_manual_filling_controller_.AsWeakPtr());

  controller->ShowTouchToFill(test_user_account_);

  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  ShouldShowAction(true),
                  autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC));
  controller->OnGeneratedPasswordRejected();
}
