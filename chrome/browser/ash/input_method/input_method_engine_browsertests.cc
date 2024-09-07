// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <memory>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/input_method/assistive_window_controller.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/input_method/input_method_menu_item.h"
#include "chrome/browser/ui/ash/input_method/input_method_menu_manager.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/ash/component_extension_ime_manager.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/input_method_descriptor.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/mock_ime_candidate_window_handler.h"
#include "ui/base/ime/ash/mock_ime_input_context_handler.h"
#include "ui/base/ime/ash/text_input_method.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/widget_test.h"

namespace ash {
namespace input_method {
namespace {

const char kIdentityIMEID[] =
    "_ext_ime_iafoklpfplgfnoimmaejoeondnjnlcfpIdentityIME";
const char kToUpperIMEID[] =
    "_ext_ime_iafoklpfplgfnoimmaejoeondnjnlcfpToUpperIME";
const char kAPIArgumentIMEID[] =
    "_ext_ime_iafoklpfplgfnoimmaejoeondnjnlcfpAPIArgumentIME";

// InputMethod extension should work on 1)normal extension, 2)normal extension
// in incognito mode 3)component extension.
enum TestType {
  kTestTypeNormal = 0,
  kTestTypeIncognito = 1,
  kTestTypeComponent = 2,
};

class InputMethodEngineBrowserTest
    : public extensions::ExtensionBrowserTest,
      public ::testing::WithParamInterface<TestType> {
 public:
  InputMethodEngineBrowserTest() = default;

  InputMethodEngineBrowserTest(const InputMethodEngineBrowserTest&) = delete;
  InputMethodEngineBrowserTest& operator=(const InputMethodEngineBrowserTest&) =
      delete;

  virtual ~InputMethodEngineBrowserTest() = default;

  void TearDownInProcessBrowserTestFixture() override { extension_ = nullptr; }

  TextInputMethod::InputContext CreateInputContextWithInputType(
      ui::TextInputType type) {
    return TextInputMethod::InputContext(type);
  }

 protected:
  void LoadTestInputMethod() {
    // This will load "chrome/test/data/extensions/input_ime"
    ExtensionTestMessageListener ime_ready_listener("ReadyToUseImeEvent");
    extension_ = LoadExtensionWithType("input_ime", GetParam());
    ASSERT_TRUE(extension_);
    ASSERT_TRUE(ime_ready_listener.WaitUntilSatisfied());

    // Extension IMEs are not enabled by default.
    std::vector<std::string> extension_ime_ids;
    extension_ime_ids.push_back(kIdentityIMEID);
    extension_ime_ids.push_back(kToUpperIMEID);
    extension_ime_ids.push_back(kAPIArgumentIMEID);
    InputMethodManager::Get()->GetActiveIMEState()->SetEnabledExtensionImes(
        extension_ime_ids);

    InputMethodDescriptors extension_imes;
    InputMethodManager::Get()->GetActiveIMEState()->GetInputMethodExtensions(
        &extension_imes);

    // Test IME has two input methods, thus InputMethodManager should have two
    // extension IME.
    // Note: Even extension is loaded by LoadExtensionAsComponent as above, the
    // IME does not managed by ComponentExtensionIMEManager or it's id won't
    // start with __comp__. The component extension IME is allowlisted and
    // managed by ComponentExtensionIMEManager, but its framework is same as
    // normal extension IME.
    EXPECT_EQ(3U, extension_imes.size());
  }

  const extensions::Extension* LoadExtensionWithType(
      const std::string& extension_name,
      TestType type) {
    switch (type) {
      case kTestTypeNormal:
        return LoadExtension(test_data_dir_.AppendASCII(extension_name));
      case kTestTypeIncognito:
        return LoadExtension(test_data_dir_.AppendASCII(extension_name),
                             {.allow_in_incognito = true});
      case kTestTypeComponent:
        return LoadExtensionAsComponent(
            test_data_dir_.AppendASCII(extension_name));
    }
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  raw_ptr<const extensions::Extension, DanglingUntriaged> extension_;
};

class KeyEventDoneCallback {
 public:
  explicit KeyEventDoneCallback(ui::ime::KeyEventHandledState expected_argument)
      : expected_argument_(expected_argument) {}

  KeyEventDoneCallback(const KeyEventDoneCallback&) = delete;
  KeyEventDoneCallback& operator=(const KeyEventDoneCallback&) = delete;

  ~KeyEventDoneCallback() = default;

  void Run(ui::ime::KeyEventHandledState consumed) {
    if (consumed == expected_argument_) {
      run_loop_.Quit();
    }
  }

  void WaitUntilCalled() { run_loop_.Run(); }

 private:
  ui::ime::KeyEventHandledState expected_argument_;
  base::RunLoop run_loop_;
};

class TestTextInputClient : public ui::DummyTextInputClient {
 public:
  explicit TestTextInputClient(ui::TextInputType type)
      : ui::DummyTextInputClient(type) {}

  TestTextInputClient(const TestTextInputClient&) = delete;
  TestTextInputClient& operator=(const TestTextInputClient&) = delete;

  ~TestTextInputClient() override = default;

  void WaitUntilCalled() { run_loop_.Run(); }

  const std::u16string& inserted_text() const { return inserted_text_; }

 private:
  // ui::DummyTextInputClient:
  bool ShouldDoLearning() override { return true; }
  void InsertText(const std::u16string& text,
                  InsertTextCursorBehavior cursor_behavior) override {
    inserted_text_ = text;
    run_loop_.Quit();
  }

  std::u16string inserted_text_;
  base::RunLoop run_loop_;
};

INSTANTIATE_TEST_SUITE_P(InputMethodEngineBrowserTest,
                         InputMethodEngineBrowserTest,
                         ::testing::Values(kTestTypeNormal));
INSTANTIATE_TEST_SUITE_P(InputMethodEngineIncognitoBrowserTest,
                         InputMethodEngineBrowserTest,
                         ::testing::Values(kTestTypeIncognito));
INSTANTIATE_TEST_SUITE_P(InputMethodEngineComponentExtensionBrowserTest,
                         InputMethodEngineBrowserTest,
                         ::testing::Values(kTestTypeComponent));

IN_PROC_BROWSER_TEST_P(InputMethodEngineBrowserTest, BasicScenarioTest) {
  LoadTestInputMethod();

  auto mock_input_context = std::make_unique<MockIMEInputContextHandler>();
  std::unique_ptr<MockIMECandidateWindowHandler> mock_candidate_window(
      new MockIMECandidateWindowHandler());

  IMEBridge::Get()->SetInputContextHandler(mock_input_context.get());
  IMEBridge::Get()->SetCandidateWindowHandler(mock_candidate_window.get());

  // onActivate event should be fired when changing input methods.
  ExtensionTestMessageListener activated_listener("onActivate");
  InputMethodManager::Get()->GetActiveIMEState()->ChangeInputMethod(
      kIdentityIMEID, false /* show_message */);
  ASSERT_TRUE(activated_listener.WaitUntilSatisfied());
  ASSERT_TRUE(activated_listener.was_satisfied());

  TextInputMethod* engine_handler = IMEBridge::Get()->GetCurrentEngineHandler();
  ASSERT_TRUE(engine_handler);

  // onFocus event should be fired if Focus function is called.
  ExtensionTestMessageListener focus_listener(
      "onFocus:text:true:true:true:false");
  const auto context =
      CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_TEXT);
  engine_handler->Focus(context);
  ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
  ASSERT_TRUE(focus_listener.was_satisfied());

  // onKeyEvent should be fired if ProcessKeyEvent is called.
  KeyEventDoneCallback callback(
      ui::ime::KeyEventHandledState::kNotHandled);  // EchoBackIME doesn't
                                                    // consume keys.
  ExtensionTestMessageListener keyevent_listener("onKeyEvent");
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  TextInputMethod::KeyEventDoneCallback keyevent_callback =
      base::BindOnce(&KeyEventDoneCallback::Run, base::Unretained(&callback));
  engine_handler->ProcessKeyEvent(key_event, std::move(keyevent_callback));
  ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
  ASSERT_TRUE(keyevent_listener.was_satisfied());
  callback.WaitUntilCalled();

  // onSurroundingTextChange should be fired if SetSurroundingText is called.
  ExtensionTestMessageListener surrounding_text_listener(
      "onSurroundingTextChanged");
  engine_handler->SetSurroundingText(u"text",  // Surrounding text.
                                     gfx::Range(0, 1),
                                     0);  // offset position.
  ASSERT_TRUE(surrounding_text_listener.WaitUntilSatisfied());
  ASSERT_TRUE(surrounding_text_listener.was_satisfied());

  // onMenuItemActivated should be fired if PropertyActivate is called.
  ExtensionTestMessageListener property_listener("onMenuItemActivated");
  engine_handler->PropertyActivate("property_name");
  ASSERT_TRUE(property_listener.WaitUntilSatisfied());
  ASSERT_TRUE(property_listener.was_satisfied());

  // onReset should be fired if Reset is called.
  ExtensionTestMessageListener reset_listener("onReset");
  engine_handler->Reset();
  ASSERT_TRUE(reset_listener.WaitUntilSatisfied());
  ASSERT_TRUE(reset_listener.was_satisfied());

  // onBlur should be fired if Blur is called.
  ExtensionTestMessageListener blur_listener("onBlur");
  engine_handler->Blur();
  ASSERT_TRUE(blur_listener.WaitUntilSatisfied());
  ASSERT_TRUE(blur_listener.was_satisfied());

  // onDeactivated should be fired when changing input methods.
  ExtensionTestMessageListener disabled_listener("onDeactivated");
  InputMethodManager::Get()->GetActiveIMEState()->ChangeInputMethod(
      kAPIArgumentIMEID, false /* show_message */);
  ASSERT_TRUE(disabled_listener.WaitUntilSatisfied());
  ASSERT_TRUE(disabled_listener.was_satisfied());

  IMEBridge::Get()->SetInputContextHandler(nullptr);
  IMEBridge::Get()->SetCandidateWindowHandler(nullptr);
}

// Test is flaky. https://crbug.com/1462135.
IN_PROC_BROWSER_TEST_P(InputMethodEngineBrowserTest, DISABLED_APIArgumentTest) {
  // TODO(crbug.com/41455212): Makes real end to end test without mocking the
  // input context handler. The test should mock the TextInputClient instance
  // hooked up with `InputMethodAsh`, or even using the real `TextInputClient`
  // if possible.
  LoadTestInputMethod();

  InputMethodManager::Get()->GetActiveIMEState()->ChangeInputMethod(
      kAPIArgumentIMEID, false /* show_message */);

  auto mock_input_context = std::make_unique<MockIMEInputContextHandler>();
  std::unique_ptr<MockIMECandidateWindowHandler> mock_candidate_window(
      new MockIMECandidateWindowHandler());

  IMEBridge::Get()->SetInputContextHandler(mock_input_context.get());
  IMEBridge::Get()->SetCandidateWindowHandler(mock_candidate_window.get());

  TextInputMethod* engine_handler = IMEBridge::Get()->GetCurrentEngineHandler();
  ASSERT_TRUE(engine_handler);

  extensions::ExtensionHost* host =
      extensions::ProcessManager::Get(profile())->GetBackgroundHostForExtension(
          extension_->id());
  ASSERT_TRUE(host);
  GURL test_url = ui_test_utils::GetTestUrl(
      base::FilePath("extensions/api_test/input_method/typing/"),
      base::FilePath("test_page.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  engine_handler->Focus(
      CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_TEXT));

  {
    SCOPED_TRACE("KeyDown, Ctrl:No, Alt:No, AltGr:No, Shift:No, Caps:No");
    KeyEventDoneCallback callback(ui::ime::KeyEventHandledState::kNotHandled);
    const std::string expected_value =
        "onKeyEvent::true:keydown:a:KeyA:false:false:false:false:false";
    ExtensionTestMessageListener keyevent_listener(expected_value);

    ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A,
                           ui::DomCode::US_A, ui::EF_NONE);
    TextInputMethod::KeyEventDoneCallback keyevent_callback =
        base::BindOnce(&KeyEventDoneCallback::Run, base::Unretained(&callback));
    engine_handler->ProcessKeyEvent(key_event, std::move(keyevent_callback));
    ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
    EXPECT_TRUE(keyevent_listener.was_satisfied());
    callback.WaitUntilCalled();
  }
  {
    SCOPED_TRACE("KeyDown, Ctrl:Yes, Alt:No, AltGr:No, Shift:No, Caps:No");
    KeyEventDoneCallback callback(ui::ime::KeyEventHandledState::kNotHandled);
    const std::string expected_value =
        "onKeyEvent::true:keydown:a:KeyA:true:false:false:false:false";
    ExtensionTestMessageListener keyevent_listener(expected_value);

    ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A,
                           ui::DomCode::US_A, ui::EF_CONTROL_DOWN);
    TextInputMethod::KeyEventDoneCallback keyevent_callback =
        base::BindOnce(&KeyEventDoneCallback::Run, base::Unretained(&callback));
    engine_handler->ProcessKeyEvent(key_event, std::move(keyevent_callback));
    ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
    EXPECT_TRUE(keyevent_listener.was_satisfied());
    callback.WaitUntilCalled();
  }
  {
    SCOPED_TRACE("KeyDown, Ctrl:No, Alt:Yes, AltGr:No, Shift:No, Caps:No");
    KeyEventDoneCallback callback(ui::ime::KeyEventHandledState::kNotHandled);
    const std::string expected_value =
        "onKeyEvent::true:keydown:a:KeyA:false:true:false:false:false";
    ExtensionTestMessageListener keyevent_listener(expected_value);

    ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A,
                           ui::DomCode::US_A, ui::EF_ALT_DOWN);
    TextInputMethod::KeyEventDoneCallback keyevent_callback =
        base::BindOnce(&KeyEventDoneCallback::Run, base::Unretained(&callback));
    engine_handler->ProcessKeyEvent(key_event, std::move(keyevent_callback));
    ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
    EXPECT_TRUE(keyevent_listener.was_satisfied());
    callback.WaitUntilCalled();
  }
  {
    SCOPED_TRACE("KeyDown, Ctrl:No, Alt:No, AltGr:No, Shift:Yes, Caps:No");
    KeyEventDoneCallback callback(ui::ime::KeyEventHandledState::kNotHandled);
    const std::string expected_value =
        "onKeyEvent::true:keydown:A:KeyA:false:false:false:true:false";
    ExtensionTestMessageListener keyevent_listener(expected_value);

    ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A,
                           ui::DomCode::US_A, ui::EF_SHIFT_DOWN);
    TextInputMethod::KeyEventDoneCallback keyevent_callback =
        base::BindOnce(&KeyEventDoneCallback::Run, base::Unretained(&callback));
    engine_handler->ProcessKeyEvent(key_event, std::move(keyevent_callback));
    ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
    EXPECT_TRUE(keyevent_listener.was_satisfied());
    callback.WaitUntilCalled();
  }
  {
    SCOPED_TRACE("KeyDown, Ctrl:No, Alt:No, AltGr:No, Shift:No, Caps:Yes");
    KeyEventDoneCallback callback(ui::ime::KeyEventHandledState::kNotHandled);
    const std::string expected_value =
        "onKeyEvent::true:keydown:A:KeyA:false:false:false:false:true";
    ExtensionTestMessageListener keyevent_listener(expected_value);

    ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A,
                           ui::DomCode::US_A, ui::EF_CAPS_LOCK_ON);
    TextInputMethod::KeyEventDoneCallback keyevent_callback =
        base::BindOnce(&KeyEventDoneCallback::Run, base::Unretained(&callback));
    engine_handler->ProcessKeyEvent(key_event, std::move(keyevent_callback));
    ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
    EXPECT_TRUE(keyevent_listener.was_satisfied());
    callback.WaitUntilCalled();
  }
  {
    SCOPED_TRACE("KeyDown, Ctrl:Yes, Alt:Yes, AltGr:No, Shift:No, Caps:No");
    KeyEventDoneCallback callback(ui::ime::KeyEventHandledState::kNotHandled);
    const std::string expected_value =
        "onKeyEvent::true:keydown:a:KeyA:true:true:false:false:false";
    ExtensionTestMessageListener keyevent_listener(expected_value);

    ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A,
                           ui::DomCode::US_A,
                           ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN);
    TextInputMethod::KeyEventDoneCallback keyevent_callback =
        base::BindOnce(&KeyEventDoneCallback::Run, base::Unretained(&callback));
    engine_handler->ProcessKeyEvent(key_event, std::move(keyevent_callback));
    ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
    EXPECT_TRUE(keyevent_listener.was_satisfied());
    callback.WaitUntilCalled();
  }
  {
    SCOPED_TRACE("KeyDown, Ctrl:No, Alt:No, AltGr:No, Shift:Yes, Caps:Yes");
    KeyEventDoneCallback callback(ui::ime::KeyEventHandledState::kNotHandled);
    const std::string expected_value =
        "onKeyEvent::true:keydown:a:KeyA:false:false:false:true:true";
    ExtensionTestMessageListener keyevent_listener(expected_value);

    ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A,
                           ui::DomCode::US_A,
                           ui::EF_SHIFT_DOWN | ui::EF_CAPS_LOCK_ON);
    TextInputMethod::KeyEventDoneCallback keyevent_callback =
        base::BindOnce(&KeyEventDoneCallback::Run, base::Unretained(&callback));
    engine_handler->ProcessKeyEvent(key_event, std::move(keyevent_callback));
    ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
    EXPECT_TRUE(keyevent_listener.was_satisfied());
    callback.WaitUntilCalled();
  }
  {
    SCOPED_TRACE("KeyDown, Ctrl:No, Alt:No, AltGr:Yes, Shift:No, Caps:No");
    KeyEventDoneCallback callback(ui::ime::KeyEventHandledState::kNotHandled);
    const std::string expected_value =
        "onKeyEvent::true:keydown:a:KeyA:false:false:true:false:false";
    ExtensionTestMessageListener keyevent_listener(expected_value);

    ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A,
                           ui::DomCode::US_A, ui::EF_ALTGR_DOWN);
    TextInputMethod::KeyEventDoneCallback keyevent_callback =
        base::BindOnce(&KeyEventDoneCallback::Run, base::Unretained(&callback));
    engine_handler->ProcessKeyEvent(key_event, std::move(keyevent_callback));
    ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
    EXPECT_TRUE(keyevent_listener.was_satisfied());
    callback.WaitUntilCalled();
  }
  // Media keys cases.
  const struct {
    ui::KeyboardCode keycode;
    const char* code;
    const char* key;
  } kMediaKeyCases[] = {
      {ui::VKEY_BROWSER_BACK, "BrowserBack", "HistoryBack"},
      {ui::VKEY_BROWSER_FORWARD, "BrowserForward", "HistoryForward"},
      {ui::VKEY_BROWSER_REFRESH, "BrowserRefresh", "BrowserRefresh"},
      {ui::VKEY_ZOOM, "ChromeOSFullscreen", "ChromeOSFullscreen"},
      {ui::VKEY_MEDIA_LAUNCH_APP1, "ChromeOSSwitchWindow",
       "ChromeOSSwitchWindow"},
      {ui::VKEY_BRIGHTNESS_DOWN, "BrightnessDown", "BrightnessDown"},
      {ui::VKEY_BRIGHTNESS_UP, "BrightnessUp", "BrightnessUp"},
      {ui::VKEY_VOLUME_MUTE, "VolumeMute", "AudioVolumeMute"},
      {ui::VKEY_VOLUME_DOWN, "VolumeDown", "AudioVolumeDown"},
      {ui::VKEY_VOLUME_UP, "VolumeUp", "AudioVolumeUp"},
      {ui::VKEY_F1, "F1", "HistoryBack"},
      {ui::VKEY_F2, "F2", "HistoryForward"},
      {ui::VKEY_F3, "F3", "BrowserRefresh"},
      {ui::VKEY_F4, "F4", "ChromeOSFullscreen"},
      {ui::VKEY_F5, "F5", "ChromeOSSwitchWindow"},
      {ui::VKEY_F6, "F6", "BrightnessDown"},
      {ui::VKEY_F7, "F7", "BrightnessUp"},
      {ui::VKEY_F8, "F8", "AudioVolumeMute"},
      {ui::VKEY_F9, "F9", "AudioVolumeDown"},
      {ui::VKEY_F10, "F10", "AudioVolumeUp"},
  };

  for (size_t i = 0; i < std::size(kMediaKeyCases); ++i) {
    SCOPED_TRACE(std::string("KeyDown, ") + kMediaKeyCases[i].code);
    KeyEventDoneCallback callback(ui::ime::KeyEventHandledState::kNotHandled);
    const std::string expected_value = base::StringPrintf(
        "onKeyEvent::true:keydown:%s:%s:false:false:false:false:false",
        kMediaKeyCases[i].key, kMediaKeyCases[i].code);
    ExtensionTestMessageListener keyevent_listener(expected_value);

    ui::KeyEvent key_event(
        ui::EventType::kKeyPressed, kMediaKeyCases[i].keycode,
        ui::KeycodeConverter::CodeStringToDomCode(kMediaKeyCases[i].code),
        ui::EF_NONE);
    TextInputMethod::KeyEventDoneCallback keyevent_callback =
        base::BindOnce(&KeyEventDoneCallback::Run, base::Unretained(&callback));
    engine_handler->ProcessKeyEvent(key_event, std::move(keyevent_callback));
    ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
    EXPECT_TRUE(keyevent_listener.was_satisfied());
    callback.WaitUntilCalled();
  }
  // TODO(nona): Add browser tests for other API as well.
  {
    SCOPED_TRACE("commitText test");
    mock_input_context->Reset();
    mock_candidate_window->Reset();

    const char commit_text_test_script[] =
        "chrome.input.ime.commitText({"
        "  contextID: engineBridge.getFocusedContextID().contextID,"
        "  text:'COMMIT_TEXT'"
        "});";

    ASSERT_TRUE(
        content::ExecJs(host->host_contents(), commit_text_test_script));
    EXPECT_EQ(1, mock_input_context->commit_text_call_count());
    EXPECT_EQ(u"COMMIT_TEXT", mock_input_context->last_commit_text());
  }
  {
    SCOPED_TRACE("sendKeyEvents test");
    mock_input_context->Reset();
    mock_candidate_window->Reset();

    const char send_key_events_test_script[] =
        "chrome.input.ime.sendKeyEvents({"
        "  contextID: engineBridge.getFocusedContextID().contextID,"
        "  keyData : [{"
        "    type : 'keydown',"
        "    key : 'z',"
        "    code : 'KeyZ',"
        "  }]"
        "});";

    ASSERT_TRUE(
        content::ExecJs(host->host_contents(), send_key_events_test_script));

    ASSERT_EQ(1u, mock_input_context->sent_key_events().size());
    const ui::KeyEvent& key_event =
        mock_input_context->sent_key_events().back();
    EXPECT_EQ(ui::EventType::kKeyPressed, key_event.type());
    EXPECT_EQ(L'z', key_event.GetCharacter());
    EXPECT_EQ(ui::DomCode::US_Z, key_event.code());
    EXPECT_EQ(ui::VKEY_Z, key_event.key_code());
    EXPECT_EQ(0, key_event.flags());
  }
  {
    SCOPED_TRACE("sendKeyEvents test with keyCode");
    mock_input_context->Reset();
    mock_candidate_window->Reset();

    const char send_key_events_test_script[] =
        "chrome.input.ime.sendKeyEvents({"
        "  contextID: engineBridge.getFocusedContextID().contextID,"
        "  keyData : [{"
        "    type : 'keyup',"
        "    key : 'a',"
        "    code : 'KeyQ',"
        "    keyCode : 0x41,"
        "  }]"
        "});";

    ASSERT_TRUE(
        content::ExecJs(host->host_contents(), send_key_events_test_script));

    ASSERT_EQ(1u, mock_input_context->sent_key_events().size());
    const ui::KeyEvent& key_event =
        mock_input_context->sent_key_events().back();
    EXPECT_EQ(ui::EventType::kKeyReleased, key_event.type());
    EXPECT_EQ(L'a', key_event.GetCharacter());
    EXPECT_EQ(ui::DomCode::US_Q, key_event.code());
    EXPECT_EQ(ui::VKEY_A, key_event.key_code());
    EXPECT_EQ(0, key_event.flags());
  }
  {
    SCOPED_TRACE("sendKeyEvents backwards compatible");
    mock_input_context->Reset();
    mock_candidate_window->Reset();

    const char send_key_events_test_script[] =
        "chrome.input.ime.sendKeyEvents({"
        "  contextID: engineBridge.getFocusedContextID().contextID,"
        "  keyData : [{"
        "    type : 'keyup',"
        "    requestId : '0',"
        "    key : 'a',"
        "    code : 'KeyQ',"
        "    keyCode : 0x41,"
        "  }]"
        "});";

    ASSERT_TRUE(
        content::ExecJs(host->host_contents(), send_key_events_test_script));

    ASSERT_EQ(1u, mock_input_context->sent_key_events().size());
    const ui::KeyEvent& key_event =
        mock_input_context->sent_key_events().back();
    EXPECT_EQ(ui::EventType::kKeyReleased, key_event.type());
    EXPECT_EQ(L'a', key_event.GetCharacter());
    EXPECT_EQ(ui::DomCode::US_Q, key_event.code());
    EXPECT_EQ(ui::VKEY_A, key_event.key_code());
    EXPECT_EQ(0, key_event.flags());
  }
  {
    SCOPED_TRACE("setComposition test");
    mock_input_context->Reset();
    mock_candidate_window->Reset();

    const char set_composition_test_script[] =
        "chrome.input.ime.setComposition({"
        "  contextID: engineBridge.getFocusedContextID().contextID,"
        "  text:'COMPOSITION_TEXT',"
        "  cursor:4,"
        "  segments : [{"
        "    start: 0,"
        "    end: 5,"
        "    style: 'underline'"
        "  },{"
        "    start: 6,"
        "    end: 10,"
        "    style: 'doubleUnderline'"
        "  }]"
        "});";

    ASSERT_TRUE(
        content::ExecJs(host->host_contents(), set_composition_test_script));
    EXPECT_EQ(1, mock_input_context->update_preedit_text_call_count());

    EXPECT_EQ(
        4U,
        mock_input_context->last_update_composition_arg().selection.start());
    EXPECT_EQ(
        4U, mock_input_context->last_update_composition_arg().selection.end());
    EXPECT_TRUE(mock_input_context->last_update_composition_arg().is_visible);

    const ui::CompositionText& composition_text =
        mock_input_context->last_update_composition_arg().composition_text;
    EXPECT_EQ(u"COMPOSITION_TEXT", composition_text.text);
    const ui::ImeTextSpans ime_text_spans = composition_text.ime_text_spans;

    ASSERT_EQ(2U, ime_text_spans.size());
    // single underline
    EXPECT_EQ(SK_ColorTRANSPARENT, ime_text_spans[0].underline_color);
    EXPECT_EQ(ui::ImeTextSpan::Thickness::kThin, ime_text_spans[0].thickness);
    EXPECT_EQ(0U, ime_text_spans[0].start_offset);
    EXPECT_EQ(5U, ime_text_spans[0].end_offset);

    // double underline
    EXPECT_EQ(SK_ColorTRANSPARENT, ime_text_spans[1].underline_color);
    EXPECT_EQ(ui::ImeTextSpan::Thickness::kThick, ime_text_spans[1].thickness);
    EXPECT_EQ(6U, ime_text_spans[1].start_offset);
    EXPECT_EQ(10U, ime_text_spans[1].end_offset);
  }
  {
    SCOPED_TRACE("clearComposition test");
    mock_input_context->Reset();
    mock_candidate_window->Reset();

    const char commite_text_test_script[] =
        "chrome.input.ime.clearComposition({"
        "  contextID: engineBridge.getFocusedContextID().contextID,"
        "});";

    ASSERT_TRUE(
        content::ExecJs(host->host_contents(), commite_text_test_script));
    EXPECT_EQ(1, mock_input_context->update_preedit_text_call_count());
    EXPECT_FALSE(mock_input_context->last_update_composition_arg().is_visible);
    const ui::CompositionText& composition_text =
        mock_input_context->last_update_composition_arg().composition_text;
    EXPECT_TRUE(composition_text.text.empty());
  }
  {
    SCOPED_TRACE(
        "setAssistiveWindowProperties:window_undo visibility_true test");

    const char set_assistive_window_test_script[] = R"(
      chrome.input.ime.setAssistiveWindowProperties({
        contextID: engineBridge.getFocusedContextID().contextID,
        properties: {
          type: 'undo',
          visible: true
        }
      });
    )";
    ASSERT_TRUE(content::ExecJs(host->host_contents(),
                                set_assistive_window_test_script));
    auto* assistive_window_controller = static_cast<AssistiveWindowController*>(
        IMEBridge::Get()->GetAssistiveWindowHandler());

    ui::ime::UndoWindow* undo_window =
        assistive_window_controller->GetUndoWindowForTesting();
    ASSERT_TRUE(undo_window);

    views::Widget* undo_window_widget = undo_window->GetWidget();
    ASSERT_TRUE(undo_window_widget);
    EXPECT_TRUE(undo_window_widget->IsVisible());
  }
  {
    SCOPED_TRACE(
        "setAssistiveWindowProperties:window_undo visibility_false test");

    const char set_assistive_window_test_script[] = R"(
      chrome.input.ime.setAssistiveWindowProperties({
        contextID: engineBridge.getFocusedContextID().contextID,
        properties: {
          type: 'undo',
          visible: false
        }
      });
    )";
    auto* assistive_window_controller = static_cast<AssistiveWindowController*>(
        IMEBridge::Get()->GetAssistiveWindowHandler());
    views::test::WidgetDestroyedWaiter waiter(
        assistive_window_controller->GetUndoWindowForTesting()->GetWidget());
    ASSERT_TRUE(content::ExecJs(host->host_contents(),
                                set_assistive_window_test_script));
    waiter.Wait();
    ui::ime::UndoWindow* undo_window =
        assistive_window_controller->GetUndoWindowForTesting();
    EXPECT_FALSE(undo_window);
  }
  {
    SCOPED_TRACE(
        "setAssistiveWindowProperties:window_undo visibility_false test");

    const char set_assistive_window_test_script[] = R"(
      chrome.input.ime.setAssistiveWindowProperties({
        contextID: engineBridge.getFocusedContextID().contextID,
        properties: {
          type: 'undo',
          visible: true
        }
      });
    )";
    ASSERT_TRUE(content::ExecJs(host->host_contents(),
                                set_assistive_window_test_script));
    auto* assistive_window_controller = static_cast<AssistiveWindowController*>(
        IMEBridge::Get()->GetAssistiveWindowHandler());

    ui::ime::UndoWindow* undo_window =
        assistive_window_controller->GetUndoWindowForTesting();
    ASSERT_TRUE(undo_window);
    ExtensionTestMessageListener button_listener(
        "undo button in undo window clicked");

    aura::Window* window = browser()->window()->GetNativeWindow();
    ui::test::EventGenerator event_generator(window->GetRootWindow());
    views::Button* undo_button = undo_window->GetUndoButtonForTesting();
    event_generator.MoveMouseTo(undo_button->GetBoundsInScreen().CenterPoint());
    event_generator.ClickLeftButton();

    ASSERT_TRUE(button_listener.WaitUntilSatisfied());
    EXPECT_TRUE(button_listener.was_satisfied());
  }
  {
    SCOPED_TRACE(
        "setAssistiveWindowButtonHighlighted:button_undo highlighted_true "
        "test");
    const char set_assistive_window_test_script[] = R"(
      chrome.input.ime.setAssistiveWindowProperties({
        contextID: engineBridge.getFocusedContextID().contextID,
        properties: {
          type: 'undo',
          visible: true
        }
      });
    )";
    const char set_assistive_window_button_highlighted_test_script[] = R"(
      chrome.input.ime.setAssistiveWindowButtonHighlighted({
        contextID: engineBridge.getFocusedContextID().contextID,
        buttonID: 'undo',
        windowType: 'undo',
        announceString: 'undo button highlighted',
        highlighted: true
      });
    )";
    ASSERT_TRUE(content::ExecJs(host->host_contents(),
                                set_assistive_window_test_script));
    ASSERT_TRUE(
        content::ExecJs(host->host_contents(),
                        set_assistive_window_button_highlighted_test_script));
    auto* assistive_window_controller = static_cast<AssistiveWindowController*>(
        IMEBridge::Get()->GetAssistiveWindowHandler());

    ui::ime::UndoWindow* undo_window =
        assistive_window_controller->GetUndoWindowForTesting();
    ASSERT_TRUE(undo_window);

    views::Button* undo_button = undo_window->GetUndoButtonForTesting();

    EXPECT_TRUE(undo_button->background() != nullptr);
  }
  {
    SCOPED_TRACE(
        "setAssistiveWindowButtonHighlighted:button_undo highlighted_false "
        "test");

    const char set_assistive_window_test_script[] = R"(
      chrome.input.ime.setAssistiveWindowProperties({
        contextID: engineBridge.getFocusedContextID().contextID,
        properties: {
          type: 'undo',
          visible: true
        }
      });
    )";
    const char set_assistive_window_button_highlighted_test_script[] = R"(
      chrome.input.ime.setAssistiveWindowButtonHighlighted({
        contextID: engineBridge.getFocusedContextID().contextID,
        buttonID: 'undo',
        windowType: 'undo',
        highlighted: false
      });
    )";
    ASSERT_TRUE(content::ExecJs(host->host_contents(),
                                set_assistive_window_test_script));
    ASSERT_TRUE(
        content::ExecJs(host->host_contents(),
                        set_assistive_window_button_highlighted_test_script));
    auto* assistive_window_controller = static_cast<AssistiveWindowController*>(
        IMEBridge::Get()->GetAssistiveWindowHandler());

    ui::ime::UndoWindow* undo_window =
        assistive_window_controller->GetUndoWindowForTesting();
    ASSERT_TRUE(undo_window);

    views::Button* undo_button = undo_window->GetUndoButtonForTesting();

    EXPECT_TRUE(undo_button->background() == nullptr);
  }
  {
    SCOPED_TRACE("setCandidateWindowProperties:visibility test");
    mock_input_context->Reset();
    mock_candidate_window->Reset();

    const char set_candidate_window_properties_test_script[] =
        "chrome.input.ime.setCandidateWindowProperties({"
        "  engineID: engineBridge.getActiveEngineID(),"
        "  properties: {"
        "    visible: true,"
        "  }"
        "});";
    ASSERT_TRUE(content::ExecJs(host->host_contents(),
                                set_candidate_window_properties_test_script));
    EXPECT_EQ(1, mock_candidate_window->update_lookup_table_call_count());
    EXPECT_TRUE(
        mock_candidate_window->last_update_lookup_table_arg().is_visible);
  }
  {
    SCOPED_TRACE("setCandidateWindowProperties:cursor_visibility test");
    mock_input_context->Reset();
    mock_candidate_window->Reset();

    const char set_candidate_window_properties_test_script[] =
        "chrome.input.ime.setCandidateWindowProperties({"
        "  engineID: engineBridge.getActiveEngineID(),"
        "  properties: {"
        "    cursorVisible: true,"
        "  }"
        "});";
    ASSERT_TRUE(content::ExecJs(host->host_contents(),
                                set_candidate_window_properties_test_script));
    EXPECT_EQ(1, mock_candidate_window->update_lookup_table_call_count());

    // window visibility is kept as before.
    EXPECT_TRUE(
        mock_candidate_window->last_update_lookup_table_arg().is_visible);

    const ui::CandidateWindow& table =
        mock_candidate_window->last_update_lookup_table_arg().lookup_table;
    EXPECT_TRUE(table.is_cursor_visible());
  }
  {
    SCOPED_TRACE("setCandidateWindowProperties:vertical test");
    mock_input_context->Reset();
    mock_candidate_window->Reset();

    const char set_candidate_window_properties_test_script[] =
        "chrome.input.ime.setCandidateWindowProperties({"
        "  engineID: engineBridge.getActiveEngineID(),"
        "  properties: {"
        "    vertical: true,"
        "  }"
        "});";
    ASSERT_TRUE(content::ExecJs(host->host_contents(),
                                set_candidate_window_properties_test_script));
    EXPECT_EQ(1, mock_candidate_window->update_lookup_table_call_count());

    // window visibility is kept as before.
    EXPECT_TRUE(
        mock_candidate_window->last_update_lookup_table_arg().is_visible);

    const ui::CandidateWindow& table =
        mock_candidate_window->last_update_lookup_table_arg().lookup_table;

    // cursor visibility is kept as before.
    EXPECT_TRUE(table.is_cursor_visible());

    EXPECT_EQ(ui::CandidateWindow::VERTICAL, table.orientation());
  }
  {
    SCOPED_TRACE("setCandidateWindowProperties:pageSize test");
    mock_input_context->Reset();
    mock_candidate_window->Reset();

    const char set_candidate_window_properties_test_script[] =
        "chrome.input.ime.setCandidateWindowProperties({"
        "  engineID: engineBridge.getActiveEngineID(),"
        "  properties: {"
        "    pageSize: 7,"
        "  }"
        "});";
    ASSERT_TRUE(content::ExecJs(host->host_contents(),
                                set_candidate_window_properties_test_script));
    EXPECT_EQ(1, mock_candidate_window->update_lookup_table_call_count());

    // window visibility is kept as before.
    EXPECT_TRUE(
        mock_candidate_window->last_update_lookup_table_arg().is_visible);

    const ui::CandidateWindow& table =
        mock_candidate_window->last_update_lookup_table_arg().lookup_table;

    // cursor visibility is kept as before.
    EXPECT_TRUE(table.is_cursor_visible());

    // oritantation is kept as before.
    EXPECT_EQ(ui::CandidateWindow::VERTICAL, table.orientation());

    EXPECT_EQ(7U, table.page_size());
  }
  {
    SCOPED_TRACE("setCandidateWindowProperties:auxTextVisibility test");
    mock_input_context->Reset();
    mock_candidate_window->Reset();

    const char set_candidate_window_properties_test_script[] =
        "chrome.input.ime.setCandidateWindowProperties({"
        "  engineID: engineBridge.getActiveEngineID(),"
        "  properties: {"
        "    auxiliaryTextVisible: true"
        "  }"
        "});";
    ASSERT_TRUE(content::ExecJs(host->host_contents(),
                                set_candidate_window_properties_test_script));
    EXPECT_EQ(1, mock_candidate_window->update_lookup_table_call_count());

    const ui::CandidateWindow& table =
        mock_candidate_window->last_update_lookup_table_arg().lookup_table;
    EXPECT_TRUE(table.is_auxiliary_text_visible());
  }
  {
    SCOPED_TRACE("setCandidateWindowProperties:auxText test");
    mock_input_context->Reset();
    mock_candidate_window->Reset();

    const char set_candidate_window_properties_test_script[] =
        "chrome.input.ime.setCandidateWindowProperties({"
        "  engineID: engineBridge.getActiveEngineID(),"
        "  properties: {"
        "    auxiliaryText: 'AUXILIARY_TEXT'"
        "  }"
        "});";
    ASSERT_TRUE(content::ExecJs(host->host_contents(),
                                set_candidate_window_properties_test_script));
    EXPECT_EQ(1, mock_candidate_window->update_lookup_table_call_count());

    // aux text visibility is kept as before.
    const ui::CandidateWindow& table =
        mock_candidate_window->last_update_lookup_table_arg().lookup_table;
    EXPECT_TRUE(table.is_auxiliary_text_visible());
    EXPECT_EQ("AUXILIARY_TEXT", table.auxiliary_text());
  }
  {
    SCOPED_TRACE("setCandidateWindowProperties:currentCandidateIndex test");
    mock_input_context->Reset();
    mock_candidate_window->Reset();

    const char set_candidate_window_properties_test_script[] =
        "chrome.input.ime.setCandidateWindowProperties({"
        "  engineID: engineBridge.getActiveEngineID(),"
        "  properties: {"
        "    currentCandidateIndex: 1"
        "  }"
        "});";
    ASSERT_TRUE(content::ExecJs(host->host_contents(),
                                set_candidate_window_properties_test_script));
    EXPECT_EQ(1, mock_candidate_window->update_lookup_table_call_count());

    const ui::CandidateWindow& table =
        mock_candidate_window->last_update_lookup_table_arg().lookup_table;
    EXPECT_EQ(1, table.current_candidate_index());
  }
  {
    SCOPED_TRACE("setCandidateWindowProperties:totalCandidates test");
    mock_input_context->Reset();
    mock_candidate_window->Reset();

    const char set_candidate_window_properties_test_script[] =
        "chrome.input.ime.setCandidateWindowProperties({"
        "  engineID: engineBridge.getActiveEngineID(),"
        "  properties: {"
        "    totalCandidates: 100"
        "  }"
        "});";
    ASSERT_TRUE(content::ExecJs(host->host_contents(),
                                set_candidate_window_properties_test_script));
    EXPECT_EQ(1, mock_candidate_window->update_lookup_table_call_count());

    const ui::CandidateWindow& table =
        mock_candidate_window->last_update_lookup_table_arg().lookup_table;
    EXPECT_EQ(100, table.total_candidates());
  }
  {
    SCOPED_TRACE("setCandidates test");
    mock_input_context->Reset();
    mock_candidate_window->Reset();

    const char set_candidates_test_script[] =
        "chrome.input.ime.setCandidates({"
        "  contextID: engineBridge.getFocusedContextID().contextID,"
        "  candidates: [{"
        "    candidate: 'CANDIDATE_1',"
        "    id: 1,"
        "    },{"
        "    candidate: 'CANDIDATE_2',"
        "    id: 2,"
        "    label: 'LABEL_2',"
        "    },{"
        "    candidate: 'CANDIDATE_3',"
        "    id: 3,"
        "    label: 'LABEL_3',"
        "    annotation: 'ANNOTACTION_3'"
        "    },{"
        "    candidate: 'CANDIDATE_4',"
        "    id: 4,"
        "    label: 'LABEL_4',"
        "    annotation: 'ANNOTACTION_4',"
        "    usage: {"
        "      title: 'TITLE_4',"
        "      body: 'BODY_4'"
        "    }"
        "  }]"
        "});";
    ASSERT_TRUE(
        content::ExecJs(host->host_contents(), set_candidates_test_script));

    // window visibility is kept as before.
    EXPECT_TRUE(
        mock_candidate_window->last_update_lookup_table_arg().is_visible);

    const ui::CandidateWindow& table =
        mock_candidate_window->last_update_lookup_table_arg().lookup_table;

    // cursor visibility is kept as before.
    EXPECT_TRUE(table.is_cursor_visible());

    // oritantation is kept as before.
    EXPECT_EQ(ui::CandidateWindow::VERTICAL, table.orientation());

    // page size is kept as before.
    EXPECT_EQ(7U, table.page_size());

    ASSERT_EQ(4U, table.candidates().size());

    EXPECT_EQ(u"CANDIDATE_1", table.candidates().at(0).value);

    EXPECT_EQ(u"CANDIDATE_2", table.candidates().at(1).value);
    EXPECT_EQ(u"LABEL_2", table.candidates().at(1).label);

    EXPECT_EQ(u"CANDIDATE_3", table.candidates().at(2).value);
    EXPECT_EQ(u"LABEL_3", table.candidates().at(2).label);
    EXPECT_EQ(u"ANNOTACTION_3", table.candidates().at(2).annotation);

    EXPECT_EQ(u"CANDIDATE_4", table.candidates().at(3).value);
    EXPECT_EQ(u"LABEL_4", table.candidates().at(3).label);
    EXPECT_EQ(u"ANNOTACTION_4", table.candidates().at(3).annotation);
    EXPECT_EQ(u"TITLE_4", table.candidates().at(3).description_title);
    EXPECT_EQ(u"BODY_4", table.candidates().at(3).description_body);
  }
  {
    SCOPED_TRACE("setCursorPosition test");
    mock_input_context->Reset();
    mock_candidate_window->Reset();

    const char set_cursor_position_test_script[] =
        "chrome.input.ime.setCursorPosition({"
        "  contextID: engineBridge.getFocusedContextID().contextID,"
        "  candidateID: 2"
        "});";
    ASSERT_TRUE(content::ExecJs(host->host_contents(),
                                set_cursor_position_test_script));
    EXPECT_EQ(1, mock_candidate_window->update_lookup_table_call_count());

    // window visibility is kept as before.
    EXPECT_TRUE(
        mock_candidate_window->last_update_lookup_table_arg().is_visible);

    const ui::CandidateWindow& table =
        mock_candidate_window->last_update_lookup_table_arg().lookup_table;

    // cursor visibility is kept as before.
    EXPECT_TRUE(table.is_cursor_visible());

    // oritantation is kept as before.
    EXPECT_EQ(ui::CandidateWindow::VERTICAL, table.orientation());

    // page size is kept as before.
    EXPECT_EQ(7U, table.page_size());

    // candidates are same as before.
    ASSERT_EQ(4U, table.candidates().size());

    // Candidate ID == 2 is 1 in index.
    EXPECT_EQ(1U, table.cursor_position());
  }
  {
    SCOPED_TRACE("setMenuItem test");
    mock_input_context->Reset();
    mock_candidate_window->Reset();

    const char set_menu_item_test_script[] =
        "chrome.input.ime.setMenuItems({"
        "  engineID: engineBridge.getActiveEngineID(),"
        "  items: [{"
        "    id: 'ID0',"
        "  },{"
        "    id: 'ID1',"
        "    label: 'LABEL1',"
        "  },{"
        "    id: 'ID2',"
        "    label: 'LABEL2',"
        "    style: 'radio',"
        "  },{"
        "    id: 'ID3',"
        "    label: 'LABEL3',"
        "    style: 'check',"
        "    visible: true,"
        "  },{"
        "    id: 'ID4',"
        "    label: 'LABEL4',"
        "    style: 'separator',"
        "    visible: true,"
        "    checked: true"
        "  }]"
        "});";
    ASSERT_TRUE(
        content::ExecJs(host->host_contents(), set_menu_item_test_script));

    const ui::ime::InputMethodMenuItemList& props =
        ui::ime::InputMethodMenuManager::GetInstance()
            ->GetCurrentInputMethodMenuItemList();
    ASSERT_EQ(5U, props.size());

    EXPECT_EQ("ID0", props[0].key);
    EXPECT_EQ("ID1", props[1].key);
    EXPECT_EQ("ID2", props[2].key);
    EXPECT_EQ("ID3", props[3].key);
    EXPECT_EQ("ID4", props[4].key);

    EXPECT_EQ("LABEL1", props[1].label);
    EXPECT_EQ("LABEL2", props[2].label);
    EXPECT_EQ("LABEL3", props[3].label);
    EXPECT_EQ("LABEL4", props[4].label);

    EXPECT_TRUE(props[4].is_selection_item_checked);
  }
  {
    SCOPED_TRACE("deleteSurroundingText test");
    mock_input_context->Reset();
    mock_candidate_window->Reset();

    const char delete_surrounding_text_test_script[] =
        "chrome.input.ime.deleteSurroundingText({"
        "  engineID: engineBridge.getActiveEngineID(),"
        "  contextID: engineBridge.getFocusedContextID().contextID,"
        "  offset: -1,"
        "  length: 3"
        "});";
    ASSERT_TRUE(content::ExecJs(host->host_contents(),
                                delete_surrounding_text_test_script));

    EXPECT_EQ(1, mock_input_context->delete_surrounding_text_call_count());
    EXPECT_EQ(1u, mock_input_context->last_delete_surrounding_text_arg()
                      .num_char16s_before_cursor);
    EXPECT_EQ(2u, mock_input_context->last_delete_surrounding_text_arg()
                      .num_char16s_after_cursor);
  }
  {
    SCOPED_TRACE("onFocus test");
    mock_input_context->Reset();
    mock_candidate_window->Reset();

    {
      ExtensionTestMessageListener focus_listener(
          "onFocus:text:true:true:true:false");
      engine_handler->Focus(
          CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_TEXT));
      ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
      ASSERT_TRUE(focus_listener.was_satisfied());
    }
    {
      // Verify that onfocus is called as if it is a password field, when the
      // text field was a password field in the past, but is now a text field.
      ExtensionTestMessageListener focus_listener(
          "onFocus:password:true:true:true:true");

      constexpr std::string_view password_field_change_to_text_script = R"(
        const input = document.createElement('input');
        document.body.appendChild(input);
        input.type = 'password';
        input.type = 'text';
        input.focus();
      )";

      ASSERT_TRUE(
          content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                          password_field_change_to_text_script));

      ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
      ASSERT_TRUE(focus_listener.was_satisfied());
    }
    {
      ExtensionTestMessageListener focus_listener(
          "onFocus:search:true:true:true:false");
      engine_handler->Focus(
          CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_SEARCH));
      ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
      ASSERT_TRUE(focus_listener.was_satisfied());
    }
    {
      ExtensionTestMessageListener focus_listener(
          "onFocus:tel:true:true:true:false");
      engine_handler->Focus(
          CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_TELEPHONE));
      ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
      ASSERT_TRUE(focus_listener.was_satisfied());
    }
    {
      ExtensionTestMessageListener focus_listener(
          "onFocus:url:true:true:true:false");
      engine_handler->Focus(
          CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_URL));
      ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
      ASSERT_TRUE(focus_listener.was_satisfied());
    }
    {
      ExtensionTestMessageListener focus_listener(
          "onFocus:email:true:true:true:false");
      engine_handler->Focus(
          CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_EMAIL));
      ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
      ASSERT_TRUE(focus_listener.was_satisfied());
    }
    {
      ExtensionTestMessageListener focus_listener(
          "onFocus:number:true:true:true:false");
      engine_handler->Focus(
          CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_NUMBER));
      ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
      ASSERT_TRUE(focus_listener.was_satisfied());
    }
  }
  {
    SCOPED_TRACE("changeInputMethod with uncommited text test");
    // For http://crbug.com/529999.

    mock_input_context->Reset();
    mock_candidate_window->Reset();

    EXPECT_TRUE(mock_input_context->last_commit_text().empty());

    const char set_composition_test_script[] =
        "chrome.input.ime.setComposition({"
        "  contextID: engineBridge.getFocusedContextID().contextID,"
        "  text:'us',"
        "  cursor:2,"
        "  segments : [{"
        "    start: 0,"
        "    end: 1,"
        "    style: 'underline'"
        "  }]"
        "});";

    ASSERT_TRUE(
        content::ExecJs(host->host_contents(), set_composition_test_script));
    EXPECT_EQ(
        2U,
        mock_input_context->last_update_composition_arg().selection.start());
    EXPECT_EQ(
        2U, mock_input_context->last_update_composition_arg().selection.end());
    EXPECT_TRUE(mock_input_context->last_update_composition_arg().is_visible);

    const ui::CompositionText& composition_text =
        mock_input_context->last_update_composition_arg().composition_text;
    EXPECT_EQ(u"us", composition_text.text);
    const ui::ImeTextSpans ime_text_spans = composition_text.ime_text_spans;

    ASSERT_EQ(1U, ime_text_spans.size());
    // single underline
    EXPECT_EQ(SK_ColorTRANSPARENT, ime_text_spans[0].underline_color);
    EXPECT_EQ(ui::ImeTextSpan::Thickness::kThin, ime_text_spans[0].thickness);
    EXPECT_EQ(0U, ime_text_spans[0].start_offset);
    EXPECT_EQ(1U, ime_text_spans[0].end_offset);
    EXPECT_TRUE(mock_input_context->last_commit_text().empty());

    InputMethodManager::Get()->GetActiveIMEState()->ChangeInputMethod(
        kIdentityIMEID, false /* show_message */);
    EXPECT_EQ(1, mock_input_context->commit_text_call_count());
    EXPECT_EQ(u"us", mock_input_context->last_commit_text());

    // Should not call CommitText anymore.
    InputMethodManager::Get()->GetActiveIMEState()->ChangeInputMethod(
        extension_ime_util::GetInputMethodIDByEngineID("zh-t-i0-pinyin"),
        false /* show_message */);
    EXPECT_EQ(1, mock_input_context->commit_text_call_count());

    InputMethodManager::Get()->GetActiveIMEState()->ChangeInputMethod(
        extension_ime_util::GetInputMethodIDByEngineID("xkb:us::eng"),
        false /* show_message */);
    EXPECT_EQ(1, mock_input_context->commit_text_call_count());
  }

  IMEBridge::Get()->SetInputContextHandler(nullptr);
  IMEBridge::Get()->SetCandidateWindowHandler(nullptr);
}

IN_PROC_BROWSER_TEST_P(InputMethodEngineBrowserTest, RestrictedKeyboard) {
  LoadTestInputMethod();

  InputMethodManager::Get()->GetActiveIMEState()->ChangeInputMethod(
      kAPIArgumentIMEID, false /* show_message */);

  auto mock_input_context = std::make_unique<MockIMEInputContextHandler>();
  std::unique_ptr<MockIMECandidateWindowHandler> mock_candidate_window(
      new MockIMECandidateWindowHandler());

  IMEBridge::Get()->SetInputContextHandler(mock_input_context.get());
  IMEBridge::Get()->SetCandidateWindowHandler(mock_candidate_window.get());

  TextInputMethod* engine_handler = IMEBridge::Get()->GetCurrentEngineHandler();
  ASSERT_TRUE(engine_handler);

  auto keyboard_config =
      ChromeKeyboardControllerClient::Get()->GetKeyboardConfig();
  // Turn off these features, which are on by default.
  keyboard_config.auto_correct = false;
  keyboard_config.auto_complete = false;
  keyboard_config.spell_check = false;
  ChromeKeyboardControllerClient::Get()->SetKeyboardConfig(keyboard_config);

  extensions::ExtensionHost* host =
      extensions::ProcessManager::Get(profile())->GetBackgroundHostForExtension(
          extension_->id());
  ASSERT_TRUE(host);

  {
    SCOPED_TRACE("Text");

    ExtensionTestMessageListener focus_listener(
        "onFocus:text:false:false:false:false");
    const auto context =
        CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_TEXT);
    engine_handler->Focus(context);
    ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
    ASSERT_TRUE(focus_listener.was_satisfied());
  }
  {
    SCOPED_TRACE("Password");

    ExtensionTestMessageListener focus_listener(
        "onFocus:password:false:false:false:false");
    const auto context =
        CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
    engine_handler->Focus(context);
    ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
    ASSERT_TRUE(focus_listener.was_satisfied());
  }
  {
    SCOPED_TRACE("URL");

    ExtensionTestMessageListener focus_listener(
        "onFocus:url:false:false:false:false");
    const auto context =
        CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_URL);
    engine_handler->Focus(context);
    ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
    ASSERT_TRUE(focus_listener.was_satisfied());
  }
  {
    SCOPED_TRACE("Search");

    ExtensionTestMessageListener focus_listener(
        "onFocus:search:false:false:false:false");
    const auto context =
        CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_SEARCH);
    engine_handler->Focus(context);
    ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
    ASSERT_TRUE(focus_listener.was_satisfied());
  }
  {
    SCOPED_TRACE("Email");

    ExtensionTestMessageListener focus_listener(
        "onFocus:email:false:false:false:false");
    const auto context =
        CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_EMAIL);
    engine_handler->Focus(context);
    ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
    ASSERT_TRUE(focus_listener.was_satisfied());
  }
  {
    SCOPED_TRACE("Number");

    ExtensionTestMessageListener focus_listener(
        "onFocus:number:false:false:false:false");
    const auto context =
        CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_NUMBER);
    engine_handler->Focus(context);
    ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
    ASSERT_TRUE(focus_listener.was_satisfied());
  }
  {
    SCOPED_TRACE("Telephone");

    ExtensionTestMessageListener focus_listener(
        "onFocus:tel:false:false:false:false");
    const auto context =
        CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_TELEPHONE);
    engine_handler->Focus(context);
    ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
    ASSERT_TRUE(focus_listener.was_satisfied());
  }

  IMEBridge::Get()->SetInputContextHandler(nullptr);
  IMEBridge::Get()->SetCandidateWindowHandler(nullptr);
}

IN_PROC_BROWSER_TEST_P(InputMethodEngineBrowserTest, ShouldDoLearning) {
  LoadTestInputMethod();

  InputMethodManager::Get()->GetActiveIMEState()->ChangeInputMethod(
      kIdentityIMEID, false /* show_message */);

  auto mock_input_context = std::make_unique<MockIMEInputContextHandler>();
  std::unique_ptr<MockIMECandidateWindowHandler> mock_candidate_window(
      new MockIMECandidateWindowHandler());

  IMEBridge::Get()->SetInputContextHandler(mock_input_context.get());
  IMEBridge::Get()->SetCandidateWindowHandler(mock_candidate_window.get());

  TextInputMethod* engine_handler = IMEBridge::Get()->GetCurrentEngineHandler();
  ASSERT_TRUE(engine_handler);

  // onFocus event should be fired if Focus function is called.
  ExtensionTestMessageListener focus_listener(
      "onFocus:text:true:true:true:true");
  auto context = CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_TEXT);
  context.personalization_mode = PersonalizationMode::kEnabled;
  engine_handler->Focus(context);
  ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
  ASSERT_TRUE(focus_listener.was_satisfied());

  IMEBridge::Get()->SetInputContextHandler(nullptr);
  IMEBridge::Get()->SetCandidateWindowHandler(nullptr);
}

IN_PROC_BROWSER_TEST_P(InputMethodEngineBrowserTest, MojoInteractionTest) {
  LoadTestInputMethod();

  InputMethodManager::Get()->GetActiveIMEState()->ChangeInputMethod(
      kAPIArgumentIMEID, false /* show_message */);

  ui::InputMethod* im =
      browser()->window()->GetNativeWindow()->GetHost()->GetInputMethod();
  TestTextInputClient tic(ui::TEXT_INPUT_TYPE_TEXT);

  {
    SCOPED_TRACE("Verifies onFocus event.");
    ExtensionTestMessageListener focus_listener(
        "onFocus:text:true:true:true:true");

    im->SetFocusedTextInputClient(&tic);

    ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
    ASSERT_TRUE(focus_listener.was_satisfied());
  }

  {
    SCOPED_TRACE("Verifies onKeyEvent event.");
    ExtensionTestMessageListener keydown_listener(
        "onKeyEvent::true:keydown:a:KeyA:false:false:false:false:false");

    EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_A, false,
                                                false, false, false));

    ASSERT_TRUE(keydown_listener.WaitUntilSatisfied());
    EXPECT_TRUE(keydown_listener.was_satisfied());
  }

  {
    SCOPED_TRACE("Verifies commitText call.");
    extensions::ExtensionHost* host =
        extensions::ProcessManager::Get(profile())
            ->GetBackgroundHostForExtension(extension_->id());
    const char commit_text_test_script[] =
        "chrome.input.ime.commitText({"
        "  contextID: engineBridge.getFocusedContextID().contextID,"
        "  text:'COMMIT_TEXT'"
        "});";
    ASSERT_TRUE(
        content::ExecJs(host->host_contents(), commit_text_test_script));
    tic.WaitUntilCalled();
    EXPECT_EQ(u"COMMIT_TEXT", tic.inserted_text());
  }

  {
    SCOPED_TRACE("Verifies onBlur event");
    ExtensionTestMessageListener blur_listener("onBlur");

    ui::FakeTextInputClient dtic(ui::TEXT_INPUT_TYPE_TEXT);
    im->SetFocusedTextInputClient(&dtic);

    ASSERT_TRUE(blur_listener.WaitUntilSatisfied());
    ASSERT_TRUE(blur_listener.was_satisfied());

    im->DetachTextInputClient(&dtic);
  }
}

}  // namespace
}  // namespace input_method
}  // namespace ash
