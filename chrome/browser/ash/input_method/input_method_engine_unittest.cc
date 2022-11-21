// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_method_engine.h"

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/input_method/input_method_configuration.h"
#include "chrome/browser/ash/input_method/mock_input_method_manager_impl.h"
#include "chrome/browser/ash/input_method/stub_input_method_engine_observer.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client_test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/mock_component_extension_ime_manager_delegate.h"
#include "ui/base/ime/ash/mock_ime_input_context_handler.h"
#include "ui/base/ime/ash/text_input_method.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace input_method {
namespace {

const char kTestExtensionId[] = "mppnpdlheglhdfmldimlhpnegondlapf";
const char kTestExtensionId2[] = "dmpipdbjkoajgdeppkffbjhngfckdloi";
const char kTestImeComponentId[] = "test_engine_id";

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
  auto* delegate = new MockComponentExtensionIMEManagerDelegate;

  ComponentExtensionIME ext1;
  ext1.id = kTestExtensionId;

  ComponentExtensionEngine ext1_engine1;
  ext1_engine1.engine_id = kTestImeComponentId;
  ext1_engine1.language_codes.emplace_back("en-US");
  ext1_engine1.layout = "us";
  ext1.engines.push_back(ext1_engine1);

  std::vector<ComponentExtensionIME> ime_list;
  ime_list.push_back(ext1);
  delegate->set_ime_list(ime_list);

  auto* comp_ime_manager = new ComponentExtensionIMEManager(
      std::unique_ptr<ComponentExtensionIMEManagerDelegate>(delegate));

  auto* manager = new MockInputMethodManagerImpl;
  manager->SetComponentExtensionIMEManager(
      std::unique_ptr<ComponentExtensionIMEManager>(comp_ime_manager));
  InitializeForTesting(manager);
}

class TestObserver : public StubInputMethodEngineObserver {
 public:
  TestObserver() : calls_bitmap_(NONE) {}
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;
  ~TestObserver() override = default;

  void OnActivate(const std::string& engine_id) override {
    calls_bitmap_ |= ACTIVATE;
    engine_id_ = engine_id;
  }
  void OnDeactivated(const std::string& engine_id) override {
    calls_bitmap_ |= DEACTIVATED;
    engine_id_ = engine_id;
  }
  void OnFocus(const std::string& engine_id,
               int context_id,
               const ui::TextInputMethod::InputContext& context) override {
    calls_bitmap_ |= ONFOCUS;
  }
  void OnBlur(const std::string& engine_id, int context_id) override {
    calls_bitmap_ |= ONBLUR;
  }
  void OnKeyEvent(const std::string& engine_id,
                  const ui::KeyEvent& event,
                  ui::TextInputMethod::KeyEventDoneCallback callback) override {
    std::move(callback).Run(ui::ime::KeyEventHandledState::kHandledByIME);
  }
  void OnCompositionBoundsChanged(
      const std::vector<gfx::Rect>& bounds) override {
    calls_bitmap_ |= ONCOMPOSITIONBOUNDSCHANGED;
  }

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
};

class InputMethodEngineTest : public testing::Test {
 public:
  InputMethodEngineTest() : observer_(nullptr), input_view_("inputview.html") {
    languages_.emplace_back("en-US");
    layouts_.emplace_back("us");
    InitInputMethod();
    mock_ime_input_context_handler_ =
        std::make_unique<ui::MockIMEInputContextHandler>();
    ui::IMEBridge::Get()->SetInputContextHandler(
        mock_ime_input_context_handler_.get());

    chrome_keyboard_controller_client_test_helper_ =
        ChromeKeyboardControllerClientTestHelper::InitializeWithFake();
  }

  InputMethodEngineTest(const InputMethodEngineTest&) = delete;
  InputMethodEngineTest& operator=(const InputMethodEngineTest&) = delete;

  ~InputMethodEngineTest() override {
    ui::IMEBridge::Get()->SetInputContextHandler(nullptr);
    engine_.reset();
    chrome_keyboard_controller_client_test_helper_.reset();
    Shutdown();
  }

 protected:
  void CreateEngine(bool allowlisted) {
    engine_ = std::make_unique<InputMethodEngine>();
    observer_ = new TestObserver();
    std::unique_ptr<InputMethodEngineObserver> observer_ptr(observer_);
    engine_->Initialize(std::move(observer_ptr),
                        allowlisted ? kTestExtensionId : kTestExtensionId2,
                        nullptr);
  }

  void Focus(ui::TextInputType input_type) {
    ui::TextInputMethod::InputContext input_context(input_type);
    engine_->Focus(input_context);
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
};

}  // namespace

TEST_F(InputMethodEngineTest, TestSwitching) {
  CreateEngine(false);
  // Enable/disable with focus.
  Focus(ui::TEXT_INPUT_TYPE_URL);
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());
  // Enable/disable without focus.
  engine_->Blur();
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
  engine_->Blur();
  EXPECT_EQ(ONBLUR, observer_->GetCallsBitmapAndReset());
  // Focus change when disabled.
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());
  Focus(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->Blur();
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
}

TEST_F(InputMethodEngineTest, TestSwitching_Password_3rd_Party) {
  CreateEngine(false);
  // Enable/disable with focus.
  Focus(ui::TEXT_INPUT_TYPE_PASSWORD);
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
  engine_->Blur();
  EXPECT_EQ(ONBLUR, observer_->GetCallsBitmapAndReset());
  Focus(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());
}

TEST_F(InputMethodEngineTest, TestSwitching_Password_Allowlisted) {
  CreateEngine(true);
  // Enable/disable with focus.
  Focus(ui::TEXT_INPUT_TYPE_PASSWORD);
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
  engine_->Blur();
  EXPECT_EQ(ONBLUR, observer_->GetCallsBitmapAndReset());
  Focus(ui::TEXT_INPUT_TYPE_PASSWORD);
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
  Focus(ui::TEXT_INPUT_TYPE_URL);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());

  // Resets the engine.
  engine_->Reset();
  EXPECT_EQ(RESET, observer_->GetCallsBitmapAndReset());
  EXPECT_EQ(kTestImeComponentId, observer_->GetEngineIdAndReset());
}

TEST_F(InputMethodEngineTest, TestHistograms) {
  CreateEngine(true);
  Focus(ui::TEXT_INPUT_TYPE_TEXT);
  engine_->Enable(kTestImeComponentId);
  std::vector<InputMethodEngine::SegmentInfo> segments;
  int context = engine_->GetContextIdForTesting();
  std::string error;
  base::HistogramTester histograms;
  engine_->SetComposition(context, "test", 0, 0, 0, segments, nullptr);
  engine_->CommitText(context, u"input", &error);
  engine_->SetComposition(context, "test", 0, 0, 0, segments, nullptr);
  engine_->CommitText(context,
                      u"你好",  // 2 UTF-16 code units
                      &error);
  engine_->SetComposition(context, "test", 0, 0, 0, segments, nullptr);
  engine_->CommitText(context, u"input你好", &error);
  // This one shouldn't be counted because there was no composition.
  engine_->CommitText(context, u"abc", &error);
  histograms.ExpectTotalCount("InputMethod.CommitLength", 4);
  histograms.ExpectBucketCount("InputMethod.CommitLength", 5, 1);
  histograms.ExpectBucketCount("InputMethod.CommitLength", 2, 1);
  histograms.ExpectBucketCount("InputMethod.CommitLength", 7, 1);
  histograms.ExpectBucketCount("InputMethod.CommitLength", 3, 1);
}

TEST_F(InputMethodEngineTest, TestInvalidCompositionReturnsFalse) {
  CreateEngine(true);
  std::string error;
  Focus(ui::TEXT_INPUT_TYPE_TEXT);
  engine_->Enable(kTestImeComponentId);
  std::vector<InputMethodEngine::SegmentInfo> segments;
  int context = engine_->GetContextIdForTesting();
  EXPECT_EQ(engine_->SetComposition(context, "test", 0, 0, 0, segments, &error),
            true);
  EXPECT_EQ(
      engine_->SetComposition(context, "test", -1, 0, 0, segments, &error),
      false);
  EXPECT_EQ(
      engine_->SetComposition(context, "test", 0, -1, 0, segments, &error),
      false);
  EXPECT_EQ(
      engine_->SetComposition(context, "test", 0, 0, -1, segments, &error),
      false);
  // Still return false if multiple values set as negative
  EXPECT_EQ(
      engine_->SetComposition(context, "test", -12, 0, -1, segments, &error),
      false);
  EXPECT_EQ(
      engine_->SetComposition(context, "test", -1, -1, -1, segments, &error),
      false);
  EXPECT_EQ(engine_->SetComposition(context, "test", 0, 6, 0, segments, &error),
            false);
}

TEST_F(InputMethodEngineTest, TestCompositionBoundsChanged) {
  CreateEngine(true);
  // Enable/disable with focus.
  engine_->SetCompositionBounds({gfx::Rect()});
  EXPECT_EQ(ONCOMPOSITIONBOUNDSCHANGED, observer_->GetCallsBitmapAndReset());
}

// See https://crbug.com/980437.
TEST_F(InputMethodEngineTest, TestDisableAfterSetCompositionRange) {
  CreateEngine(true);
  Focus(ui::TEXT_INPUT_TYPE_TEXT);
  engine_->Enable(kTestImeComponentId);

  const int context = engine_->GetContextIdForTesting();

  std::string error;
  engine_->CommitText(context, u"text", &error);
  EXPECT_EQ("", error);
  EXPECT_EQ(1, mock_ime_input_context_handler_->commit_text_call_count());
  EXPECT_EQ(u"text", mock_ime_input_context_handler_->last_commit_text());

  // Change composition range to include "text".
  engine_->InputMethodEngine::SetCompositionRange(context, 0, 4, {}, &error);
  EXPECT_EQ("", error);

  // Disable to commit
  engine_->Disable();

  EXPECT_EQ("", error);
  EXPECT_EQ(2, mock_ime_input_context_handler_->commit_text_call_count());
  EXPECT_EQ(u"text", mock_ime_input_context_handler_->last_commit_text());
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

TEST_F(InputMethodEngineTest, AcceptSuggestionCandidateCommitsCandidate) {
  CreateEngine(true);
  Focus(ui::TEXT_INPUT_TYPE_TEXT);
  engine_->Enable(kTestImeComponentId);

  const int context = engine_->GetContextIdForTesting();

  std::string error;
  engine_->AcceptSuggestionCandidate(context, u"suggestion", 0, &error);

  EXPECT_EQ("", error);
  EXPECT_EQ(
      0, mock_ime_input_context_handler_->delete_surrounding_text_call_count());
  EXPECT_EQ(u"suggestion", mock_ime_input_context_handler_->last_commit_text());
}

TEST_F(InputMethodEngineTest,
       AcceptSuggestionCandidateDeletesSurroundingAndCommitsCandidate) {
  CreateEngine(true);
  Focus(ui::TEXT_INPUT_TYPE_TEXT);
  engine_->Enable(kTestImeComponentId);

  const int context = engine_->GetContextIdForTesting();

  std::string error;
  engine_->CommitText(context, u"text", &error);
  engine_->AcceptSuggestionCandidate(context, u"suggestion", 1, &error);

  EXPECT_EQ("", error);
  EXPECT_EQ(
      1, mock_ime_input_context_handler_->delete_surrounding_text_call_count());
  auto deleteSurroundingTextArg =
      mock_ime_input_context_handler_->last_delete_surrounding_text_arg();
  EXPECT_EQ(deleteSurroundingTextArg.num_char16s_before_cursor, 1u);
  EXPECT_EQ(deleteSurroundingTextArg.num_char16s_after_cursor, 0u);
  EXPECT_EQ(u"suggestion", mock_ime_input_context_handler_->last_commit_text());
}
}  // namespace input_method
}  // namespace ash
