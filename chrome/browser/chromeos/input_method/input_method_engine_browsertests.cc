// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/mock_ime_candidate_window_handler.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/ime_engine_handler_interface.h"
#include "ui/base/ime/mock_ime_input_context_handler.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/chromeos/ime/input_method_menu_item.h"
#include "ui/chromeos/ime/input_method_menu_manager.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace chromeos {
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
  virtual ~InputMethodEngineBrowserTest() = default;

  void TearDownInProcessBrowserTestFixture() override { extension_ = NULL; }

  ui::IMEEngineHandlerInterface::InputContext CreateInputContextWithInputType(
      ui::TextInputType type) {
    return ui::IMEEngineHandlerInterface::InputContext(
        type, ui::TEXT_INPUT_MODE_DEFAULT, ui::TEXT_INPUT_FLAG_NONE,
        ui::TextInputClient::FOCUS_REASON_OTHER,
        false /* should_do_learning */);
  }

 protected:
  void LoadTestInputMethod() {
    // This will load "chrome/test/data/extensions/input_ime"
    ExtensionTestMessageListener ime_ready_listener("ReadyToUseImeEvent",
                                                    false);
    extension_ = LoadExtensionWithType("input_ime", GetParam());
    ASSERT_TRUE(extension_);
    ASSERT_TRUE(ime_ready_listener.WaitUntilSatisfied());

    // Extension IMEs are not enabled by default.
    std::vector<std::string> extension_ime_ids;
    extension_ime_ids.push_back(kIdentityIMEID);
    extension_ime_ids.push_back(kToUpperIMEID);
    extension_ime_ids.push_back(kAPIArgumentIMEID);
    InputMethodManager::Get()->GetActiveIMEState()->SetEnabledExtensionImes(
        &extension_ime_ids);

    InputMethodDescriptors extension_imes;
    InputMethodManager::Get()->GetActiveIMEState()->GetInputMethodExtensions(
        &extension_imes);

    // Test IME has two input methods, thus InputMethodManager should have two
    // extension IME.
    // Note: Even extension is loaded by LoadExtensionAsComponent as above, the
    // IME does not managed by ComponentExtensionIMEManager or it's id won't
    // start with __comp__. The component extension IME is whitelisted and
    // managed by ComponentExtensionIMEManager, but its framework is same as
    // normal extension IME.
    EXPECT_EQ(3U, extension_imes.size());
  }

  const extensions::Extension* LoadExtensionWithType(
      const std::string& extension_name, TestType type) {
    switch (type) {
      case kTestTypeNormal:
        return LoadExtension(test_data_dir_.AppendASCII(extension_name));
      case kTestTypeIncognito:
        return LoadExtensionIncognito(
            test_data_dir_.AppendASCII(extension_name));
      case kTestTypeComponent:
        return LoadExtensionAsComponent(
            test_data_dir_.AppendASCII(extension_name));
    }
    NOTREACHED();
    return NULL;
  }

  const extensions::Extension* extension_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodEngineBrowserTest);
};

class KeyEventDoneCallback {
 public:
  explicit KeyEventDoneCallback(bool expected_argument)
      : expected_argument_(expected_argument) {}
  ~KeyEventDoneCallback() {}

  void Run(bool consumed) {
    if (consumed == expected_argument_)
      run_loop_.Quit();
  }

  void WaitUntilCalled() { run_loop_.Run(); }

 private:
  bool expected_argument_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(KeyEventDoneCallback);
};

class TestTextInputClient : public ui::DummyTextInputClient {
 public:
  explicit TestTextInputClient(ui::TextInputType type)
      : ui::DummyTextInputClient(type) {}
  ~TestTextInputClient() override = default;

  void WaitUntilCalled() { run_loop_.Run(); }

  const base::string16& inserted_text() const { return inserted_text_; }

 private:
  // ui::DummyTextInputClient:
  bool ShouldDoLearning() override { return true; }
  void InsertText(const base::string16& text) override {
    inserted_text_ = text;
    run_loop_.Quit();
  }

  base::string16 inserted_text_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestTextInputClient);
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

IN_PROC_BROWSER_TEST_P(InputMethodEngineBrowserTest,
                       BasicScenarioTest) {
  LoadTestInputMethod();

  InputMethodManager::Get()->GetActiveIMEState()->ChangeInputMethod(
      kIdentityIMEID, false /* show_message */);

  std::unique_ptr<ui::MockIMEInputContextHandler> mock_input_context(
      new ui::MockIMEInputContextHandler());
  std::unique_ptr<MockIMECandidateWindowHandler> mock_candidate_window(
      new MockIMECandidateWindowHandler());

  ui::IMEBridge::Get()->SetInputContextHandler(mock_input_context.get());
  ui::IMEBridge::Get()->SetCandidateWindowHandler(mock_candidate_window.get());

  ui::IMEEngineHandlerInterface* engine_handler =
      ui::IMEBridge::Get()->GetCurrentEngineHandler();
  ASSERT_TRUE(engine_handler);

  // onActivate event should be fired if Enable function is called.
  ExtensionTestMessageListener activated_listener("onActivate", false);
  engine_handler->Enable("IdentityIME");
  ASSERT_TRUE(activated_listener.WaitUntilSatisfied());
  ASSERT_TRUE(activated_listener.was_satisfied());

  // onFocus event should be fired if FocusIn function is called.
  ExtensionTestMessageListener focus_listener(
      "onFocus:text:true:true:true:false", false);
  const auto context =
      CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_TEXT);
  engine_handler->FocusIn(context);
  ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
  ASSERT_TRUE(focus_listener.was_satisfied());

  // onKeyEvent should be fired if ProcessKeyEvent is called.
  KeyEventDoneCallback callback(false);  // EchoBackIME doesn't consume keys.
  ExtensionTestMessageListener keyevent_listener("onKeyEvent", false);
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE);
  ui::IMEEngineHandlerInterface::KeyEventDoneCallback keyevent_callback =
      base::BindOnce(&KeyEventDoneCallback::Run, base::Unretained(&callback));
  engine_handler->ProcessKeyEvent(key_event, std::move(keyevent_callback));
  ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
  ASSERT_TRUE(keyevent_listener.was_satisfied());
  callback.WaitUntilCalled();

  // onSurroundingTextChange should be fired if SetSurroundingText is called.
  ExtensionTestMessageListener surrounding_text_listener(
      "onSurroundingTextChanged", false);
  engine_handler->SetSurroundingText("text",  // Surrounding text.
                                     0,       // focused position.
                                     1,       // anchor position.
                                     0);      // offset position.
  ASSERT_TRUE(surrounding_text_listener.WaitUntilSatisfied());
  ASSERT_TRUE(surrounding_text_listener.was_satisfied());

  // onMenuItemActivated should be fired if PropertyActivate is called.
  ExtensionTestMessageListener property_listener("onMenuItemActivated", false);
  engine_handler->PropertyActivate("property_name");
  ASSERT_TRUE(property_listener.WaitUntilSatisfied());
  ASSERT_TRUE(property_listener.was_satisfied());

  // onReset should be fired if Reset is called.
  ExtensionTestMessageListener reset_listener("onReset", false);
  engine_handler->Reset();
  ASSERT_TRUE(reset_listener.WaitUntilSatisfied());
  ASSERT_TRUE(reset_listener.was_satisfied());

  // onBlur should be fired if FocusOut is called.
  ExtensionTestMessageListener blur_listener("onBlur", false);
  engine_handler->FocusOut();
  ASSERT_TRUE(blur_listener.WaitUntilSatisfied());
  ASSERT_TRUE(blur_listener.was_satisfied());

  // onDeactivated should be fired if Disable is called.
  ExtensionTestMessageListener disabled_listener("onDeactivated", false);
  engine_handler->Disable();
  ASSERT_TRUE(disabled_listener.WaitUntilSatisfied());
  ASSERT_TRUE(disabled_listener.was_satisfied());

  ui::IMEBridge::Get()->SetInputContextHandler(nullptr);
  ui::IMEBridge::Get()->SetCandidateWindowHandler(nullptr);
}

IN_PROC_BROWSER_TEST_P(InputMethodEngineBrowserTest,
                       APIArgumentTest) {
  // TODO(crbug.com/956825): Makes real end to end test without mocking the
  // input context handler. The test should mock the TextInputClient instance
  // hooked up with InputMethodChromeOS, or even using the real TextInputClient
  // if possible.
  LoadTestInputMethod();

  InputMethodManager::Get()->GetActiveIMEState()->ChangeInputMethod(
      kAPIArgumentIMEID, false /* show_message */);

  std::unique_ptr<ui::MockIMEInputContextHandler> mock_input_context(
      new ui::MockIMEInputContextHandler());
  std::unique_ptr<MockIMECandidateWindowHandler> mock_candidate_window(
      new MockIMECandidateWindowHandler());

  ui::IMEBridge::Get()->SetInputContextHandler(mock_input_context.get());
  ui::IMEBridge::Get()->SetCandidateWindowHandler(mock_candidate_window.get());

  ui::IMEEngineHandlerInterface* engine_handler =
      ui::IMEBridge::Get()->GetCurrentEngineHandler();
  ASSERT_TRUE(engine_handler);

  extensions::ExtensionHost* host =
      extensions::ProcessManager::Get(profile())
          ->GetBackgroundHostForExtension(extension_->id());
  ASSERT_TRUE(host);

  engine_handler->Enable("APIArgumentIME");
  const auto context =
      CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_TEXT);
  engine_handler->FocusIn(context);

  {
    SCOPED_TRACE("KeyDown, Ctrl:No, Alt:No, AltGr:No, Shift:No, Caps:No");
    KeyEventDoneCallback callback(false);
    const std::string expected_value =
        "onKeyEvent::true:keydown:a:KeyA:false:false:false:false:false";
    ExtensionTestMessageListener keyevent_listener(expected_value, false);

    ui::KeyEvent key_event(
        ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A, ui::EF_NONE);
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback keyevent_callback =
        base::BindOnce(&KeyEventDoneCallback::Run, base::Unretained(&callback));
    engine_handler->ProcessKeyEvent(key_event, std::move(keyevent_callback));
    ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
    EXPECT_TRUE(keyevent_listener.was_satisfied());
    callback.WaitUntilCalled();
  }
  {
    SCOPED_TRACE("KeyDown, Ctrl:Yes, Alt:No, AltGr:No, Shift:No, Caps:No");
    KeyEventDoneCallback callback(false);
    const std::string expected_value =
        "onKeyEvent::true:keydown:a:KeyA:true:false:false:false:false";
    ExtensionTestMessageListener keyevent_listener(expected_value, false);

    ui::KeyEvent key_event(ui::ET_KEY_PRESSED,
                           ui::VKEY_A,
                           ui::DomCode::US_A,
                           ui::EF_CONTROL_DOWN);
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback keyevent_callback =
        base::BindOnce(&KeyEventDoneCallback::Run, base::Unretained(&callback));
    engine_handler->ProcessKeyEvent(key_event, std::move(keyevent_callback));
    ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
    EXPECT_TRUE(keyevent_listener.was_satisfied());
    callback.WaitUntilCalled();
  }
  {
    SCOPED_TRACE("KeyDown, Ctrl:No, Alt:Yes, AltGr:No, Shift:No, Caps:No");
    KeyEventDoneCallback callback(false);
    const std::string expected_value =
        "onKeyEvent::true:keydown:a:KeyA:false:true:false:false:false";
    ExtensionTestMessageListener keyevent_listener(expected_value, false);

    ui::KeyEvent key_event(ui::ET_KEY_PRESSED,
                           ui::VKEY_A,
                           ui::DomCode::US_A,
                           ui::EF_ALT_DOWN);
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback keyevent_callback =
        base::BindOnce(&KeyEventDoneCallback::Run, base::Unretained(&callback));
    engine_handler->ProcessKeyEvent(key_event, std::move(keyevent_callback));
    ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
    EXPECT_TRUE(keyevent_listener.was_satisfied());
    callback.WaitUntilCalled();
  }
  {
    SCOPED_TRACE("KeyDown, Ctrl:No, Alt:No, AltGr:No, Shift:Yes, Caps:No");
    KeyEventDoneCallback callback(false);
    const std::string expected_value =
        "onKeyEvent::true:keydown:A:KeyA:false:false:false:true:false";
    ExtensionTestMessageListener keyevent_listener(expected_value, false);

    ui::KeyEvent key_event(ui::ET_KEY_PRESSED,
                           ui::VKEY_A,
                           ui::DomCode::US_A,
                           ui::EF_SHIFT_DOWN);
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback keyevent_callback =
        base::BindOnce(&KeyEventDoneCallback::Run, base::Unretained(&callback));
    engine_handler->ProcessKeyEvent(key_event, std::move(keyevent_callback));
    ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
    EXPECT_TRUE(keyevent_listener.was_satisfied());
    callback.WaitUntilCalled();
  }
  {
    SCOPED_TRACE("KeyDown, Ctrl:No, Alt:No, AltGr:No, Shift:No, Caps:Yes");
    KeyEventDoneCallback callback(false);
    const std::string expected_value =
        "onKeyEvent::true:keydown:A:KeyA:false:false:false:false:true";
    ExtensionTestMessageListener keyevent_listener(expected_value, false);

    ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A,
                           ui::EF_CAPS_LOCK_ON);
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback keyevent_callback =
        base::BindOnce(&KeyEventDoneCallback::Run, base::Unretained(&callback));
    engine_handler->ProcessKeyEvent(key_event, std::move(keyevent_callback));
    ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
    EXPECT_TRUE(keyevent_listener.was_satisfied());
    callback.WaitUntilCalled();
  }
  {
    SCOPED_TRACE("KeyDown, Ctrl:Yes, Alt:Yes, AltGr:No, Shift:No, Caps:No");
    KeyEventDoneCallback callback(false);
    const std::string expected_value =
        "onKeyEvent::true:keydown:a:KeyA:true:true:false:false:false";
    ExtensionTestMessageListener keyevent_listener(expected_value, false);

    ui::KeyEvent key_event(ui::ET_KEY_PRESSED,
                           ui::VKEY_A,
                           ui::DomCode::US_A,
                           ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN);
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback keyevent_callback =
        base::BindOnce(&KeyEventDoneCallback::Run, base::Unretained(&callback));
    engine_handler->ProcessKeyEvent(key_event, std::move(keyevent_callback));
    ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
    EXPECT_TRUE(keyevent_listener.was_satisfied());
    callback.WaitUntilCalled();
  }
  {
    SCOPED_TRACE("KeyDown, Ctrl:No, Alt:No, AltGr:No, Shift:Yes, Caps:Yes");
    KeyEventDoneCallback callback(false);
    const std::string expected_value =
        "onKeyEvent::true:keydown:a:KeyA:false:false:false:true:true";
    ExtensionTestMessageListener keyevent_listener(expected_value, false);

    ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A,
                           ui::EF_SHIFT_DOWN | ui::EF_CAPS_LOCK_ON);
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback keyevent_callback =
        base::BindOnce(&KeyEventDoneCallback::Run, base::Unretained(&callback));
    engine_handler->ProcessKeyEvent(key_event, std::move(keyevent_callback));
    ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
    EXPECT_TRUE(keyevent_listener.was_satisfied());
    callback.WaitUntilCalled();
  }
  {
    SCOPED_TRACE("KeyDown, Ctrl:No, Alt:No, AltGr:Yes, Shift:No, Caps:No");
    KeyEventDoneCallback callback(false);
    const std::string expected_value =
        "onKeyEvent::true:keydown:a:KeyA:false:false:true:false:false";
    ExtensionTestMessageListener keyevent_listener(expected_value, false);

    ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A,
                           ui::EF_ALTGR_DOWN);
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback keyevent_callback =
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
    { ui::VKEY_BROWSER_BACK, "BrowserBack", "HistoryBack" },
    { ui::VKEY_BROWSER_FORWARD, "BrowserForward", "HistoryForward" },
    { ui::VKEY_BROWSER_REFRESH, "BrowserRefresh", "BrowserRefresh" },
    { ui::VKEY_MEDIA_LAUNCH_APP2, "ChromeOSFullscreen", "ChromeOSFullscreen" },
    { ui::VKEY_MEDIA_LAUNCH_APP1,
      "ChromeOSSwitchWindow", "ChromeOSSwitchWindow" },
    { ui::VKEY_BRIGHTNESS_DOWN, "BrightnessDown", "BrightnessDown" },
    { ui::VKEY_BRIGHTNESS_UP, "BrightnessUp", "BrightnessUp" },
    { ui::VKEY_VOLUME_MUTE, "VolumeMute", "AudioVolumeMute" },
    { ui::VKEY_VOLUME_DOWN, "VolumeDown", "AudioVolumeDown" },
    { ui::VKEY_VOLUME_UP, "VolumeUp", "AudioVolumeUp" },
    { ui::VKEY_F1, "F1", "HistoryBack" },
    { ui::VKEY_F2, "F2", "HistoryForward" },
    { ui::VKEY_F3, "F3", "BrowserRefresh" },
    { ui::VKEY_F4, "F4", "ChromeOSFullscreen" },
    { ui::VKEY_F5, "F5", "ChromeOSSwitchWindow" },
    { ui::VKEY_F6, "F6", "BrightnessDown" },
    { ui::VKEY_F7, "F7", "BrightnessUp" },
    { ui::VKEY_F8, "F8", "AudioVolumeMute" },
    { ui::VKEY_F9, "F9", "AudioVolumeDown" },
    { ui::VKEY_F10, "F10", "AudioVolumeUp" },
  };

  for (size_t i = 0; i < base::size(kMediaKeyCases); ++i) {
    SCOPED_TRACE(std::string("KeyDown, ") + kMediaKeyCases[i].code);
    KeyEventDoneCallback callback(false);
    const std::string expected_value = base::StringPrintf(
        "onKeyEvent::true:keydown:%s:%s:false:false:false:false:false",
        kMediaKeyCases[i].key, kMediaKeyCases[i].code);
    ExtensionTestMessageListener keyevent_listener(expected_value, false);

    ui::KeyEvent key_event(
        ui::ET_KEY_PRESSED,
        kMediaKeyCases[i].keycode,
        ui::KeycodeConverter::CodeStringToDomCode(kMediaKeyCases[i].code),
        ui::EF_NONE);
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback keyevent_callback =
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

    ASSERT_TRUE(content::ExecuteScript(host->host_contents(),
                                       commit_text_test_script));
    EXPECT_EQ(1, mock_input_context->commit_text_call_count());
    EXPECT_EQ("COMMIT_TEXT", mock_input_context->last_commit_text());
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

    ASSERT_TRUE(content::ExecuteScript(host->host_contents(),
                                       send_key_events_test_script));

    const ui::KeyEvent& key_event = mock_input_context->last_sent_key_event();
    EXPECT_EQ(ui::ET_KEY_PRESSED, key_event.type());
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

    ASSERT_TRUE(content::ExecuteScript(host->host_contents(),
                                       send_key_events_test_script));

    const ui::KeyEvent& key_event = mock_input_context->last_sent_key_event();
    EXPECT_EQ(ui::ET_KEY_RELEASED, key_event.type());
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

    ASSERT_TRUE(content::ExecuteScript(host->host_contents(),
                                       send_key_events_test_script));

    const ui::KeyEvent& key_event = mock_input_context->last_sent_key_event();
    EXPECT_EQ(ui::ET_KEY_RELEASED, key_event.type());
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

    ASSERT_TRUE(content::ExecuteScript(host->host_contents(),
                                       set_composition_test_script));
    EXPECT_EQ(1, mock_input_context->update_preedit_text_call_count());

    EXPECT_EQ(
        4U,
        mock_input_context->last_update_composition_arg().selection.start());
    EXPECT_EQ(
        4U, mock_input_context->last_update_composition_arg().selection.end());
    EXPECT_TRUE(mock_input_context->last_update_composition_arg().is_visible);

    const ui::CompositionText& composition_text =
        mock_input_context->last_update_composition_arg().composition_text;
    EXPECT_EQ(base::UTF8ToUTF16("COMPOSITION_TEXT"), composition_text.text);
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

    ASSERT_TRUE(content::ExecuteScript(host->host_contents(),
                                       commite_text_test_script));
    EXPECT_EQ(1, mock_input_context->update_preedit_text_call_count());
    EXPECT_FALSE(
        mock_input_context->last_update_composition_arg().is_visible);
    const ui::CompositionText& composition_text =
        mock_input_context->last_update_composition_arg().composition_text;
    EXPECT_TRUE(composition_text.text.empty());
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
    ASSERT_TRUE(content::ExecuteScript(
        host->host_contents(),
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
    ASSERT_TRUE(content::ExecuteScript(
        host->host_contents(),
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
    ASSERT_TRUE(content::ExecuteScript(
        host->host_contents(),
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
    ASSERT_TRUE(content::ExecuteScript(
        host->host_contents(),
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
    ASSERT_TRUE(content::ExecuteScript(
        host->host_contents(),
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
    ASSERT_TRUE(content::ExecuteScript(
        host->host_contents(),
        set_candidate_window_properties_test_script));
    EXPECT_EQ(1, mock_candidate_window->update_lookup_table_call_count());

    // aux text visibility is kept as before.
    const ui::CandidateWindow& table =
        mock_candidate_window->last_update_lookup_table_arg().lookup_table;
    EXPECT_TRUE(table.is_auxiliary_text_visible());
    EXPECT_EQ("AUXILIARY_TEXT", table.auxiliary_text());
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
    ASSERT_TRUE(content::ExecuteScript(host->host_contents(),
                                       set_candidates_test_script));

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

    EXPECT_EQ(base::UTF8ToUTF16("CANDIDATE_1"),
              table.candidates().at(0).value);

    EXPECT_EQ(base::UTF8ToUTF16("CANDIDATE_2"),
              table.candidates().at(1).value);
    EXPECT_EQ(base::UTF8ToUTF16("LABEL_2"), table.candidates().at(1).label);

    EXPECT_EQ(base::UTF8ToUTF16("CANDIDATE_3"),
              table.candidates().at(2).value);
    EXPECT_EQ(base::UTF8ToUTF16("LABEL_3"), table.candidates().at(2).label);
    EXPECT_EQ(base::UTF8ToUTF16("ANNOTACTION_3"),
              table.candidates().at(2).annotation);

    EXPECT_EQ(base::UTF8ToUTF16("CANDIDATE_4"),
              table.candidates().at(3).value);
    EXPECT_EQ(base::UTF8ToUTF16("LABEL_4"), table.candidates().at(3).label);
    EXPECT_EQ(base::UTF8ToUTF16("ANNOTACTION_4"),
              table.candidates().at(3).annotation);
    EXPECT_EQ(base::UTF8ToUTF16("TITLE_4"),
              table.candidates().at(3).description_title);
    EXPECT_EQ(base::UTF8ToUTF16("BODY_4"),
              table.candidates().at(3).description_body);
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
    ASSERT_TRUE(content::ExecuteScript(
        host->host_contents(), set_cursor_position_test_script));
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
    ASSERT_TRUE(content::ExecuteScript(
        host->host_contents(), set_menu_item_test_script));

    const ui::ime::InputMethodMenuItemList& props =
        ui::ime::InputMethodMenuManager::GetInstance()->
        GetCurrentInputMethodMenuItemList();
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

    EXPECT_TRUE(props[2].is_selection_item);
    // TODO(nona): Add tests for style: ["toggle" and "separator"]
    // and visible:, when implement them.

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
        "  offset: 5,"
        "  length: 3"
        "});";
    ASSERT_TRUE(content::ExecuteScript(
        host->host_contents(), delete_surrounding_text_test_script));

    EXPECT_EQ(1, mock_input_context->delete_surrounding_text_call_count());
    EXPECT_EQ(5, mock_input_context->last_delete_surrounding_text_arg().offset);
    EXPECT_EQ(3U,
              mock_input_context->last_delete_surrounding_text_arg().length);
  }
  {
    SCOPED_TRACE("onFocus test");
    mock_input_context->Reset();
    mock_candidate_window->Reset();

    {
      ExtensionTestMessageListener focus_listener(
          "onFocus:text:true:true:true:false", false);
      const auto context =
          CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_TEXT);
      engine_handler->FocusIn(context);
      ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
      ASSERT_TRUE(focus_listener.was_satisfied());
    }
    {
      ExtensionTestMessageListener focus_listener(
          "onFocus:search:true:true:true:false", false);
      const auto context =
          CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_SEARCH);
      engine_handler->FocusIn(context);
      ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
      ASSERT_TRUE(focus_listener.was_satisfied());
    }
    {
      ExtensionTestMessageListener focus_listener(
          "onFocus:tel:true:true:true:false", false);
      const auto context =
          CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_TELEPHONE);
      engine_handler->FocusIn(context);
      ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
      ASSERT_TRUE(focus_listener.was_satisfied());
    }
    {
      ExtensionTestMessageListener focus_listener(
          "onFocus:url:true:true:true:false", false);
      const auto context =
          CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_URL);
      engine_handler->FocusIn(context);
      ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
      ASSERT_TRUE(focus_listener.was_satisfied());
    }
    {
      ExtensionTestMessageListener focus_listener(
          "onFocus:email:true:true:true:false", false);
      const auto context =
          CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_EMAIL);
      engine_handler->FocusIn(context);
      ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
      ASSERT_TRUE(focus_listener.was_satisfied());
    }
    {
      ExtensionTestMessageListener focus_listener(
          "onFocus:number:true:true:true:false", false);
      const auto context =
          CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_NUMBER);
      engine_handler->FocusIn(context);
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

    ASSERT_TRUE(content::ExecuteScript(host->host_contents(),
                                       set_composition_test_script));
    EXPECT_EQ(
        2U,
        mock_input_context->last_update_composition_arg().selection.start());
    EXPECT_EQ(
        2U, mock_input_context->last_update_composition_arg().selection.end());
    EXPECT_TRUE(mock_input_context->last_update_composition_arg().is_visible);

    const ui::CompositionText& composition_text =
        mock_input_context->last_update_composition_arg().composition_text;
    EXPECT_EQ(base::UTF8ToUTF16("us"), composition_text.text);
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
    EXPECT_EQ("us", mock_input_context->last_commit_text());

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

  ui::IMEBridge::Get()->SetInputContextHandler(nullptr);
  ui::IMEBridge::Get()->SetCandidateWindowHandler(nullptr);
}

IN_PROC_BROWSER_TEST_P(InputMethodEngineBrowserTest, RestrictedKeyboard) {
  LoadTestInputMethod();

  InputMethodManager::Get()->GetActiveIMEState()->ChangeInputMethod(
      kAPIArgumentIMEID, false /* show_message */);

  std::unique_ptr<ui::MockIMEInputContextHandler> mock_input_context(
      new ui::MockIMEInputContextHandler());
  std::unique_ptr<MockIMECandidateWindowHandler> mock_candidate_window(
      new MockIMECandidateWindowHandler());

  ui::IMEBridge::Get()->SetInputContextHandler(mock_input_context.get());
  ui::IMEBridge::Get()->SetCandidateWindowHandler(mock_candidate_window.get());

  ui::IMEEngineHandlerInterface* engine_handler =
      ui::IMEBridge::Get()->GetCurrentEngineHandler();
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

  engine_handler->Enable("APIArgumentIME");

  {
    SCOPED_TRACE("Text");

    ExtensionTestMessageListener focus_listener(
        "onFocus:text:false:false:false:false", false);
    const auto context =
        CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_TEXT);
    engine_handler->FocusIn(context);
    ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
    ASSERT_TRUE(focus_listener.was_satisfied());
  }
  {
    SCOPED_TRACE("Password");

    ExtensionTestMessageListener focus_listener(
        "onFocus:password:false:false:false:false", false);
    const auto context =
        CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
    engine_handler->FocusIn(context);
    ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
    ASSERT_TRUE(focus_listener.was_satisfied());
  }
  {
    SCOPED_TRACE("URL");

    ExtensionTestMessageListener focus_listener(
        "onFocus:url:false:false:false:false", false);
    const auto context =
        CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_URL);
    engine_handler->FocusIn(context);
    ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
    ASSERT_TRUE(focus_listener.was_satisfied());
  }
  {
    SCOPED_TRACE("Search");

    ExtensionTestMessageListener focus_listener(
        "onFocus:search:false:false:false:false", false);
    const auto context =
        CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_SEARCH);
    engine_handler->FocusIn(context);
    ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
    ASSERT_TRUE(focus_listener.was_satisfied());
  }
  {
    SCOPED_TRACE("Email");

    ExtensionTestMessageListener focus_listener(
        "onFocus:email:false:false:false:false", false);
    const auto context =
        CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_EMAIL);
    engine_handler->FocusIn(context);
    ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
    ASSERT_TRUE(focus_listener.was_satisfied());
  }
  {
    SCOPED_TRACE("Number");

    ExtensionTestMessageListener focus_listener(
        "onFocus:number:false:false:false:false", false);
    const auto context =
        CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_NUMBER);
    engine_handler->FocusIn(context);
    ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
    ASSERT_TRUE(focus_listener.was_satisfied());
  }
  {
    SCOPED_TRACE("Telephone");

    ExtensionTestMessageListener focus_listener(
        "onFocus:tel:false:false:false:false", false);
    const auto context =
        CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_TELEPHONE);
    engine_handler->FocusIn(context);
    ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
    ASSERT_TRUE(focus_listener.was_satisfied());
  }

  ui::IMEBridge::Get()->SetInputContextHandler(nullptr);
  ui::IMEBridge::Get()->SetCandidateWindowHandler(nullptr);
}

IN_PROC_BROWSER_TEST_P(InputMethodEngineBrowserTest, ShouldDoLearning) {
  LoadTestInputMethod();

  InputMethodManager::Get()->GetActiveIMEState()->ChangeInputMethod(
      kIdentityIMEID, false /* show_message */);

  std::unique_ptr<ui::MockIMEInputContextHandler> mock_input_context(
      new ui::MockIMEInputContextHandler());
  std::unique_ptr<MockIMECandidateWindowHandler> mock_candidate_window(
      new MockIMECandidateWindowHandler());

  ui::IMEBridge::Get()->SetInputContextHandler(mock_input_context.get());
  ui::IMEBridge::Get()->SetCandidateWindowHandler(mock_candidate_window.get());

  ui::IMEEngineHandlerInterface* engine_handler =
      ui::IMEBridge::Get()->GetCurrentEngineHandler();
  ASSERT_TRUE(engine_handler);
  engine_handler->Enable("IdentityIME");

  // onFocus event should be fired if FocusIn function is called.
  ExtensionTestMessageListener focus_listener(
      "onFocus:text:true:true:true:true", false);
  auto context = CreateInputContextWithInputType(ui::TEXT_INPUT_TYPE_TEXT);
  context.should_do_learning = true;
  engine_handler->FocusIn(context);
  ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
  ASSERT_TRUE(focus_listener.was_satisfied());

  ui::IMEBridge::Get()->SetInputContextHandler(nullptr);
  ui::IMEBridge::Get()->SetCandidateWindowHandler(nullptr);
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
        "onFocus:text:true:true:true:true", false);

    im->SetFocusedTextInputClient(&tic);

    ASSERT_TRUE(focus_listener.WaitUntilSatisfied());
    ASSERT_TRUE(focus_listener.was_satisfied());
  }

  {
    SCOPED_TRACE("Verifies onKeyEvent event.");
    ExtensionTestMessageListener keydown_listener(
        "onKeyEvent::true:keydown:a:KeyA:false:false:false:false:false", false);

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
        content::ExecuteScript(host->host_contents(), commit_text_test_script));
    tic.WaitUntilCalled();
    EXPECT_EQ(base::UTF8ToUTF16("COMMIT_TEXT"), tic.inserted_text());
  }

  {
    SCOPED_TRACE("Verifies onBlur event");
    ExtensionTestMessageListener blur_listener("onBlur", false);

    ui::DummyTextInputClient dtic;
    im->SetFocusedTextInputClient(&dtic);

    ASSERT_TRUE(blur_listener.WaitUntilSatisfied());
    ASSERT_TRUE(blur_listener.was_satisfied());

    im->DetachTextInputClient(&dtic);
  }
}

}  // namespace
}  // namespace input_method
}  // namespace chromeos
