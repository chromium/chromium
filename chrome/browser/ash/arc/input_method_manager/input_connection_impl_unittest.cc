// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_method_manager/input_connection_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/arc/input_method_manager/test_input_method_manager_bridge.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client_test_helper.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/mock_ime_input_context_handler.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/mock_input_method.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace arc {

namespace {

class DummyInputMethodEngineObserver
    : public ash::input_method::InputMethodEngineObserver {
 public:
  DummyInputMethodEngineObserver() = default;

  DummyInputMethodEngineObserver(const DummyInputMethodEngineObserver&) =
      delete;
  DummyInputMethodEngineObserver& operator=(
      const DummyInputMethodEngineObserver&) = delete;

  ~DummyInputMethodEngineObserver() override = default;

  void OnActivate(const std::string& engine_id) override {}
  void OnFocus(const std::string& engine_id,
               int context_id,
               const ash::TextInputMethod::InputContext& context) override {}
  void OnBlur(const std::string& engine_id, int context_id) override {}
  void OnKeyEvent(
      const std::string& engine_id,
      const ui::KeyEvent& event,
      ash::TextInputMethod::KeyEventDoneCallback key_data) override {}
  void OnReset(const std::string& engine_id) override {}
  void OnDeactivated(const std::string& engine_id) override {}
  void OnCaretBoundsChanged(const gfx::Rect& caret_bounds) override {}
  void OnSurroundingTextChanged(const std::string& engine_id,
                                const std::u16string& text,
                                const gfx::Range selection_range,
                                int offset_pos) override {}
  void OnCandidateClicked(const std::string& component_id,
                          int candidate_id,
                          ash::input_method::MouseButtonEvent button) override {
  }
  void OnAssistiveWindowChanged(
      const ash::ime::AssistiveWindow& window) override {}
  void OnMenuItemActivated(const std::string& component_id,
                           const std::string& menu_id) override {}
  void OnScreenProjectionChanged(bool is_projected) override {}
  void OnSuggestionsChanged(
      const std::vector<std::string>& suggestions) override {}
  void OnInputMethodOptionsChanged(const std::string& engine_id) override {}
};

class TestInputMethodManager
    : public ash::input_method::MockInputMethodManager {
 public:
  TestInputMethodManager()
      : state_(base::MakeRefCounted<
               ash::input_method::MockInputMethodManager::State>()) {}
  TestInputMethodManager(const TestInputMethodManager&) = delete;
  TestInputMethodManager& operator=(const TestInputMethodManager&) = delete;
  ~TestInputMethodManager() override = default;

  scoped_refptr<InputMethodManager::State> GetActiveIMEState() override {
    return state_;
  }

 private:
  scoped_refptr<State> state_;
};

class TestIMEInputContextHandler : public ash::MockIMEInputContextHandler {
 public:
  explicit TestIMEInputContextHandler(ui::InputMethod* input_method)
      : input_method_(input_method) {}

  TestIMEInputContextHandler(const TestIMEInputContextHandler&) = delete;
  TestIMEInputContextHandler& operator=(const TestIMEInputContextHandler&) =
      delete;

  ~TestIMEInputContextHandler() override = default;

  ui::InputMethod* GetInputMethod() override { return input_method_; }

  void SendKeyEvent(ui::KeyEvent* event) override {
    ash::MockIMEInputContextHandler::SendKeyEvent(event);
    ++send_key_event_call_count_;
  }

  bool SetComposingRange(
      uint32_t before,
      uint32_t after,
      const std::vector<ui::ImeTextSpan>& text_spans) override {
    ash::MockIMEInputContextHandler::SetComposingRange(before, after,
                                                       text_spans);
    composition_range_history_.push_back(std::make_tuple(before, after));
    return true;
  }

  void Reset() {
    ash::MockIMEInputContextHandler::Reset();
    send_key_event_call_count_ = 0;
    composition_range_history_.clear();
  }

  int send_key_event_call_count() const { return send_key_event_call_count_; }
  const std::vector<std::tuple<int, int>>& composition_range_history() {
    return composition_range_history_;
  }

 private:
  const raw_ptr<ui::InputMethod> input_method_;

  int send_key_event_call_count_ = 0;
  std::vector<std::tuple<int, int>> composition_range_history_;
};

class MockTextInputClient : public ui::DummyTextInputClient {
 public:
  void SetText(const std::string& text) { text_ = text; }

  void SetCursorPos(int pos) { cursor_pos_ = pos; }

  void SetCompositionRange(const gfx::Range& range) {
    composition_range_ = range;
  }

  bool GetTextRange(gfx::Range* range) const override {
    *range = gfx::Range(0, base::ASCIIToUTF16(text_).length());
    return true;
  }

  bool GetTextFromRange(const gfx::Range& range,
                        std::u16string* text) const override {
    *text = base::ASCIIToUTF16(text_.substr(range.start(), range.end()));
    return true;
  }

  bool GetEditableSelectionRange(gfx::Range* range) const override {
    *range = gfx::Range(cursor_pos_, cursor_pos_);
    return true;
  }

  bool GetCompositionTextRange(gfx::Range* range) const override {
    *range = composition_range_;
    return true;
  }

 private:
  std::string text_;
  int cursor_pos_ = 0;
  gfx::Range composition_range_ = gfx::Range(0, 0);
};

class InputConnectionImplTest : public testing::Test {
 public:
  InputConnectionImplTest() = default;

  InputConnectionImplTest(const InputConnectionImplTest&) = delete;
  InputConnectionImplTest& operator=(const InputConnectionImplTest&) = delete;

  ~InputConnectionImplTest() override = default;

  std::unique_ptr<InputConnectionImpl> CreateNewConnection(int context_id) {
    return std::make_unique<InputConnectionImpl>(engine_.get(), bridge_.get(),
                                                 context_id);
  }

  ash::input_method::InputMethodEngine* engine() { return engine_.get(); }

  TestIMEInputContextHandler* context_handler() { return &context_handler_; }

  MockTextInputClient* client() { return &text_input_client_; }

  ash::TextInputMethod::InputContext context() {
    return ash::TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT);
  }

  void SetUp() override {
    ash::input_method::InputMethodManager::Initialize(
        new TestInputMethodManager);
    bridge_ = std::make_unique<TestInputMethodManagerBridge>();
    engine_ = std::make_unique<ash::input_method::InputMethodEngine>();
    engine_->Initialize(std::make_unique<DummyInputMethodEngineObserver>(),
                        "test_extension_id", nullptr);
    chrome_keyboard_controller_client_test_helper_ =
        ChromeKeyboardControllerClientTestHelper::InitializeWithFake();

    // Enable InputMethodEngine.
    ash::IMEBridge::Get()->SetInputContextHandler(&context_handler_);
    input_method_.SetFocusedTextInputClient(&text_input_client_);
    engine()->Enable("test_component_id");
  }

  void TearDown() override {
    chrome_keyboard_controller_client_test_helper_.reset();
    ash::IMEBridge::Get()->SetInputContextHandler(nullptr);
    engine_.reset();
    bridge_.reset();
    ash::input_method::InputMethodManager::Shutdown();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestInputMethodManagerBridge> bridge_;
  std::unique_ptr<ash::input_method::InputMethodEngine> engine_;
  MockTextInputClient text_input_client_;
  ui::MockInputMethod input_method_{nullptr};
  TestIMEInputContextHandler context_handler_{&input_method_};
  std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
      chrome_keyboard_controller_client_test_helper_;
};

}  // anonymous namespace

TEST_F(InputConnectionImplTest, CommitText) {
  auto connection = CreateNewConnection(1);
  engine()->Focus(context());

  context_handler()->Reset();
  connection->CommitText(u"text", 1);
  EXPECT_EQ(1, context_handler()->commit_text_call_count());
  EXPECT_EQ(u"text", context_handler()->last_commit_text());

  // Calling Commit() with '\n' invokes SendKeyEvent.
  context_handler()->Reset();
  connection->CommitText(u"\n", 1);
  EXPECT_EQ(0, context_handler()->commit_text_call_count());
  const std::vector<ui::KeyEvent>& sent_key_events =
      context_handler()->sent_key_events();
  EXPECT_EQ(2u, sent_key_events.size());
  const ui::KeyEvent& last_sent_key_event = sent_key_events.back();
  EXPECT_EQ(ui::VKEY_RETURN, last_sent_key_event.key_code());
  EXPECT_EQ(ui::EventType::kKeyReleased, last_sent_key_event.type());

  engine()->Blur();
}

TEST_F(InputConnectionImplTest, DeleteSurroundingText) {
  auto connection = CreateNewConnection(1);
  engine()->Focus(context());

  context_handler()->Reset();
  connection->DeleteSurroundingText(1, 1);
  EXPECT_EQ(1, context_handler()->delete_surrounding_text_call_count());

  engine()->Blur();
}

TEST_F(InputConnectionImplTest, FinishComposingText) {
  auto connection = CreateNewConnection(1);
  engine()->Focus(context());

  // If there is no composing text, FinishComposingText() does nothing.
  context_handler()->Reset();
  connection->FinishComposingText();
  EXPECT_EQ(0, context_handler()->commit_text_call_count());

  // If there is composing text, FinishComposingText() calls CommitText() with
  // the text.
  context_handler()->Reset();
  connection->SetComposingText(u"composing", 0, std::nullopt);
  client()->SetText("composing");
  client()->SetCompositionRange(gfx::Range(0, 9));
  EXPECT_EQ(0, context_handler()->commit_text_call_count());
  connection->FinishComposingText();
  EXPECT_EQ(1, context_handler()->commit_text_call_count());
  EXPECT_EQ(u"composing", context_handler()->last_commit_text());

  client()->SetCompositionRange(gfx::Range(0, 0));
  connection->FinishComposingText();
  EXPECT_EQ(1, context_handler()->commit_text_call_count());

  engine()->Blur();
}

TEST_F(InputConnectionImplTest, SetComposingText) {
  const std::u16string text = u"text";
  auto connection = CreateNewConnection(1);
  engine()->Focus(context());

  context_handler()->Reset();
  connection->SetComposingText(text, 0, std::nullopt);
  EXPECT_EQ(1, context_handler()->update_preedit_text_call_count());
  EXPECT_EQ(
      text,
      context_handler()->last_update_composition_arg().composition_text.text);
  EXPECT_EQ(3u, context_handler()
                    ->last_update_composition_arg()
                    .composition_text.selection.start());
  // Commitiing the composing text calls ClearComposition() and CommitText().
  connection->CommitText(text, 0);
  EXPECT_EQ(2, context_handler()->update_preedit_text_call_count());
  EXPECT_EQ(
      u"",
      context_handler()->last_update_composition_arg().composition_text.text);
  EXPECT_EQ(1, context_handler()->commit_text_call_count());

  // CommitText should clear the composing text.
  connection->FinishComposingText();
  // commit_text_call_count() doesn't change.
  EXPECT_EQ(1, context_handler()->commit_text_call_count());

  // Selection range
  context_handler()->Reset();
  connection->SetComposingText(text, 0, std::make_optional<gfx::Range>(1, 3));
  EXPECT_EQ(1u, context_handler()
                    ->last_update_composition_arg()
                    .composition_text.selection.start());
  EXPECT_EQ(3u, context_handler()
                    ->last_update_composition_arg()
                    .composition_text.selection.end());

  engine()->Blur();
}

TEST_F(InputConnectionImplTest, SetSelection) {
  auto connection = CreateNewConnection(1);
  engine()->Focus(context());
  ASSERT_TRUE(client()->selection_history().empty());

  context_handler()->Reset();
  connection->SetSelection(gfx::Range(2, 4));
  EXPECT_FALSE(client()->selection_history().empty());
  EXPECT_EQ(2u, client()->selection_history().back().start());
  EXPECT_EQ(4u, client()->selection_history().back().end());

  engine()->Blur();
}

TEST_F(InputConnectionImplTest, SendKeyEvent) {
  auto connection = CreateNewConnection(1);
  engine()->Focus(context());

  context_handler()->Reset();

  {
    auto sent = std::make_unique<ui::KeyEvent>(ui::EventType::kKeyPressed,
                                               ui::VKEY_RETURN, ui::EF_NONE);
    connection->SendKeyEvent(std::move(sent));
    const std::vector<ui::KeyEvent>& sent_key_events =
        context_handler()->sent_key_events();
    EXPECT_EQ(1u, sent_key_events.size());
    const ui::KeyEvent& received = sent_key_events.back();
    EXPECT_EQ(ui::VKEY_RETURN, received.key_code());
    EXPECT_EQ(ui::DomCode::ENTER, received.code());
    EXPECT_EQ("Enter", received.GetCodeString());
    EXPECT_EQ(ui::EventType::kKeyPressed, received.type());
    EXPECT_EQ(0, ui::EF_SHIFT_DOWN & received.flags());
    EXPECT_EQ(0, ui::EF_CONTROL_DOWN & received.flags());
    EXPECT_EQ(0, ui::EF_ALT_DOWN & received.flags());
    EXPECT_EQ(0, ui::EF_CAPS_LOCK_ON & received.flags());
  }

  {
    auto sent = std::make_unique<ui::KeyEvent>(
        ui::EventType::kKeyReleased, ui::VKEY_A,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_CAPS_LOCK_ON);

    connection->SendKeyEvent(std::move(sent));
    const std::vector<ui::KeyEvent>& sent_key_events =
        context_handler()->sent_key_events();
    EXPECT_EQ(2u, sent_key_events.size());
    const ui::KeyEvent& received = sent_key_events.back();
    EXPECT_EQ(ui::VKEY_A, received.key_code());
    EXPECT_EQ(ui::DomCode::US_A, received.code());
    EXPECT_EQ("KeyA", received.GetCodeString());
    EXPECT_EQ(ui::EventType::kKeyReleased, received.type());
    EXPECT_NE(0, ui::EF_SHIFT_DOWN & received.flags());
    EXPECT_NE(0, ui::EF_CONTROL_DOWN & received.flags());
    EXPECT_NE(0, ui::EF_ALT_DOWN & received.flags());
    EXPECT_NE(0, ui::EF_CAPS_LOCK_ON & received.flags());
  }
  engine()->Blur();
}

TEST_F(InputConnectionImplTest, SetCompositionRange) {
  auto connection = CreateNewConnection(1);
  engine()->Focus(context());

  context_handler()->Reset();
  client()->SetText("abcde");
  // ab|cde
  client()->SetCursorPos(2);
  // a[b|cd]e
  connection->SetCompositionRange(gfx::Range(1, 4));
  EXPECT_EQ(1u, context_handler()->composition_range_history().size());
  EXPECT_EQ(std::make_tuple(1, 4),
            context_handler()->composition_range_history().back());

  engine()->Blur();
}

TEST_F(InputConnectionImplTest, InputContextHandlerIsNull) {
  auto connection = CreateNewConnection(1);
  ash::IMEBridge::Get()->SetInputContextHandler(nullptr);

  connection->CommitText(u"text", 1);
  connection->DeleteSurroundingText(1, 1);
  connection->FinishComposingText();
  connection->SetComposingText(u"text", 0, std::nullopt);
  connection->SetSelection(gfx::Range(2, 4));
  connection->GetTextInputState(true);
}

}  // namespace arc
