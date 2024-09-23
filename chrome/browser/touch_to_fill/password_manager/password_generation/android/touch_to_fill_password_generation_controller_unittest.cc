// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>

#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_manual_filling_controller.h"
#include "chrome/browser/password_manager/android/password_generation_element_data.h"
#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/mock_touch_to_fill_password_generation_bridge.h"
#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/touch_to_fill_password_generation_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/text_input_test_utils.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"
#include "ui/base/ime/text_input_type.h"

using autofill::password_generation::PasswordGenerationType;
using password_manager::metrics_util::GenerationDialogChoice;
using testing::_;
using testing::Combine;
using testing::Eq;
using testing::Values;
using ShouldShowAction = ManualFillingController::ShouldShowAction;
using TouchToFillPasswordGenerationControllerMetricsParam =
    std::tuple<PasswordGenerationType, bool, std::string>;

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
    return password_manager_driver_->AsWeakPtrImpl();
  }

  TestingPrefServiceSimple* pref_service() { return &test_pref_service_; }

  base::MockCallback<base::OnceCallback<void()>> on_dismissed_callback_;
  const std::string test_user_account_ = "test@email.com";
  MockManualFillingController mock_manual_filling_controller_;

 private:
  std::unique_ptr<password_manager::ContentPasswordManagerDriver>
      password_manager_driver_;
  password_manager::StubPasswordManagerClient client_;
  TestingPrefServiceSimple test_pref_service_;
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
  controller->ShowTouchToFill(
      test_user_account_, PasswordGenerationType::kAutomatic, pref_service());

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

  controller->ShowTouchToFill(
      test_user_account_, PasswordGenerationType::kAutomatic, pref_service());

  EXPECT_CALL(on_dismissed_callback_, Run);
  controller->OnDismissed(/*generated_password_accepted=*/false);
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

  EXPECT_CALL(*bridge_ptr, Show(_, _, _, _, Eq(test_user_account_)));
  controller->ShowTouchToFill(
      test_user_account_, PasswordGenerationType::kAutomatic, pref_service());

  EXPECT_CALL(*bridge_ptr, Hide);
  controller.reset();
}

class TouchToFillPasswordGenerationControllerMetricsTest
    : public TouchToFillPasswordGenerationControllerTest,
      public testing::WithParamInterface<
          TouchToFillPasswordGenerationControllerMetricsParam> {};

TEST_P(TouchToFillPasswordGenerationControllerMetricsTest,
       MetricsReportedWhenBottomSheetIsDismissed) {
  auto [password_generation_type, generated_password_accepted,
        expected_histogram_name] = GetParam();
  base::HistogramTester histogram_tester;

  auto controller = std::make_unique<TouchToFillPasswordGenerationController>(
      password_mananger_driver(), web_contents(),
      PasswordGenerationElementData(),
      std::make_unique<MockTouchToFillPasswordGenerationBridge>(),
      on_dismissed_callback_.Get(),
      mock_manual_filling_controller_.AsWeakPtr());

  controller->ShowTouchToFill(test_user_account_, password_generation_type,
                              pref_service());

  controller->OnDismissed(generated_password_accepted);
  histogram_tester.ExpectUniqueSample(expected_histogram_name,
                                      generated_password_accepted
                                          ? GenerationDialogChoice::kAccepted
                                          : GenerationDialogChoice::kRejected,
                                      1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TouchToFillPasswordGenerationControllerMetricsTest,
    testing::Values(
        TouchToFillPasswordGenerationControllerMetricsParam(
            PasswordGenerationType::kTouchToFill,
            /*generated_password_accepted=*/true,
            "PasswordManager.TouchToFill.PasswordGeneration.UserChoice"),
        TouchToFillPasswordGenerationControllerMetricsParam(
            PasswordGenerationType::kAutomatic,
            /*generated_password_accepted=*/false,
            "KeyboardAccessory.GenerationDialogChoice.Automatic"),
        TouchToFillPasswordGenerationControllerMetricsParam(
            PasswordGenerationType::kManual,
            /*generated_password_accepted=*/true,
            "KeyboardAccessory.GenerationDialogChoice.Manual")));
