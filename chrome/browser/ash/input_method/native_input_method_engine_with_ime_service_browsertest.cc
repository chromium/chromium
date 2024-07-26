// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/ash/input_method/native_input_method_engine.h"
#include "chrome/browser/ash/input_method/stub_input_method_engine_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/input_method_ash.h"
#include "ui/base/ime/ash/text_input_method.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/ime_key_event_dispatcher.h"

namespace ash {
namespace input_method {

namespace {

class TestObserver : public StubInputMethodEngineObserver {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  void OnKeyEvent(const std::string& engine_id,
                  const ui::KeyEvent& event,
                  TextInputMethod::KeyEventDoneCallback callback) override {
    std::move(callback).Run(ui::ime::KeyEventHandledState::kNotHandled);
  }
};

class KeyProcessingWaiter {
 public:
  TextInputMethod::KeyEventDoneCallback CreateCallback() {
    return base::BindOnce(&KeyProcessingWaiter::OnKeyEventDone,
                          base::Unretained(this));
  }

  void OnKeyEventDone(ui::ime::KeyEventHandledState handled_state) {
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

// These use the browser test framework but tamper with the environment through
// global singletons, effectively bypassing CrOS IMF "input method management".
// Test subject is a bespoke NativeInputMethodEngine instance manually attached
// to the environment, shadowing those created and managed by CrOS IMF (an
// integral part of the "browser" environment set up by the browser test).
// TODO(crbug/1197005): Migrate all these to e2e Tast tests.
class NativeInputMethodEngineWithImeServiceTest
    : public InProcessBrowserTest,
      public ui::ImeKeyEventDispatcher {
 public:
  NativeInputMethodEngineWithImeServiceTest() : input_method_(this) {}

 protected:
  void SetUp() override {
    mojo::core::Init();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Passing |true| for |use_ime_service| means NativeInputMethodEngine will
    // launch the IME Service which typically tries to load libimedecoder.so
    // unsupported in browser tests. However, it's luckily okay for these tests
    // as they only test "m17n" whose implementation is directly in the IME
    // Service container, thus not trigger libimedecoder.so loading attempt.
    // TODO(crbug/1197005): Migrate to Tast to avoid reliance on delicate luck.
    engine_ =
        NativeInputMethodEngine::CreateForTesting(/*use_ime_service=*/true);
    IMEBridge::Get()->SetInputContextHandler(&input_method_);
    IMEBridge::Get()->SetCurrentEngineHandler(engine_.get());

    auto observer = std::make_unique<TestObserver>();
    Profile* profile = browser()->profile();
    PrefService* prefs = profile->GetPrefs();
    prefs->Set(::prefs::kLanguageInputMethodSpecificSettings, base::Value());
    engine_->Initialize(std::move(observer), /*extension_id=*/"", profile);

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    // Reset the engine before shutting down the browser because the engine
    // observes ChromeKeyboardControllerClient, which is tied to the browser
    // lifetime.
    engine_.reset();
    IMEBridge::Get()->SetInputContextHandler(nullptr);
    IMEBridge::Get()->SetCurrentEngineHandler(nullptr);
    InProcessBrowserTest::TearDownOnMainThread();
  }

  // Overridden from ui::ImeKeyEventDispatcher:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event) override {
    return ui::EventDispatchDetails();
  }

  void DispatchKeyPress(ui::KeyboardCode code,
                        bool need_flush,
                        int flags = ui::EF_NONE) {
    KeyProcessingWaiter waiterPressed;
    KeyProcessingWaiter waiterReleased;
    engine_->ProcessKeyEvent({ui::EventType::kKeyPressed, code, flags},
                             waiterPressed.CreateCallback());
    engine_->ProcessKeyEvent({ui::EventType::kKeyReleased, code, flags},
                             waiterReleased.CreateCallback());
    if (need_flush) {
      engine_->FlushForTesting();
    }

    waiterPressed.Wait();
    waiterReleased.Wait();
  }

  void SetFocus(ui::TextInputClient* client) {
    input_method_.SetFocusedTextInputClient(client);
  }

  std::unique_ptr<NativeInputMethodEngine> engine_;

 private:
  InputMethodAsh input_method_;
};

// ID is specified in google_xkb_manifest.json.
constexpr char kEngineIdArabic[] = "vkd_ar";
constexpr char kEngineIdVietnameseTelex[] = "vkd_vi_telex";

}  // namespace

// TODO(crbug.com/1361212): Test is flaky. Re-enable the test.
IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineWithImeServiceTest,
                       DISABLED_VietnameseTelex_SimpleTransform) {
  engine_->Enable(kEngineIdVietnameseTelex);
  engine_->FlushForTesting();
  EXPECT_TRUE(engine_->IsConnectedForTesting());

  // Create a fake text field.
  ui::DummyTextInputClient text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  SetFocus(&text_input_client);

  DispatchKeyPress(ui::VKEY_A, true, ui::EF_SHIFT_DOWN);
  DispatchKeyPress(ui::VKEY_S, true);
  DispatchKeyPress(ui::VKEY_SPACE, true);

  // Expect to commit 'Á '.
  ASSERT_EQ(text_input_client.composition_history().size(), 2U);
  EXPECT_EQ(text_input_client.composition_history()[0].text, u"A");
  EXPECT_EQ(text_input_client.composition_history()[1].text, u"\u00c1");
  ASSERT_EQ(text_input_client.insert_text_history().size(), 1U);
  EXPECT_EQ(text_input_client.insert_text_history()[0], u"\u00c1 ");

  SetFocus(nullptr);
}

// TODO(crbug.com/1361212): Test is flaky. Re-enable the test.
IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineWithImeServiceTest,
                       DISABLED_VietnameseTelex_Reset) {
  engine_->Enable(kEngineIdVietnameseTelex);
  engine_->FlushForTesting();
  EXPECT_TRUE(engine_->IsConnectedForTesting());

  // Create a fake text field.
  ui::DummyTextInputClient text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  SetFocus(&text_input_client);

  DispatchKeyPress(ui::VKEY_A, true);
  engine_->Reset();
  DispatchKeyPress(ui::VKEY_S, true);

  // Expect to commit 's'.
  ASSERT_EQ(text_input_client.composition_history().size(), 1U);
  EXPECT_EQ(text_input_client.composition_history()[0].text, u"a");
  ASSERT_EQ(text_input_client.insert_text_history().size(), 1U);
  EXPECT_EQ(text_input_client.insert_text_history()[0], u"s");

  SetFocus(nullptr);
}

// TODO(crbug.com/1361212): Test is flaky. Re-enable the test.
IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineWithImeServiceTest,
                       DISABLED_SwitchActiveController) {
  // Swap between two controllers.
  engine_->Enable(kEngineIdVietnameseTelex);
  engine_->FlushForTesting();
  engine_->Disable();
  engine_->Enable(kEngineIdArabic);
  engine_->FlushForTesting();

  // Create a fake text field.
  ui::DummyTextInputClient text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  SetFocus(&text_input_client);

  DispatchKeyPress(ui::VKEY_A, true);

  // Expect to commit 'ش'.
  ASSERT_EQ(text_input_client.composition_history().size(), 0U);
  ASSERT_EQ(text_input_client.insert_text_history().size(), 1U);
  EXPECT_EQ(text_input_client.insert_text_history()[0], u"ش");

  SetFocus(nullptr);
}

// TODO(crbug.com/1361212): Test is flaky. Re-enable the test.
IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineWithImeServiceTest,
                       DISABLED_NoActiveController) {
  engine_->Enable(kEngineIdVietnameseTelex);
  engine_->FlushForTesting();
  engine_->Disable();

  // Create a fake text field.
  ui::DummyTextInputClient text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  SetFocus(&text_input_client);

  DispatchKeyPress(ui::VKEY_A, true);
  engine_->Reset();

  // Expect no changes.
  ASSERT_EQ(text_input_client.composition_history().size(), 0U);
  ASSERT_EQ(text_input_client.insert_text_history().size(), 0U);

  SetFocus(nullptr);
}

}  // namespace input_method
}  // namespace ash
