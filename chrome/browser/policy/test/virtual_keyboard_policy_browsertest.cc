// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "build/build_config.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/accessibility/magnifier_type.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/browser.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/base/ime/input_method.h"

namespace policy {
namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

ChromeKeyboardControllerClient* GetEnabledKeyboardClient() {
  auto* client = ChromeKeyboardControllerClient::Get();
  if (client != nullptr) {
    client->SetEnableFlag(keyboard::KeyboardEnableFlag::kTouchEnabled);
  }
  return client;
}

ui::InputMethod* GetInputMethod(ChromeKeyboardControllerClient* client) {
  aura::Window* root_window = client->GetKeyboardWindow()->GetRootWindow();
  return root_window != nullptr ? root_window->GetHost()->GetInputMethod()
                                : nullptr;
}

void Wait(base::TimeDelta timeout) {
  base::RunLoop run_loop;
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, timeout, run_loop.QuitClosure());
  run_loop.Run();
  timer.Stop();
}

ui::FakeTextInputClient::Options WebTextFieldOptions() {
  return {
      .type = ui::TEXT_INPUT_TYPE_TEXT,
      .mode = ui::TEXT_INPUT_MODE_TEXT,
      .flags = ui::TEXT_INPUT_FLAG_SPELLCHECK_ON,
  };
}

class KeyboardVisibilityWaiter
    : public ChromeKeyboardControllerClient::Observer {
 public:
  explicit KeyboardVisibilityWaiter(ChromeKeyboardControllerClient* client,
                                    bool visible)
      : visible_(visible) {
    observer_.Observe(client);
  }
  KeyboardVisibilityWaiter(const KeyboardVisibilityWaiter&) = delete;
  KeyboardVisibilityWaiter& operator=(const KeyboardVisibilityWaiter&) = delete;
  ~KeyboardVisibilityWaiter() override = default;

  void Wait() {
    if (observer_.GetSource()->is_keyboard_visible() != visible_) {
      run_loop_.Run();
    }
  }

  // ChromeKeyboardControllerClient::Observer
  void OnKeyboardVisibilityChanged(bool visible) override {
    if (visible == visible_) {
      run_loop_.Quit();
    }
  }

 private:
  bool visible_;
  base::ScopedObservation<ChromeKeyboardControllerClient,
                          ChromeKeyboardControllerClient::Observer>
      observer_{this};
  base::RunLoop run_loop_;
};

void WaitUntilKeyboardShown(ChromeKeyboardControllerClient* client) {
  KeyboardVisibilityWaiter(client, true).Wait();
}

void WaitUntilKeyboardHidden(ChromeKeyboardControllerClient* client) {
  KeyboardVisibilityWaiter(client, false).Wait();
}

#endif

}  // namespace

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

class VirtualKeyboardPolicyTest : public PolicyTest {};

IN_PROC_BROWSER_TEST_F(VirtualKeyboardPolicyTest,
                       VirtualKeyboardSmartVisibilityEnabledDefault) {
  auto* keyboard_client = GetEnabledKeyboardClient();
  ASSERT_TRUE(keyboard_client);
  ui::InputMethod* input_method = GetInputMethod(keyboard_client);
  ASSERT_TRUE(input_method);
  ui::FakeTextInputClient text_client(input_method, WebTextFieldOptions());

  // Showing the virtual keyboard, losing focus, then focusing again should show
  // the virtual keyboard again.
  text_client.Focus();
  input_method->SetVirtualKeyboardVisibilityIfEnabled(true);
  WaitUntilKeyboardShown(keyboard_client);
  text_client.Blur();
  WaitUntilKeyboardHidden(keyboard_client);
  text_client.Focus();
  WaitUntilKeyboardShown(keyboard_client);
}

IN_PROC_BROWSER_TEST_F(VirtualKeyboardPolicyTest,
                       VirtualKeyboardSmartVisibilityEnabledEnabled) {
  auto* keyboard_client = GetEnabledKeyboardClient();
  ASSERT_TRUE(keyboard_client);
  ui::InputMethod* input_method = GetInputMethod(keyboard_client);
  ASSERT_TRUE(input_method);
  ui::FakeTextInputClient text_client(input_method, WebTextFieldOptions());

  PolicyMap policies;
  policies.Set(key::kVirtualKeyboardSmartVisibilityEnabled,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(true), nullptr);
  UpdateProviderPolicy(policies);

  // Showing the virtual keyboard, losing focus, then focusing again should show
  // the virtual keyboard again.
  text_client.Focus();
  input_method->SetVirtualKeyboardVisibilityIfEnabled(true);
  WaitUntilKeyboardShown(keyboard_client);
  text_client.Blur();
  WaitUntilKeyboardHidden(keyboard_client);
  text_client.Focus();
  WaitUntilKeyboardShown(keyboard_client);
}

IN_PROC_BROWSER_TEST_F(VirtualKeyboardPolicyTest,
                       VirtualKeyboardSmartVisibilityEnabledDisabled) {
  auto* keyboard_client = GetEnabledKeyboardClient();
  ASSERT_TRUE(keyboard_client);
  ui::InputMethod* input_method = GetInputMethod(keyboard_client);
  ASSERT_TRUE(input_method);
  ui::FakeTextInputClient text_client(input_method, WebTextFieldOptions());

  PolicyMap policies;
  policies.Set(key::kVirtualKeyboardSmartVisibilityEnabled,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(false), nullptr);
  UpdateProviderPolicy(policies);

  // Showing the virtual keyboard, losing focus, then focusing again should keep
  // the virtual keyboard hidden.
  text_client.Focus();
  input_method->SetVirtualKeyboardVisibilityIfEnabled(true);
  WaitUntilKeyboardShown(keyboard_client);
  text_client.Blur();
  WaitUntilKeyboardHidden(keyboard_client);
  text_client.Focus();
  Wait(base::Seconds(1));
  EXPECT_FALSE(keyboard_client->is_keyboard_visible());
}

#endif

}  // namespace policy
