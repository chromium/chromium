// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/input_method_manager/input_connection_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/arc/input_method_manager/test_input_method_manager_bridge.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/mock_input_method_manager.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/mock_ime_input_context_handler.h"
#include "ui/base/ime/mock_input_method.h"
#include "ui/events/keycodes/dom/dom_codes.h"

namespace arc {

namespace {

class DummyInputMethodEngineObserver
    : public input_method::InputMethodEngineBase::Observer {
 public:
  DummyInputMethodEngineObserver() = default;
  ~DummyInputMethodEngineObserver() override = default;

  void OnActivate(const std::string& engine_id) override {}
  void OnFocus(
      const ui::IMEEngineHandlerInterface::InputContext& context) override {}
  void OnBlur(int context_id) override {}
  void OnKeyEvent(
      const std::string& engine_id,
      const input_method::InputMethodEngineBase::KeyboardEvent& event,
      ui::IMEEngineHandlerInterface::KeyEventDoneCallback key_data) override {}
  void OnReset(const std::string& engine_id) override {}
  void OnDeactivated(const std::string& engine_id) override {}
  void OnCompositionBoundsChanged(
      const std::vector<gfx::Rect>& bounds) override {}
  void OnSurroundingTextChanged(const std::string& engine_id,
                                const std::string& text,
                                int cursor_pos,
                                int anchor_pos,
                                int offset_pos) override {}
  void OnInputContextUpdate(
      const ui::IMEEngineHandlerInterface::InputContext& context) override {}
  void OnCandidateClicked(
      const std::string& component_id,
      int candidate_id,
      input_method::InputMethodEngineBase::MouseButtonEvent button) override {}
  void OnMenuItemActivated(const std::string& component_id,
                           const std::string& menu_id) override {}
  void OnScreenProjectionChanged(bool is_projected) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyInputMethodEngineObserver);
};

class TestInputMethodManager
    : public chromeos::input_method::MockInputMethodManager {
 public:
  TestInputMethodManager()
      : state_(base::MakeRefCounted<
               chromeos::input_method::MockInputMethodManager::State>()) {}
  ~TestInputMethodManager() override = default;

  scoped_refptr<InputMethodManager::State> GetActiveIMEState() override {
    return state_;
  }

 private:
  scoped_refptr<State> state_;

  DISALLOW_COPY_AND_ASSIGN(TestInputMethodManager);
};

class TestIMEInputContextHandler : public ui::MockIMEInputContextHandler {
 public:
  explicit TestIMEInputContextHandler(ui::InputMethod* input_method)
      : input_method_(input_method) {}
  ~TestIMEInputContextHandler() override = default;

  ui::InputMethod* GetInputMethod() override { return input_method_; }

  void SendKeyEvent(ui::KeyEvent* event) override {
    ui::MockIMEInputContextHandler::SendKeyEvent(event);
    ++send_key_event_call_count_;
  }

  bool SetCompositionRange(
      uint32_t before,
      uint32_t after,
      const std::vector<ui::ImeTextSpan>& text_spans) override {
    ui::MockIMEInputContextHandler::SetCompositionRange(before, after,
                                                        text_spans);
    composition_range_history_.push_back(std::make_tuple(before, after));
    return true;
  }

  void Reset() {
    ui::MockIMEInputContextHandler::Reset();
    send_key_event_call_count_ = 0;
    composition_range_history_.clear();
  }

  int send_key_event_call_count() const { return send_key_event_call_count_; }
  const std::vector<std::tuple<int, int>>& composition_range_history() {
    return composition_range_history_;
  }

 private:
  ui::InputMethod* const input_method_;

  int send_key_event_call_count_ = 0;
  std::vector<std::tuple<int, int>> composition_range_history_;

  DISALLOW_COPY_AND_ASSIGN(TestIMEInputContextHandler);
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
                        base::string16* text) const override {
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
  ~InputConnectionImplTest() override = default;

  std::unique_ptr<InputConnectionImpl> CreateNewConnection(int context_id) {
    return std::make_unique<InputConnectionImpl>(engine_.get(), bridge_.get(),
                                                 context_id);
  }

  chromeos::InputMethodEngine* engine() { return engine_.get(); }

  TestIMEInputContextHandler* context_handler() { return &context_handler_; }

  MockTextInputClient* client() { return &text_input_client_; }

  ui::IMEEngineHandlerInterface::InputContext context() {
    return ui::IMEEngineHandlerInterface::InputContext{
        1,
        ui::TEXT_INPUT_TYPE_TEXT,
        ui::TEXT_INPUT_MODE_DEFAULT,
        0 /* flags */,
        ui::TextInputClient::FOCUS_REASON_MOUSE,
        true /* should_do_learning */};
  }

  void SetUp() override {
    ui::IMEBridge::Initialize();
    chromeos::input_method::InputMethodManager::Initialize(
        new TestInputMethodManager);
    bridge_ = std::make_unique<TestInputMethodManagerBridge>();
    engine_ = std::make_unique<chromeos::InputMethodEngine>();
    engine_->Initialize(std::make_unique<DummyInputMethodEngineObserver>(),
                        "test_extension_id", nullptr);
    chrome_keyboard_controller_client_test_helper_ =
        ChromeKeyboardControllerClientTestHelper::InitializeWithFake();

    // Enable InputMethodEngine.
    ui::IMEBridge::Get()->SetInputContextHandler(&context_handler_);
    input_method_.SetFocusedTextInputClient(&text_input_client_);
    engine()->Enable("test_component_id");
  }

  void TearDown() override {
    chrome_keyboard_controller_client_test_helper_.reset();
    ui::IMEBridge::Get()->SetInputContextHandler(nullptr);
    engine_.reset();
    bridge_.reset();
    chromeos::input_method::InputMethodManager::Shutdown();
    ui::IMEBridge::Shutdown();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestInputMethodManagerBridge> bridge_;
  std::unique_ptr<chromeos::InputMethodEngine> engine_;
  MockTextInputClient text_input_client_;
  ui::MockInputMethod input_method_{nullptr};
  TestIMEInputContextHandler context_handler_{&input_method_};
  std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
      chrome_keyboard_controller_client_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(InputConnectionImplTest);
};

}  // anonymous namespace

TEST_F(InputConnectionImplTest, CommitText) {
  auto connection = CreateNewConnection(1);
  engine()->FocusIn(context());

  context_handler()->Reset();
  connection->CommitText(base::ASCIIToUTF16("text"), 1);
  EXPECT_EQ(1, context_handler()->commit_text_call_count());
  EXPECT_EQ("text", context_handler()->last_commit_text());

  // Calling Commit() with '\n' invokes SendKeyEvent.
  context_handler()->Reset();
  connection->CommitText(base::ASCIIToUTF16("\n"), 1);
  EXPECT_EQ(0, context_handler()->commit_text_call_count());
  EXPECT_EQ(2, context_handler()->send_key_event_call_count());
  EXPECT_EQ(ui::VKEY_RETURN,
            context_handler()->last_sent_key_event().key_code());
  EXPECT_EQ(ui::ET_KEY_RELEASED,
            context_handler()->last_sent_key_event().type());

  engine()->FocusOut();
}

TEST_F(InputConnectionImplTest, DeleteSurroundingText) {
  auto connection = CreateNewConnection(1);
  engine()->FocusIn(context());

  context_handler()->Reset();
  connection->DeleteSurroundingText(1, 1);
  EXPECT_EQ(1, context_handler()->delete_surrounding_text_call_count());

  engine()->FocusOut();
}

TEST_F(InputConnectionImplTest, FinishComposingText) {
  auto connection = CreateNewConnection(1);
  engine()->FocusIn(context());

  // If there is no composing text, FinishComposingText() does nothing.
  context_handler()->Reset();
  connection->FinishComposingText();
  EXPECT_EQ(0, context_handler()->commit_text_call_count());

  // If there is composing text, FinishComposingText() calls CommitText() with
  // the text.
  context_handler()->Reset();
  connection->SetComposingText(base::ASCIIToUTF16("composing"), 0,
                               base::nullopt);
  client()->SetText("composing");
  client()->SetCompositionRange(gfx::Range(0, 9));
  EXPECT_EQ(0, context_handler()->commit_text_call_count());
  connection->FinishComposingText();
  EXPECT_EQ(1, context_handler()->commit_text_call_count());
  EXPECT_EQ("composing", context_handler()->last_commit_text());

  client()->SetCompositionRange(gfx::Range(0, 0));
  connection->FinishComposingText();
  EXPECT_EQ(1, context_handler()->commit_text_call_count());

  engine()->FocusOut();
}

TEST_F(InputConnectionImplTest, SetComposingText) {
  const base::string16 text = base::ASCIIToUTF16("text");
  auto connection = CreateNewConnection(1);
  engine()->FocusIn(context());

  context_handler()->Reset();
  connection->SetComposingText(text, 0, base::nullopt);
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
      base::ASCIIToUTF16(""),
      context_handler()->last_update_composition_arg().composition_text.text);
  EXPECT_EQ(1, context_handler()->commit_text_call_count());

  // CommitText should clear the composing text.
  connection->FinishComposingText();
  // commit_text_call_count() doesn't change.
  EXPECT_EQ(1, context_handler()->commit_text_call_count());

  // Selection range
  context_handler()->Reset();
  connection->SetComposingText(text, 0, base::make_optional<gfx::Range>(1, 3));
  EXPECT_EQ(1u, context_handler()
                    ->last_update_composition_arg()
                    .composition_text.selection.start());
  EXPECT_EQ(3u, context_handler()
                    ->last_update_composition_arg()
                    .composition_text.selection.end());

  engine()->FocusOut();
}

TEST_F(InputConnectionImplTest, SetSelection) {
  auto connection = CreateNewConnection(1);
  engine()->FocusIn(context());
  ASSERT_TRUE(client()->selection_history().empty());

  context_handler()->Reset();
  connection->SetSelection(gfx::Range(2, 4));
  EXPECT_FALSE(client()->selection_history().empty());
  EXPECT_EQ(2u, client()->selection_history().back().start());
  EXPECT_EQ(4u, client()->selection_history().back().end());

  engine()->FocusOut();
}

TEST_F(InputConnectionImplTest, SendKeyEvent) {
  auto connection = CreateNewConnection(1);
  engine()->FocusIn(context());

  context_handler()->Reset();

  {
    mojom::KeyEventDataPtr data = mojom::KeyEventData::New();
    data->pressed = true;
    data->key_code = ui::VKEY_RETURN;
    data->is_shift_down = false;
    data->is_control_down = false;
    data->is_alt_down = false;
    data->is_capslock_on = false;

    connection->SendKeyEvent(std::move(data));
    EXPECT_EQ(1, context_handler()->send_key_event_call_count());
    const auto& event = context_handler()->last_sent_key_event();
    EXPECT_EQ(ui::VKEY_RETURN, event.key_code());
    EXPECT_EQ(ui::DomCode::ENTER, event.code());
    EXPECT_EQ("Enter", event.GetCodeString());
    EXPECT_EQ(ui::ET_KEY_PRESSED, event.type());
    EXPECT_EQ(0, ui::EF_SHIFT_DOWN & event.flags());
    EXPECT_EQ(0, ui::EF_CONTROL_DOWN & event.flags());
    EXPECT_EQ(0, ui::EF_ALT_DOWN & event.flags());
    EXPECT_EQ(0, ui::EF_CAPS_LOCK_ON & event.flags());
  }

  {
    mojom::KeyEventDataPtr data = mojom::KeyEventData::New();
    data->pressed = false;
    data->key_code = ui::VKEY_A;
    data->is_shift_down = true;
    data->is_control_down = true;
    data->is_alt_down = true;
    data->is_capslock_on = true;

    connection->SendKeyEvent(std::move(data));
    EXPECT_EQ(2, context_handler()->send_key_event_call_count());
    const auto& event = context_handler()->last_sent_key_event();
    EXPECT_EQ(ui::VKEY_A, event.key_code());
    EXPECT_EQ(ui::DomCode::US_A, event.code());
    EXPECT_EQ("KeyA", event.GetCodeString());
    EXPECT_EQ(ui::ET_KEY_RELEASED, event.type());
    EXPECT_NE(0, ui::EF_SHIFT_DOWN & event.flags());
    EXPECT_NE(0, ui::EF_CONTROL_DOWN & event.flags());
    EXPECT_NE(0, ui::EF_ALT_DOWN & event.flags());
    EXPECT_NE(0, ui::EF_CAPS_LOCK_ON & event.flags());
  }
  engine()->FocusOut();
}

TEST_F(InputConnectionImplTest, SetCompositionRange) {
  auto connection = CreateNewConnection(1);
  engine()->FocusIn(context());

  context_handler()->Reset();
  client()->SetText("abcde");
  // ab|cde
  client()->SetCursorPos(2);
  // a[b|cd]e
  connection->SetCompositionRange(gfx::Range(1, 4));
  EXPECT_EQ(1u, context_handler()->composition_range_history().size());
  EXPECT_EQ(std::make_tuple(1, 2),
            context_handler()->composition_range_history().back());

  engine()->FocusOut();
}

TEST_F(InputConnectionImplTest, InputContextHandlerIsNull) {
  auto connection = CreateNewConnection(1);
  ui::IMEBridge::Get()->SetInputContextHandler(nullptr);

  connection->CommitText(base::ASCIIToUTF16("text"), 1);
  connection->DeleteSurroundingText(1, 1);
  connection->FinishComposingText();
  connection->SetComposingText(base::ASCIIToUTF16("text"), 0, base::nullopt);
  connection->SetSelection(gfx::Range(2, 4));
  connection->GetTextInputState(true);
}

}  // namespace arc
