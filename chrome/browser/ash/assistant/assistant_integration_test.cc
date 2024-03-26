// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_element_identifiers.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/test/assistant_test_api.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/test/base/chromeos/crosier/ash_integration_test.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

// Assistant requires Gaia login, which is only supported for branded build.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {

using ::ash::assistant::AssistantStatus;

// Waiter that waits for the assistant to be ready.
class AssistantReadyWaiter : private AssistantStateObserver {
 public:
  explicit AssistantReadyWaiter(AssistantState* state) : state_(state) {
    state_->AddObserver(this);
  }

  ~AssistantReadyWaiter() override { state_->RemoveObserver(this); }

  void WaitForReady() {
    if (state_->assistant_status() == AssistantStatus::READY) {
      return;
    }

    // Wait until we're ready or we hit the timeout.
    run_loop_ = std::make_unique<base::RunLoop>();
    EXPECT_NO_FATAL_FAILURE(run_loop_->Run())
        << "Failed waiting for assistant to be ready. Current status is "
        << state_->assistant_status() << ". ";
    run_loop_.reset();
  }

 private:
  void OnAssistantStatusChanged(AssistantStatus status) override {
    if (status == AssistantStatus::READY && run_loop_) {
      run_loop_->Quit();
    }
  }

 private:
  const raw_ptr<AssistantState> state_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

// The suite is parameterized by which hotkey is being tested (the Assistant
// keyboard key or Search-A).
class AssistantIntegrationTest : public AshIntegrationTest,
                                 public testing::WithParamInterface<bool> {
 public:
  AssistantIntegrationTest() {
    set_exit_when_last_browser_closes(false);

    // Allows network access for production Gaia.
    SetAllowNetworkAccessToHostResolutions();

    // The assistant requires Gaia login.
    login_mixin().SetMode(ChromeOSIntegrationLoginMixin::Mode::kGaiaLogin);
  }
};

INSTANTIATE_TEST_SUITE_P(All, AssistantIntegrationTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(AssistantIntegrationTest, Hotkey) {
  SetupContextWidget();

  // Login and wait for the user session to start.
  login_mixin().Login();
  test::WaitForPrimaryUserSessionStart();

  // Enable the assistant.
  std::unique_ptr<AssistantTestApi> test_api = AssistantTestApi::Create();
  test_api->SetAssistantEnabled(true);
  test_api->DisableAnimations();

  // Wait for the assistant to be ready.
  AssistantReadyWaiter waiter(test_api->GetAssistantState());
  waiter.WaitForReady();

  RunTestSequence(
      // clang-format off
      Log("Pressing accelerator to open assistant"),
      Do([] {
        if (GetParam()) {
          // Test the Assistant key.
          ui_controls::SendKeyPress(/*window=*/nullptr, ui::VKEY_ASSISTANT,
                                    /*control=*/false, /*shift=*/false,
                                    /*alt=*/false, /*command=*/false);
        } else {
          // Test Search-A.
          ui_controls::SendKeyPress(/*window=*/nullptr, ui::VKEY_A,
                                    /*control=*/false, /*shift=*/false,
                                    /*alt=*/false, /*command=*/true);
        }
      }),
      Log("Waiting for assistant dialog plate to show"),
      WaitForShow(ash::kAssistantDialogPlateElementId),
      Log("Test complete")
      // clang-format on
  );
}

}  // namespace ash

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
