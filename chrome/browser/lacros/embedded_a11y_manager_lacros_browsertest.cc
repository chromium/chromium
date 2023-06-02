// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lacros/embedded_a11y_manager_lacros.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-shared.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests for EmbeddedA11yManagerLacros, ensuring it can install
// the correct accessibility helper extensions on all the profiles
// and responds to the state of the ash accessibility prefs.
//
// NOTE: Tests in this file modify Ash accessibility features. That is
// potentially a lasting side effect that can affect other tests.
// * To prevent interference with tests that are run in parallel, these tests
// are a part of lacros_chrome_browsertests_run_in_series test suite.
// * To prevent interference with following tests, they try to clean up all the
// side effects themselves, e.g. if a test sets a pref, it is also responsible
// for unsetting it.

namespace {

using AssistiveTechnologyType = crosapi::mojom::AssistiveTechnologyType;

bool IsEnabled(AssistiveTechnologyType at_type, bool enabled) {
  auto* manager = EmbeddedA11yManagerLacros::GetInstance();
  switch (at_type) {
    case AssistiveTechnologyType::kChromeVox:
      return manager->chromevox_enabled() == enabled;
    case AssistiveTechnologyType::kSelectToSpeak:
      return manager->select_to_speak_enabled() == enabled;
    case AssistiveTechnologyType::kSwitchAccess:
      return manager->switch_access_enabled() == enabled;
    case AssistiveTechnologyType::kUnknown:
      return false;
  }
}

}  // namespace

class EmbeddedA11yManagerLacrosTest : public InProcessBrowserTest {
 public:
  EmbeddedA11yManagerLacrosTest() = default;
  ~EmbeddedA11yManagerLacrosTest() override = default;
  EmbeddedA11yManagerLacrosTest(const EmbeddedA11yManagerLacrosTest&) = delete;
  EmbeddedA11yManagerLacrosTest& operator=(
      const EmbeddedA11yManagerLacrosTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    EmbeddedA11yManagerLacros::GetInstance()->AddPrefChangedCallbackForTest(
        base::BindRepeating(&EmbeddedA11yManagerLacrosTest::OnPrefChanged,
                            base::Unretained(this)));

    auto* lacros_service = chromeos::LacrosService::Get();
    if (!lacros_service ||
        !lacros_service->IsAvailable<crosapi::mojom::TestController>() ||
        lacros_service->GetInterfaceVersion<crosapi::mojom::TestController>() <
            static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                                 kSetAssistiveTechnologyEnabledMinVersion)) {
      GTEST_SKIP() << "Ash version doesn't have required test API";
    }
  }

  void OnPrefChanged() {
    if (waiter_ && waiter_->running()) {
      waiter_->Quit();
    }
  }

  void SetFeatureEnabled(AssistiveTechnologyType at_type, bool enabled) {
    chromeos::LacrosService::Get()
        ->GetRemote<crosapi::mojom::TestController>()
        ->SetAssistiveTechnologyEnabled(at_type, enabled);
  }

  void WaitForFeatureEnabled(AssistiveTechnologyType at_type, bool enabled) {
    while (!IsEnabled(at_type, enabled)) {
      waiter_ = std::make_unique<base::RunLoop>();
      waiter_->Run();
    }
  }

 private:
  std::unique_ptr<base::RunLoop> waiter_;
};

// Tests for changing the Ash feature getting noticed by
// EmbeddedA11yManagerLacros. This also provides test coverage for
// TestControllerAsh changing a11y features.

IN_PROC_BROWSER_TEST_F(EmbeddedA11yManagerLacrosTest, ListensForChromevoxPref) {
  EmbeddedA11yManagerLacros::GetInstance()->Init();
  SetFeatureEnabled(AssistiveTechnologyType::kChromeVox, true);
  WaitForFeatureEnabled(AssistiveTechnologyType::kChromeVox, true);
  SetFeatureEnabled(AssistiveTechnologyType::kChromeVox, false);
  WaitForFeatureEnabled(AssistiveTechnologyType::kChromeVox, false);
}

IN_PROC_BROWSER_TEST_F(EmbeddedA11yManagerLacrosTest,
                       AddsAndRemovesHelperForSelectToSpeak) {
  EmbeddedA11yManagerLacros::GetInstance()->Init();
  SetFeatureEnabled(AssistiveTechnologyType::kSelectToSpeak, true);
  WaitForFeatureEnabled(AssistiveTechnologyType::kSelectToSpeak, true);
  SetFeatureEnabled(AssistiveTechnologyType::kSelectToSpeak, false);
  WaitForFeatureEnabled(AssistiveTechnologyType::kSelectToSpeak, false);
}

IN_PROC_BROWSER_TEST_F(EmbeddedA11yManagerLacrosTest,
                       AddsAndRemovesHelperForSwitchAccess) {
  EmbeddedA11yManagerLacros::GetInstance()->Init();
  SetFeatureEnabled(AssistiveTechnologyType::kSwitchAccess, true);
  WaitForFeatureEnabled(AssistiveTechnologyType::kSwitchAccess, true);
  SetFeatureEnabled(AssistiveTechnologyType::kSwitchAccess, false);
  WaitForFeatureEnabled(AssistiveTechnologyType::kSwitchAccess, false);
}
