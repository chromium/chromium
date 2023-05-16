// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/touch_to_fill/password_generation/android/mock_touch_to_fill_password_generation_bridge.h"
#include "chrome/browser/touch_to_fill/password_generation/android/touch_to_fill_password_generation_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/test/text_input_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"
#include "ui/base/ime/text_input_type.h"

class TouchToFillPasswordGenerationControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    password_mananger_driver_ =
        std::make_unique<password_manager::ContentPasswordManagerDriver>(
            main_rfh(), &client_, &test_autofill_client_);
  }

  base::WeakPtr<password_manager::ContentPasswordManagerDriver>
  password_mananger_driver() {
    return base::AsWeakPtr(password_mananger_driver_.get());
  }

 private:
  std::unique_ptr<password_manager::ContentPasswordManagerDriver>
      password_mananger_driver_;
  password_manager::StubPasswordManagerClient client_;
  autofill::TestAutofillClient test_autofill_client_;
};

TEST_F(TouchToFillPasswordGenerationControllerTest,
       KeyboardIsSuppressedWhileTheBottomSheetIsShown) {
  auto bridge = std::make_unique<MockTouchToFillPasswordGenerationBridge>();
  MockTouchToFillPasswordGenerationBridge* bridge_ptr = bridge.get();
  auto controller = std::make_unique<TouchToFillPasswordGenerationController>(
      password_mananger_driver(), web_contents(), std::move(bridge));
  EXPECT_CALL(*bridge_ptr, Show);
  controller->ShowTouchToFill();

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
