// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_engine.h"

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client_test_helper.h"
#include "chrome/browser/ui/input_method/input_method_engine_base.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/mock_component_extension_ime_manager_delegate.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/ime_engine_handler_interface.h"
#include "ui/base/ime/mock_ime_input_context_handler.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/geometry/rect.h"

using input_method::InputMethodEngineBase;

namespace chromeos {

namespace input_method {
namespace {

const char kTestExtensionId[] = "mppnpdlheglhdfmldimlhpnegondlapf";
const char kTestExtensionId2[] = "dmpipdbjkoajgdeppkffbjhngfckdloi";
const char kTestImeComponentId[] = "test_engine_id";
const char kErrorNotActive[] = "IME is not active";
const char kErrorInvalidValue[] = "Argument '%s' with value '%d' is not valid";

enum CallsBitmap {
  NONE = 0U,
  ACTIVATE = 1U,
  DEACTIVATED = 2U,
  ONFOCUS = 4U,
  ONBLUR = 8U,
  ONCOMPOSITIONBOUNDSCHANGED = 16U,
  RESET = 32U
};

void InitInputMethod() {
  auto* comp_ime_manager = new ComponentExtensionIMEManager;
  auto* delegate = new MockComponentExtIMEManagerDelegate;

  ComponentExtensionIME ext1;
  ext1.id = kTestExtensionId;

  ComponentExtensionEngine ext1_engine1;
  ext1_engine1.engine_id = kTestImeComponentId;
  ext1_engine1.language_codes.emplace_back("en-US");
  ext1_engine1.layouts.emplace_back("us");
  ext1.engines.push_back(ext1_engine1);

  std::vector<ComponentExtensionIME> ime_list;
  ime_list.push_back(ext1);
  delegate->set_ime_list(ime_list);
  comp_ime_manager->Initialize(
      std::unique_ptr<ComponentExtensionIMEManagerDelegate>(delegate));

  auto* manager = new MockInputMethodManagerImpl;
  manager->SetComponentExtensionIMEManager(
      std::unique_ptr<ComponentExtensionIMEManager>(comp_ime_manager));
  InitializeForTesting(manager);
}

class TestObserver : public InputMethodEngineBase::Observer {
 public:
  TestObserver() : calls_bitmap_(NONE) {}
  ~TestObserver() override = default;

  void OnActivate(const std::string& engine_id) override {
    calls_bitmap_ |= ACTIVATE;
    engine_id_ = engine_id;
  }
  void OnDeactivated(const std::string& engine_id) override {
    calls_bitmap_ |= DEACTIVATED;
    engine_id_ = engine_id;
  }
  void OnFocus(
      const ui::IMEEngineHandlerInterface::InputContext& context) override {
    calls_bitmap_ |= ONFOCUS;
  }
  void OnBlur(int context_id) override { calls_bitmap_ |= ONBLUR; }
  void OnKeyEvent(
      const std::string& engine_id,
      const InputMethodEngineBase::KeyboardEvent& event,
      ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback) override {
    std::move(callback).Run(/* handled */ true);
  }
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
      const std::vector<gfx::Rect>& bounds) override {
    calls_bitmap_ |= ONCOMPOSITIONBOUNDSCHANGED;
  }
  void OnScreenProjectionChanged(bool is_projected) override {}
  void OnReset(const std::string& engine_id) override {
    calls_bitmap_ |= RESET;
    engine_id_ = engine_id;
  }

  unsigned char GetCallsBitmapAndReset() {
    unsigned char ret = calls_bitmap_;
    calls_bitmap_ = NONE;
    return ret;
  }

  std::string GetEngineIdAndReset() {
    std::string engine_id{engine_id_};
    engine_id_.clear();
    return engine_id;
  }

 private:
  unsigned char calls_bitmap_;
  std::string engine_id_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class InputMethodEngineTest : public testing::Test {
 public:
  InputMethodEngineTest() : observer_(nullptr), input_view_("inputview.html") {
    languages_.emplace_back("en-US");
    layouts_.emplace_back("us");
    InitInputMethod();
    ui::IMEBridge::Initialize();
    mock_ime_input_context_handler_.reset(new ui::MockIMEInputContextHandler());
    ui::IMEBridge::Get()->SetInputContextHandler(
        mock_ime_input_context_handler_.get());

    chrome_keyboard_controller_client_test_helper_ =
        ChromeKeyboardControllerClientTestHelper::InitializeWithFake();
  }
  ~InputMethodEngineTest() override {
    ui::IMEBridge::Get()->SetInputContextHandler(nullptr);
    engine_.reset();
    chrome_keyboard_controller_client_test_helper_.reset();
    Shutdown();
  }

 protected:
  void CreateEngine(bool whitelisted) {
    engine_.reset(new InputMethodEngine());
    observer_ = new TestObserver();
    std::unique_ptr<InputMethodEngineBase::Observer> observer_ptr(observer_);
    engine_->Initialize(std::move(observer_ptr),
                        whitelisted ? kTestExtensionId : kTestExtensionId2,
                        ProfileManager::GetActiveUserProfile());
  }

  void FocusIn(ui::TextInputType input_type) {
    ui::IMEEngineHandlerInterface::InputContext input_context(
        input_type, ui::TEXT_INPUT_MODE_DEFAULT, ui::TEXT_INPUT_FLAG_NONE,
        ui::TextInputClient::FOCUS_REASON_OTHER,
        false /* should_do_learning */);
    engine_->FocusIn(input_context);
    ui::IMEBridge::Get()->SetCurrentInputContext(input_context);
  }

  std::unique_ptr<InputMethodEngine> engine_;

  TestObserver* observer_;
  std::vector<std::string> languages_;
  std::vector<std::string> layouts_;
  GURL options_page_;
  GURL input_view_;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ui::MockIMEInputContextHandler>
      mock_ime_input_context_handler_;
  std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
      chrome_keyboard_controller_client_test_helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodEngineTest);
};

}  // namespace

TEST_F(InputMethodEngineTest, TestSwitching) {
  CreateEngine(false);
  // Enable/disable with focus.
  FocusIn(ui::TEXT_INPUT_TYPE_URL);
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());
  // Enable/disable without focus.
  engine_->FocusOut();
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());
  // Focus change when enabled.
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());
  engine_->FocusOut();
  EXPECT_EQ(ONBLUR, observer_->GetCallsBitmapAndReset());
  // Focus change when disabled.
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());
  FocusIn(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->FocusOut();
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
}

TEST_F(InputMethodEngineTest, TestSwitching_Password_3rd_Party) {
  CreateEngine(false);
  // Enable/disable with focus.
  FocusIn(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());
  // Focus change when enabled.
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());
  engine_->FocusOut();
  EXPECT_EQ(ONBLUR, observer_->GetCallsBitmapAndReset());
  FocusIn(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());
}

TEST_F(InputMethodEngineTest, TestSwitching_Password_Whitelisted) {
  CreateEngine(true);
  // Enable/disable with focus.
  FocusIn(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());
  // Focus change when enabled.
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());
  engine_->FocusOut();
  EXPECT_EQ(ONBLUR, observer_->GetCallsBitmapAndReset());
  FocusIn(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());
}

// Tests input.ime.onReset API.
TEST_F(InputMethodEngineTest, TestReset) {
  CreateEngine(false);
  // Enables the extension with focus.
  engine_->Enable(kTestImeComponentId);
  FocusIn(ui::TEXT_INPUT_TYPE_URL);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());

  // Resets the engine.
  engine_->Reset();
  EXPECT_EQ(RESET, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());
}

TEST_F(InputMethodEngineTest, TestHistograms) {
  CreateEngine(true);
  FocusIn(ui::TEXT_INPUT_TYPE_TEXT);
  engine_->Enable(kTestImeComponentId);
  std::vector<InputMethodEngineBase::SegmentInfo> segments;
  int context = engine_->GetContextIdForTesting();
  std::string error;
  base::HistogramTester histograms;
  engine_->SetComposition(context, "test", 0, 0, 0, segments, nullptr);
  engine_->CommitText(context, "input", &error);
  engine_->SetComposition(context, "test", 0, 0, 0, segments, nullptr);
  engine_->CommitText(context,
                      "\xE5\x85\xA5\xE5\x8A\x9B",  // 2 UTF-8 characters
                      &error);
  engine_->SetComposition(context, "test", 0, 0, 0, segments, nullptr);
  engine_->CommitText(context, "input\xE5\x85\xA5\xE5\x8A\x9B", &error);
  // This one shouldn't be counted because there was no composition.
  engine_->CommitText(context, "abc", &error);
  histograms.ExpectTotalCount("InputMethod.CommitLength", 4);
  histograms.ExpectBucketCount("InputMethod.CommitLength", 5, 1);
  histograms.ExpectBucketCount("InputMethod.CommitLength", 2, 1);
  histograms.ExpectBucketCount("InputMethod.CommitLength", 7, 1);
  histograms.ExpectBucketCount("InputMethod.CommitLength", 3, 1);
}

TEST_F(InputMethodEngineTest, TestCompositionBoundsChanged) {
  CreateEngine(true);
  // Enable/disable with focus.
  engine_->SetCompositionBounds({gfx::Rect()});
  EXPECT_EQ(ONCOMPOSITIONBOUNDSCHANGED, observer_->GetCallsBitmapAndReset());
}

TEST_F(InputMethodEngineTest, TestSetSelectionRange) {
  CreateEngine(true);
  const int context = engine_->GetContextIdForTesting();
  std::string error;
  engine_->::input_method::InputMethodEngineBase::SetSelectionRange(
      context, /* start */ 0, /* end */ 0, &error);
  EXPECT_EQ(kErrorNotActive, error);
  EXPECT_EQ(0,
            mock_ime_input_context_handler_->set_selection_range_call_count());
  error = "";

  engine_->Enable(kTestImeComponentId);
  engine_->::input_method::InputMethodEngineBase::SetSelectionRange(
      context, /* start */ 0, /* end */ 0, &error);
  EXPECT_EQ("", error);
  EXPECT_EQ(1,
            mock_ime_input_context_handler_->set_selection_range_call_count());
  error = "";

  engine_->::input_method::InputMethodEngineBase::SetSelectionRange(
      context, /* start */ -1, /* end */ 0, &error);
  EXPECT_EQ(base::StringPrintf(kErrorInvalidValue, "start", -1), error);
  EXPECT_EQ(1,
            mock_ime_input_context_handler_->set_selection_range_call_count());
  error = "";

  engine_->::input_method::InputMethodEngineBase::SetSelectionRange(
      context, /* start */ 0, /* end */ -1, &error);
  EXPECT_EQ(base::StringPrintf(kErrorInvalidValue, "end", -1), error);
  EXPECT_EQ(1,
            mock_ime_input_context_handler_->set_selection_range_call_count());
}

// See https://crbug.com/980437.
TEST_F(InputMethodEngineTest, TestDisableAfterSetCompositionRange) {
  CreateEngine(true);
  FocusIn(ui::TEXT_INPUT_TYPE_TEXT);
  engine_->Enable(kTestImeComponentId);

  const int context = engine_->GetContextIdForTesting();

  std::string error;
  engine_->CommitText(context, "text", &error);
  EXPECT_EQ("", error);
  EXPECT_EQ(1, mock_ime_input_context_handler_->commit_text_call_count());
  EXPECT_EQ("text", mock_ime_input_context_handler_->last_commit_text());

  // Change composition range to include "text".
  engine_->::input_method::InputMethodEngineBase::SetCompositionRange(
      context, 0, 4, {}, &error);
  EXPECT_EQ("", error);

  // Disable to commit
  engine_->Disable();

  EXPECT_EQ("", error);
  EXPECT_EQ(2, mock_ime_input_context_handler_->commit_text_call_count());
  EXPECT_EQ("text", mock_ime_input_context_handler_->last_commit_text());
}

TEST_F(InputMethodEngineTest, KeyEventHandledRecordsLatencyHistogram) {
  CreateEngine(true);
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectTotalCount("InputMethod.KeyEventLatency", 0);

  const ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A, 0,
                           ui::DomKey::FromCharacter('a'),
                           ui::EventTimeForNow());
  engine_->ProcessKeyEvent(event, base::DoNothing());

  histogram_tester.ExpectTotalCount("InputMethod.KeyEventLatency", 1);
}

}  // namespace input_method
}  // namespace chromeos
