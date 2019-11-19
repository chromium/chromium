// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/native_input_method_engine.h"

#include "base/test/task_environment.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "mojo/core/embedder/embedder.h"

namespace {

using input_method::InputMethodEngineBase;

class TestObserver : public InputMethodEngineBase::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  void OnActivate(const std::string& engine_id) override {}
  void OnDeactivated(const std::string& engine_id) override {}
  void OnFocus(
      const ui::IMEEngineHandlerInterface::InputContext& context) override {}
  void OnBlur(int context_id) override {}
  void OnKeyEvent(
      const std::string& engine_id,
      const InputMethodEngineBase::KeyboardEvent& event,
      ui::IMEEngineHandlerInterface::KeyEventDoneCallback key_data) override {}
  void OnInputContextUpdate(
      const ui::IMEEngineHandlerInterface::InputContext& context) override {}
  void OnCandidateClicked(
      const std::string& engine_id,
      int candidate_id,
      InputMethodEngineBase::MouseButtonEvent button) override {}
  void OnMenuItemActivated(const std::string& engine_id,
                           const std::string& menu_id) override {}
  void OnSurroundingTextChanged(const std::string& engine_id,
                                const std::string& text,
                                int cursor_pos,
                                int anchor_pos,
                                int offset) override {}
  void OnCompositionBoundsChanged(
      const std::vector<gfx::Rect>& bounds) override {}
  void OnScreenProjectionChanged(bool is_projected) override {}
  void OnReset(const std::string& engine_id) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class NativeInputMethodEngineTest : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    mojo::core::Init();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    auto observer = std::make_unique<TestObserver>();
    engine_.Initialize(std::move(observer), "", nullptr);
    InProcessBrowserTest::SetUpOnMainThread();
  }

  chromeos::NativeInputMethodEngine engine_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest, CanConnectToInputEngine) {
  // ID for Arabic is specified in google_xkb_manifest.json.
  engine_.Enable("vkd_ar");
  engine_.FlushForTesting();
  EXPECT_TRUE(engine_.IsConnectedForTesting());
}
