// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/shell.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/sticky_keys/sticky_keys_overlay.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/events/event_rewriter_delegate_impl.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager_impl.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_member.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/chromeos/fake_ime_keyboard.h"
#include "ui/chromeos/events/event_rewriter_chromeos.h"
#include "ui/chromeos/events/modifier_key.h"
#include "ui/chromeos/events/pref_names.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/test_event_processor.h"
#include "ui/wm/core/window_util.h"

namespace {

constexpr int kKeyboardDeviceId = 123;

std::string GetExpectedResultAsString(ui::EventType ui_type,
                                      ui::KeyboardCode ui_keycode,
                                      ui::DomCode code,
                                      int ui_flags,  // ui::EventFlags
                                      ui::DomKey key) {
  return base::StringPrintf(
      "type=%d code=0x%06X flags=0x%X vk=0x%02X key=0x%08X", ui_type,
      static_cast<unsigned int>(code), ui_flags & ~ui::EF_IS_REPEAT, ui_keycode,
      static_cast<unsigned int>(key));
}

std::string GetKeyEventAsString(const ui::KeyEvent& keyevent) {
  return GetExpectedResultAsString(keyevent.type(), keyevent.key_code(),
                                   keyevent.code(), keyevent.flags(),
                                   keyevent.GetDomKey());
}

std::string GetRewrittenEventAsString(
    const std::unique_ptr<ui::EventRewriterChromeOS>& rewriter,
    ui::EventType ui_type,
    ui::KeyboardCode ui_keycode,
    ui::DomCode code,
    int ui_flags,  // ui::EventFlags
    ui::DomKey key,
    int device_id = kKeyboardDeviceId) {
  ui::KeyEvent event(ui_type, ui_keycode, code, ui_flags, key,
                     ui::EventTimeForNow());
  event.set_source_device_id(device_id);
  std::unique_ptr<ui::Event> new_event;
  rewriter->RewriteEvent(event, &new_event);
  if (new_event)
    return GetKeyEventAsString(*new_event->AsKeyEvent());
  return GetKeyEventAsString(event);
}

// Table entry for simple single key event rewriting tests.
struct KeyTestCase {
  ui::EventType type;
  struct Event {
    ui::KeyboardCode key_code;
    ui::DomCode code;
    int flags;  // ui::EventFlags
    ui::DomKey::Base key;
  } input, expected;
  int device_id = kKeyboardDeviceId;
};

std::string GetTestCaseAsString(ui::EventType ui_type,
                                const KeyTestCase::Event& test) {
  return GetExpectedResultAsString(ui_type, test.key_code, test.code,
                                   test.flags, test.key);
}

// Tests a single stateless key rewrite operation.
void CheckKeyTestCase(
    const std::unique_ptr<ui::EventRewriterChromeOS>& rewriter,
    const KeyTestCase& test) {
  SCOPED_TRACE("\nSource:    " + GetTestCaseAsString(test.type, test.input));
  std::string expected = GetTestCaseAsString(test.type, test.expected);
  EXPECT_EQ(expected,
            GetRewrittenEventAsString(rewriter, test.type, test.input.key_code,
                                      test.input.code, test.input.flags,
                                      test.input.key, test.device_id));
}

}  // namespace

namespace chromeos {

class EventRewriterTest : public ChromeAshTestBase {
 public:
  EventRewriterTest()
      : fake_user_manager_(new chromeos::FakeChromeUserManager),
        user_manager_enabler_(base::WrapUnique(fake_user_manager_)) {}
  ~EventRewriterTest() override {}

  void SetUp() override {
    input_method_manager_mock_ = new input_method::MockInputMethodManagerImpl;
    chromeos::input_method::InitializeForTesting(
        input_method_manager_mock_);  // pass ownership
    delegate_ = std::make_unique<EventRewriterDelegateImpl>(nullptr);
    delegate_->set_pref_service_for_testing(prefs());
    rewriter_ =
        std::make_unique<ui::EventRewriterChromeOS>(delegate_.get(), nullptr);
    ChromeAshTestBase::SetUp();
  }

  void TearDown() override {
    ChromeAshTestBase::TearDown();
    // Shutdown() deletes the IME mock object.
    chromeos::input_method::Shutdown();
  }

 protected:
  void TestRewriteNumPadKeys();
  void TestRewriteNumPadKeysOnAppleKeyboard();

  // Parameterized version of test depending on feature flag values. The feature
  // kUseSearchClickForRightClick determines if this should test for alt-click
  // or search-click.
  void DontRewriteIfNotRewritten(int right_click_flags);

  const ui::MouseEvent* RewriteMouseButtonEvent(
      const ui::MouseEvent& event,
      std::unique_ptr<ui::Event>* new_event) {
    rewriter_->RewriteMouseButtonEventForTesting(event, new_event);
    return *new_event ? new_event->get()->AsMouseEvent() : &event;
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

  void InitModifierKeyPref(IntegerPrefMember* int_pref,
                           const std::string& pref_name,
                           ui::chromeos::ModifierKey modifierKey) {
    if (int_pref->GetPrefName() != pref_name)  // skip if already initialized.
      int_pref->Init(pref_name, prefs());
    int_pref->SetValue(static_cast<int>(modifierKey));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  FakeChromeUserManager* fake_user_manager_;  // Not owned.
  user_manager::ScopedUserManager user_manager_enabler_;
  input_method::MockInputMethodManagerImpl* input_method_manager_mock_;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<EventRewriterDelegateImpl> delegate_;
  std::unique_ptr<ui::EventRewriterChromeOS> rewriter_;
};

TEST_F(EventRewriterTest, TestRewriteCommandToControl) {
  // First, test with a PC keyboard.
  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter_->set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);

  KeyTestCase pc_keyboard_tests[] = {
      // VKEY_A, Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN,
        ui::DomKey::UNIDENTIFIED}},

      // VKEY_A, Win modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED}},

      // VKEY_A, Alt+Win modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED}},

      // VKEY_LWIN (left Windows key), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, ui::DomKey::META},
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, ui::DomKey::META}},

      // VKEY_RWIN (right Windows key), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_RWIN, ui::DomCode::META_RIGHT,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, ui::DomKey::META},
       {ui::VKEY_RWIN, ui::DomCode::META_RIGHT,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, ui::DomKey::META}},
  };

  for (const auto& test : pc_keyboard_tests)
    CheckKeyTestCase(rewriter_, test);

  // An Apple keyboard reusing the ID, zero.
  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "Apple Keyboard");
  rewriter_->set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);

  // Simulate the default initialization of the Apple Command key remap pref to
  // Ctrl.
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());

  KeyTestCase apple_keyboard_tests[] = {
      // VKEY_A, Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN,
        ui::DomKey::UNIDENTIFIED}},

      // VKEY_A, Win modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'a'>::Character}},

      // VKEY_A, Alt+Win modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'a'>::Character}},

      // VKEY_LWIN (left Windows key), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, ui::DomKey::CONTROL}},

      // VKEY_RWIN (right Windows key), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_RWIN, ui::DomCode::META_RIGHT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_RIGHT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, ui::DomKey::CONTROL}},
  };

  for (const auto& test : apple_keyboard_tests)
    CheckKeyTestCase(rewriter_, test);

  // Now simulate the user remapped the Command key back to Search.
  IntegerPrefMember command;
  InitModifierKeyPref(&command, prefs::kLanguageRemapExternalCommandKeyTo,
                      ui::chromeos::ModifierKey::kSearchKey);

  KeyTestCase command_remapped_to_search_tests[] = {
      // VKEY_A, Win modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED}},

      // VKEY_A, Alt+Win modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED}},

      // VKEY_LWIN (left Windows key), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META}},

      // VKEY_RWIN (right Windows key), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_RWIN, ui::DomCode::META_RIGHT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       {ui::VKEY_LWIN, ui::DomCode::META_RIGHT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META}},
  };

  for (const auto& test : command_remapped_to_search_tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestRewriteExternalMetaKey) {
  // Simulate the default initialization of the Meta key on external keyboards
  // remap pref to Search.
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());

  // Add an internal and external keyboards.
  rewriter_->KeyboardDeviceAddedForTesting(
      kKeyboardDeviceId, "Internal Keyboard",
      ui::EventRewriterChromeOS::kKbdTopRowLayoutDefault,
      ui::INPUT_DEVICE_INTERNAL);
  rewriter_->KeyboardDeviceAddedForTesting(
      kKeyboardDeviceId + 1, "External Keyboard",
      ui::EventRewriterChromeOS::kKbdTopRowLayoutDefault, ui::INPUT_DEVICE_USB);

  // The Meta key on both external and internal keyboards should produce Search.

  // Test internal keyboard.
  rewriter_->set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);
  KeyTestCase default_internal_search_tests[] = {
      // VKEY_A, Win modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       kKeyboardDeviceId},

      // VKEY_A, Alt+Win modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       kKeyboardDeviceId},

      // VKEY_LWIN (left Windows key), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       kKeyboardDeviceId},

      // VKEY_RWIN (right Windows key), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_RWIN, ui::DomCode::META_RIGHT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       {ui::VKEY_LWIN, ui::DomCode::META_RIGHT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       kKeyboardDeviceId},
  };
  for (const auto& test : default_internal_search_tests)
    CheckKeyTestCase(rewriter_, test);

  // Test external Keyboard.
  KeyTestCase default_external_meta_tests[] = {
      // VKEY_A, Win modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       kKeyboardDeviceId + 1},

      // VKEY_A, Alt+Win modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       kKeyboardDeviceId + 1},

      // VKEY_LWIN (left Windows key), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       kKeyboardDeviceId + 1},

      // VKEY_RWIN (right Windows key), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_RWIN, ui::DomCode::META_RIGHT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       {ui::VKEY_LWIN, ui::DomCode::META_RIGHT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       kKeyboardDeviceId + 1},
  };
  rewriter_->set_last_keyboard_device_id_for_testing(kKeyboardDeviceId + 1);
  for (const auto& test : default_external_meta_tests)
    CheckKeyTestCase(rewriter_, test);

  // Both preferences for internal Search and external Meta are independent,
  // even if one or both are modified.

  // Remap internal Search to Ctrl.
  IntegerPrefMember internal_search;
  InitModifierKeyPref(&internal_search, prefs::kLanguageRemapSearchKeyTo,
                      ui::chromeos::ModifierKey::kControlKey);

  // Remap external Search to Alt.
  IntegerPrefMember meta;
  InitModifierKeyPref(&meta, prefs::kLanguageRemapExternalMetaKeyTo,
                      ui::chromeos::ModifierKey::kAltKey);

  // Test internal keyboard.
  KeyTestCase remapped_internal_search_tests[] = {
      // VKEY_A, Win modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'a'>::Character},
       kKeyboardDeviceId},

      // VKEY_A, Alt+Win modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'a'>::Character},
       kKeyboardDeviceId},

      // VKEY_LWIN (left Windows key), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, ui::DomKey::CONTROL},
       kKeyboardDeviceId},

      // VKEY_RWIN (right Windows key), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_RWIN, ui::DomCode::META_RIGHT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_RIGHT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, ui::DomKey::CONTROL},
       kKeyboardDeviceId},
  };
  rewriter_->set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);
  for (const auto& test : remapped_internal_search_tests)
    CheckKeyTestCase(rewriter_, test);

  // Test external keyboard.
  KeyTestCase remapped_external_search_tests[] = {
      // VKEY_A, Win modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'a'>::Character},
       kKeyboardDeviceId + 1},

      // VKEY_A, Alt+Win modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'a'>::Character},
       kKeyboardDeviceId + 1},

      // VKEY_LWIN (left Windows key), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN, ui::DomKey::ALT},
       kKeyboardDeviceId + 1},

      // VKEY_RWIN (right Windows key), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_RWIN, ui::DomCode::META_RIGHT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       {ui::VKEY_MENU, ui::DomCode::ALT_RIGHT, ui::EF_ALT_DOWN,
        ui::DomKey::ALT},
       kKeyboardDeviceId + 1},
  };
  rewriter_->set_last_keyboard_device_id_for_testing(kKeyboardDeviceId + 1);
  for (const auto& test : remapped_external_search_tests)
    CheckKeyTestCase(rewriter_, test);
}

// For crbug.com/133896.
TEST_F(EventRewriterTest, TestRewriteCommandToControlWithControlRemapped) {
  // Remap Control to Alt.
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember control;
  InitModifierKeyPref(&control, prefs::kLanguageRemapControlKeyTo,
                      ui::chromeos::ModifierKey::kAltKey);

  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter_->set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);

  KeyTestCase pc_keyboard_tests[] = {
      // Control should be remapped to Alt.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL},
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN,
        ui::DomKey::ALT}},
  };

  for (const auto& test : pc_keyboard_tests)
    CheckKeyTestCase(rewriter_, test);

  // An Apple keyboard reusing the ID, zero.
  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "Apple Keyboard");
  rewriter_->set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);

  KeyTestCase apple_keyboard_tests[] = {
      // VKEY_LWIN (left Command key) with  Alt modifier. The remapped Command
      // key should never be re-remapped to Alt.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, ui::DomKey::CONTROL}},

      // VKEY_RWIN (right Command key) with  Alt modifier. The remapped Command
      // key should never be re-remapped to Alt.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_RWIN, ui::DomCode::META_RIGHT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_RIGHT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, ui::DomKey::CONTROL}},
  };

  for (const auto& test : apple_keyboard_tests)
    CheckKeyTestCase(rewriter_, test);
}

void EventRewriterTest::TestRewriteNumPadKeys() {
  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter_->set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);

  KeyTestCase tests[] = {
      // XK_KP_Insert (= NumPad 0 without Num Lock), no modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_INSERT, ui::DomCode::NUMPAD0, ui::EF_NONE, ui::DomKey::INSERT},
       {ui::VKEY_NUMPAD0, ui::DomCode::NUMPAD0, ui::EF_NONE,
        ui::DomKey::Constant<'0'>::Character}},

      // XK_KP_Insert (= NumPad 0 without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_INSERT, ui::DomCode::NUMPAD0, ui::EF_ALT_DOWN,
        ui::DomKey::INSERT},
       {ui::VKEY_NUMPAD0, ui::DomCode::NUMPAD0, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'0'>::Character}},

      // XK_KP_Delete (= NumPad . without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_DELETE, ui::DomCode::NUMPAD_DECIMAL, ui::EF_ALT_DOWN,
        ui::DomKey::DEL},
       {ui::VKEY_DECIMAL, ui::DomCode::NUMPAD_DECIMAL, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'.'>::Character}},

      // XK_KP_End (= NumPad 1 without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_END, ui::DomCode::NUMPAD1, ui::EF_ALT_DOWN, ui::DomKey::END},
       {ui::VKEY_NUMPAD1, ui::DomCode::NUMPAD1, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'1'>::Character}},

      // XK_KP_Down (= NumPad 2 without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::DomCode::NUMPAD2, ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_DOWN},
       {ui::VKEY_NUMPAD2, ui::DomCode::NUMPAD2, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'2'>::Character}},

      // XK_KP_Next (= NumPad 3 without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NEXT, ui::DomCode::NUMPAD3, ui::EF_ALT_DOWN,
        ui::DomKey::PAGE_DOWN},
       {ui::VKEY_NUMPAD3, ui::DomCode::NUMPAD3, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'3'>::Character}},

      // XK_KP_Left (= NumPad 4 without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LEFT, ui::DomCode::NUMPAD4, ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_LEFT},
       {ui::VKEY_NUMPAD4, ui::DomCode::NUMPAD4, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'4'>::Character}},

      // XK_KP_Begin (= NumPad 5 without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_CLEAR, ui::DomCode::NUMPAD5, ui::EF_ALT_DOWN,
        ui::DomKey::CLEAR},
       {ui::VKEY_NUMPAD5, ui::DomCode::NUMPAD5, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'5'>::Character}},

      // XK_KP_Right (= NumPad 6 without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_RIGHT, ui::DomCode::NUMPAD6, ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_RIGHT},
       {ui::VKEY_NUMPAD6, ui::DomCode::NUMPAD6, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'6'>::Character}},

      // XK_KP_Home (= NumPad 7 without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_HOME, ui::DomCode::NUMPAD7, ui::EF_ALT_DOWN, ui::DomKey::HOME},
       {ui::VKEY_NUMPAD7, ui::DomCode::NUMPAD7, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'7'>::Character}},

      // XK_KP_Up (= NumPad 8 without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_UP, ui::DomCode::NUMPAD8, ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_UP},
       {ui::VKEY_NUMPAD8, ui::DomCode::NUMPAD8, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'8'>::Character}},

      // XK_KP_Prior (= NumPad 9 without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_PRIOR, ui::DomCode::NUMPAD9, ui::EF_ALT_DOWN,
        ui::DomKey::PAGE_UP},
       {ui::VKEY_NUMPAD9, ui::DomCode::NUMPAD9, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'9'>::Character}},

      // XK_KP_0 (= NumPad 0 with Num Lock), Num Lock modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD0, ui::DomCode::NUMPAD0, ui::EF_NONE,
        ui::DomKey::Constant<'0'>::Character},
       {ui::VKEY_NUMPAD0, ui::DomCode::NUMPAD0, ui::EF_NONE,
        ui::DomKey::Constant<'0'>::Character}},

      // XK_KP_DECIMAL (= NumPad . with Num Lock), Num Lock modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_DECIMAL, ui::DomCode::NUMPAD_DECIMAL, ui::EF_NONE,
        ui::DomKey::Constant<'.'>::Character},
       {ui::VKEY_DECIMAL, ui::DomCode::NUMPAD_DECIMAL, ui::EF_NONE,
        ui::DomKey::Constant<'.'>::Character}},

      // XK_KP_1 (= NumPad 1 with Num Lock), Num Lock modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD1, ui::DomCode::NUMPAD1, ui::EF_NONE,
        ui::DomKey::Constant<'1'>::Character},
       {ui::VKEY_NUMPAD1, ui::DomCode::NUMPAD1, ui::EF_NONE,
        ui::DomKey::Constant<'1'>::Character}},

      // XK_KP_2 (= NumPad 2 with Num Lock), Num Lock modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD2, ui::DomCode::NUMPAD2, ui::EF_NONE,
        ui::DomKey::Constant<'2'>::Character},
       {ui::VKEY_NUMPAD2, ui::DomCode::NUMPAD2, ui::EF_NONE,
        ui::DomKey::Constant<'2'>::Character}},

      // XK_KP_3 (= NumPad 3 with Num Lock), Num Lock modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD3, ui::DomCode::NUMPAD3, ui::EF_NONE,
        ui::DomKey::Constant<'3'>::Character},
       {ui::VKEY_NUMPAD3, ui::DomCode::NUMPAD3, ui::EF_NONE,
        ui::DomKey::Constant<'3'>::Character}},

      // XK_KP_4 (= NumPad 4 with Num Lock), Num Lock modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD4, ui::DomCode::NUMPAD4, ui::EF_NONE,
        ui::DomKey::Constant<'4'>::Character},
       {ui::VKEY_NUMPAD4, ui::DomCode::NUMPAD4, ui::EF_NONE,
        ui::DomKey::Constant<'4'>::Character}},

      // XK_KP_5 (= NumPad 5 with Num Lock), Num Lock
      // modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD5, ui::DomCode::NUMPAD5, ui::EF_NONE,
        ui::DomKey::Constant<'5'>::Character},
       {ui::VKEY_NUMPAD5, ui::DomCode::NUMPAD5, ui::EF_NONE,
        ui::DomKey::Constant<'5'>::Character}},

      // XK_KP_6 (= NumPad 6 with Num Lock), Num Lock
      // modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD6, ui::DomCode::NUMPAD6, ui::EF_NONE,
        ui::DomKey::Constant<'6'>::Character},
       {ui::VKEY_NUMPAD6, ui::DomCode::NUMPAD6, ui::EF_NONE,
        ui::DomKey::Constant<'6'>::Character}},

      // XK_KP_7 (= NumPad 7 with Num Lock), Num Lock
      // modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD7, ui::DomCode::NUMPAD7, ui::EF_NONE,
        ui::DomKey::Constant<'7'>::Character},
       {ui::VKEY_NUMPAD7, ui::DomCode::NUMPAD7, ui::EF_NONE,
        ui::DomKey::Constant<'7'>::Character}},

      // XK_KP_8 (= NumPad 8 with Num Lock), Num Lock
      // modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD8, ui::DomCode::NUMPAD8, ui::EF_NONE,
        ui::DomKey::Constant<'8'>::Character},
       {ui::VKEY_NUMPAD8, ui::DomCode::NUMPAD8, ui::EF_NONE,
        ui::DomKey::Constant<'8'>::Character}},

      // XK_KP_9 (= NumPad 9 with Num Lock), Num Lock
      // modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD9, ui::DomCode::NUMPAD9, ui::EF_NONE,
        ui::DomKey::Constant<'9'>::Character},
       {ui::VKEY_NUMPAD9, ui::DomCode::NUMPAD9, ui::EF_NONE,
        ui::DomKey::Constant<'9'>::Character}},
  };

  for (const auto& test : tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestRewriteNumPadKeys) {
  TestRewriteNumPadKeys();
}

// Tests if the rewriter can handle a Command + Num Pad event.
void EventRewriterTest::TestRewriteNumPadKeysOnAppleKeyboard() {
  // Simulate the default initialization of the Apple Command key remap pref to
  // Ctrl.
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());

  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "Apple Keyboard");
  rewriter_->set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);

  KeyTestCase tests[] = {
      // XK_KP_End (= NumPad 1 without Num Lock), Win modifier.
      // The result should be "Num Pad 1 with Control + Num Lock modifiers".
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_END, ui::DomCode::NUMPAD1, ui::EF_COMMAND_DOWN,
        ui::DomKey::END},
       {ui::VKEY_NUMPAD1, ui::DomCode::NUMPAD1, ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'1'>::Character}},

      // XK_KP_1 (= NumPad 1 with Num Lock), Win modifier.
      // The result should also be "Num Pad 1 with Control + Num Lock
      // modifiers".
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD1, ui::DomCode::NUMPAD1, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'1'>::Character},
       {ui::VKEY_NUMPAD1, ui::DomCode::NUMPAD1, ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'1'>::Character}}};

  for (const auto& test : tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestRewriteNumPadKeysOnAppleKeyboard) {
  TestRewriteNumPadKeysOnAppleKeyboard();
}

TEST_F(EventRewriterTest, TestRewriteModifiersNoRemap) {
  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");

  KeyTestCase tests[] = {
      // Press Search. Confirm the event is not rewritten.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_NONE, ui::DomKey::META},
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_COMMAND_DOWN,
        ui::DomKey::META}},

      // Press left Control. Confirm the event is not rewritten.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL}},

      // Press right Control. Confirm the event is not rewritten.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL}},

      // Press left Alt. Confirm the event is not rewritten.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN, ui::DomKey::ALT},
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN,
        ui::DomKey::ALT}},

      // Press right Alt. Confirm the event is not rewritten.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN, ui::DomKey::ALT},
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN,
        ui::DomKey::ALT}},

      // Test KeyRelease event, just in case.
      // Release Search. Confirm the release event is not rewritten.
      {ui::ET_KEY_RELEASED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_NONE, ui::DomKey::META},
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_NONE, ui::DomKey::META}},
  };

  for (const auto& test : tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestRewriteModifiersNoRemapMultipleKeys) {
  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");

  KeyTestCase tests[] = {
      // Press Alt with Shift. Confirm the event is not rewritten.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, ui::DomKey::ALT},
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, ui::DomKey::ALT}},

      // Press Escape with Alt and Shift. Confirm the event is not rewritten.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_ESCAPE, ui::DomCode::ESCAPE,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, ui::DomKey::ESCAPE},
       {ui::VKEY_ESCAPE, ui::DomCode::ESCAPE,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, ui::DomKey::ESCAPE}},

      // Press Search with Caps Lock mask. Confirm the event is not rewritten.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_CAPS_LOCK_ON | ui::EF_COMMAND_DOWN, ui::DomKey::META},
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_CAPS_LOCK_ON | ui::EF_COMMAND_DOWN, ui::DomKey::META}},

      // Release Search with Caps Lock mask. Confirm the event is not rewritten.
      {ui::ET_KEY_RELEASED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_CAPS_LOCK_ON,
        ui::DomKey::META},
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_CAPS_LOCK_ON,
        ui::DomKey::META}},

      // Press Shift+Ctrl+Alt+Search+Escape. Confirm the event is not rewritten.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_ESCAPE, ui::DomCode::ESCAPE,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::ESCAPE},
       {ui::VKEY_ESCAPE, ui::DomCode::ESCAPE,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::ESCAPE}},

      // Press Shift+Ctrl+Alt+Search+B. Confirm the event is not rewritten.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_B, ui::DomCode::US_B,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'B'>::Character},
       {ui::VKEY_B, ui::DomCode::US_B,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'B'>::Character}},
  };

  for (const auto& test : tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestRewriteModifiersDisableSome) {
  // Disable Search, Control and Escape keys.
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember search;
  InitModifierKeyPref(&search, prefs::kLanguageRemapSearchKeyTo,
                      ui::chromeos::ModifierKey::kVoidKey);
  IntegerPrefMember control;
  InitModifierKeyPref(&control, prefs::kLanguageRemapControlKeyTo,
                      ui::chromeos::ModifierKey::kVoidKey);
  IntegerPrefMember escape;
  InitModifierKeyPref(&escape, prefs::kLanguageRemapEscapeKeyTo,
                      ui::chromeos::ModifierKey::kVoidKey);

  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");

  KeyTestCase disabled_modifier_tests[] = {
      // Press Alt with Shift. This key press shouldn't be affected by the
      // pref. Confirm the event is not rewritten.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, ui::DomKey::ALT},
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, ui::DomKey::ALT}},

      // Press Search. Confirm the event is now VKEY_UNKNOWN.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_NONE, ui::DomKey::META},
       {ui::VKEY_UNKNOWN, ui::DomCode::NONE, ui::EF_NONE,
        ui::DomKey::UNIDENTIFIED}},

      // Press Control. Confirm the event is now VKEY_UNKNOWN.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL},
       {ui::VKEY_UNKNOWN, ui::DomCode::NONE, ui::EF_NONE,
        ui::DomKey::UNIDENTIFIED}},

      // Press Escape. Confirm the event is now VKEY_UNKNOWN.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_ESCAPE, ui::DomCode::ESCAPE, ui::EF_NONE, ui::DomKey::ESCAPE},
       {ui::VKEY_UNKNOWN, ui::DomCode::NONE, ui::EF_NONE,
        ui::DomKey::UNIDENTIFIED}},

      // Press Control+Search. Confirm the event is now VKEY_UNKNOWN
      // without any modifiers.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::META},
       {ui::VKEY_UNKNOWN, ui::DomCode::NONE, ui::EF_NONE,
        ui::DomKey::UNIDENTIFIED}},

      // Press Control+Search+a. Confirm the event is now VKEY_A without any
      // modifiers.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'a'>::Character},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_NONE,
        ui::DomKey::Constant<'a'>::Character}},

      // Press Control+Search+Alt+a. Confirm the event is now VKEY_A only with
      // the Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'a'>::Character},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'a'>::Character}},
  };

  for (const auto& test : disabled_modifier_tests)
    CheckKeyTestCase(rewriter_, test);

  // Remap Alt to Control.
  IntegerPrefMember alt;
  InitModifierKeyPref(&alt, prefs::kLanguageRemapAltKeyTo,
                      ui::chromeos::ModifierKey::kControlKey);

  KeyTestCase tests[] = {
      // Press left Alt. Confirm the event is now VKEY_CONTROL
      // even though the Control key itself is disabled.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN, ui::DomKey::ALT},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL}},

      // Press Alt+a. Confirm the event is now Control+a even though the Control
      // key itself is disabled.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'a'>::Character},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'a'>::Character}},
  };

  for (const auto& test : tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapToControl) {
  // Remap Search to Control.
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember search;
  InitModifierKeyPref(&search, prefs::kLanguageRemapSearchKeyTo,
                      ui::chromeos::ModifierKey::kControlKey);

  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");

  KeyTestCase s_tests[] = {
      // Press Search. Confirm the event is now VKEY_CONTROL.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_COMMAND_DOWN,
        ui::DomKey::META},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL}},
  };

  for (const auto& test : s_tests)
    CheckKeyTestCase(rewriter_, test);

  // Remap Alt to Control too.
  IntegerPrefMember alt;
  InitModifierKeyPref(&alt, prefs::kLanguageRemapAltKeyTo,
                      ui::chromeos::ModifierKey::kControlKey);

  KeyTestCase sa_tests[] = {
      // Press Alt. Confirm the event is now VKEY_CONTROL.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN, ui::DomKey::ALT},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL}},

      // Press Alt+Search. Confirm the event is now VKEY_CONTROL.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, ui::DomKey::META},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL}},

      // Press Control+Alt+Search. Confirm the event is now VKEY_CONTROL.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::META},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL}},

      // Press Shift+Control+Alt+Search. Confirm the event is now Control with
      // Shift and Control modifiers.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::META},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, ui::DomKey::CONTROL}},

      // Press Shift+Control+Alt+Search+B. Confirm the event is now B with Shift
      // and Control modifiers.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_B, ui::DomCode::US_B,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'B'>::Character},
       {ui::VKEY_B, ui::DomCode::US_B, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'B'>::Character}},
  };

  for (const auto& test : sa_tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapToEscape) {
  // Remap Search to Escape.
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember search;
  InitModifierKeyPref(&search, prefs::kLanguageRemapSearchKeyTo,
                      ui::chromeos::ModifierKey::kEscapeKey);

  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");

  KeyTestCase tests[] = {
      // Press Search. Confirm the event is now VKEY_ESCAPE.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_COMMAND_DOWN,
        ui::DomKey::META},
       {ui::VKEY_ESCAPE, ui::DomCode::ESCAPE, ui::EF_NONE, ui::DomKey::ESCAPE}},
  };

  for (const auto& test : tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapEscapeToAlt) {
  // Remap Escape to Alt.
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember escape;
  InitModifierKeyPref(&escape, prefs::kLanguageRemapEscapeKeyTo,
                      ui::chromeos::ModifierKey::kAltKey);
  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");

  KeyTestCase e2a_tests[] = {
      // Press Escape. Confirm the event is now VKEY_MENU.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_ESCAPE, ui::DomCode::ESCAPE, ui::EF_NONE, ui::DomKey::ESCAPE},
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN,
        ui::DomKey::ALT}},
      // Release Escape to clear flags.
      {ui::ET_KEY_RELEASED,
       {ui::VKEY_ESCAPE, ui::DomCode::ESCAPE, ui::EF_NONE, ui::DomKey::ESCAPE},
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_NONE, ui::DomKey::ALT}},
  };

  for (const auto& test : e2a_tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapAltToControl) {
  // Remap Alt to Control.
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember alt;
  InitModifierKeyPref(&alt, prefs::kLanguageRemapAltKeyTo,
                      ui::chromeos::ModifierKey::kControlKey);
  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");

  std::vector<KeyTestCase> a2c_tests = {
      // Press left Alt. Confirm the event is now VKEY_CONTROL.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN, ui::DomKey::ALT},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL}},
      // Press Shift+comma. Verify that only the flags are changed.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_COMMA, ui::DomCode::COMMA,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_OEM_COMMA, ui::DomCode::COMMA,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'<'>::Character}},
      // Press Shift+9. Verify that only the flags are changed.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_9, ui::DomCode::DIGIT9, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_9, ui::DomCode::DIGIT9,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'('>::Character}},
  };

  for (const auto& test : a2c_tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapUnderEscapeControlAlt) {
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());

  // Remap Escape to Alt.
  IntegerPrefMember escape;
  InitModifierKeyPref(&escape, prefs::kLanguageRemapEscapeKeyTo,
                      ui::chromeos::ModifierKey::kAltKey);

  // Remap Alt to Control.
  IntegerPrefMember alt;
  InitModifierKeyPref(&alt, prefs::kLanguageRemapAltKeyTo,
                      ui::chromeos::ModifierKey::kControlKey);

  // Remap Control to Search.
  IntegerPrefMember control;
  InitModifierKeyPref(&control, prefs::kLanguageRemapControlKeyTo,
                      ui::chromeos::ModifierKey::kSearchKey);

  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");

  std::vector<KeyTestCase> c2s_tests = {
      // Press left Control. Confirm the event is now VKEY_LWIN.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL},
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_COMMAND_DOWN,
        ui::DomKey::META}},

      // Then, press all of the three, Control+Alt+Escape.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_ESCAPE, ui::DomCode::ESCAPE,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, ui::DomKey::CONTROL},
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::ALT}},

      // Press Shift+Control+Alt+Escape.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_ESCAPE, ui::DomCode::ESCAPE,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::ESCAPE},
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::ALT}},

      // Press Shift+Control+Alt+B
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_B, ui::DomCode::US_B,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'B'>::Character},
       {ui::VKEY_B, ui::DomCode::US_B,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'B'>::Character}},
  };

  for (const auto& test : c2s_tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest,
       TestRewriteModifiersRemapUnderEscapeControlAltSearch) {
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());

  // Remap Escape to Alt.
  IntegerPrefMember escape;
  InitModifierKeyPref(&escape, prefs::kLanguageRemapEscapeKeyTo,
                      ui::chromeos::ModifierKey::kAltKey);

  // Remap Alt to Control.
  IntegerPrefMember alt;
  InitModifierKeyPref(&alt, prefs::kLanguageRemapAltKeyTo,
                      ui::chromeos::ModifierKey::kControlKey);

  // Remap Control to Search.
  IntegerPrefMember control;
  InitModifierKeyPref(&control, prefs::kLanguageRemapControlKeyTo,
                      ui::chromeos::ModifierKey::kSearchKey);

  // Remap Search to Backspace.
  IntegerPrefMember search;
  InitModifierKeyPref(&search, prefs::kLanguageRemapSearchKeyTo,
                      ui::chromeos::ModifierKey::kBackspaceKey);

  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");

  std::vector<KeyTestCase> s2b_tests = {
      // Release Control and Escape, as Search and Alt would transform Backspace
      // to Delete.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_NONE,
        ui::DomKey::CONTROL},
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_COMMAND_DOWN,
        ui::DomKey::META}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_ESCAPE, ui::DomCode::ESCAPE, ui::EF_NONE, ui::DomKey::ESCAPE},
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN,
        ui::DomKey::ALT}},
      {ui::ET_KEY_RELEASED,
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_NONE,
        ui::DomKey::CONTROL},
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_ALT_DOWN,
        ui::DomKey::META}},
      {ui::ET_KEY_RELEASED,
       {ui::VKEY_ESCAPE, ui::DomCode::ESCAPE, ui::EF_NONE, ui::DomKey::ESCAPE},
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_NONE, ui::DomKey::ALT}},
      // Press Search. Confirm the event is now VKEY_BACK.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_COMMAND_DOWN,
        ui::DomKey::META},
       {ui::VKEY_BACK, ui::DomCode::BACKSPACE, ui::EF_NONE,
        ui::DomKey::BACKSPACE}},
  };

  for (const auto& test : s2b_tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapBackspaceToEscape) {
  // Remap Backspace to Escape.
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember backspace;
  InitModifierKeyPref(&backspace, prefs::kLanguageRemapBackspaceKeyTo,
                      ui::chromeos::ModifierKey::kEscapeKey);

  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");

  std::vector<KeyTestCase> b2e_tests = {
      // Press Backspace. Confirm the event is now VKEY_ESCAPE.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK, ui::DomCode::BACKSPACE, ui::EF_NONE,
        ui::DomKey::BACKSPACE},
       {ui::VKEY_ESCAPE, ui::DomCode::ESCAPE, ui::EF_NONE, ui::DomKey::ESCAPE}},
  };

  for (const auto& test : b2e_tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapToCapsLock) {
  // Remap Search to Caps Lock.
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember search;
  InitModifierKeyPref(&search, prefs::kLanguageRemapSearchKeyTo,
                      ui::chromeos::ModifierKey::kCapsLockKey);

  chromeos::input_method::FakeImeKeyboard ime_keyboard;
  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter_->set_ime_keyboard_for_testing(&ime_keyboard);
  EXPECT_FALSE(ime_keyboard.caps_lock_is_enabled_);

  // Press Search.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::ET_KEY_PRESSED, ui::VKEY_CAPITAL, ui::DomCode::CAPS_LOCK,
                ui::EF_MOD3_DOWN | ui::EF_CAPS_LOCK_ON, ui::DomKey::CAPS_LOCK),
            GetRewrittenEventAsString(rewriter_, ui::ET_KEY_PRESSED,
                                      ui::VKEY_LWIN, ui::DomCode::META_LEFT,
                                      ui::EF_COMMAND_DOWN, ui::DomKey::META));
  EXPECT_FALSE(ime_keyboard.caps_lock_is_enabled_);

  // Release Search.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_RELEASED, ui::VKEY_CAPITAL,
                                      ui::DomCode::CAPS_LOCK, ui::EF_NONE,
                                      ui::DomKey::CAPS_LOCK),
            GetRewrittenEventAsString(rewriter_, ui::ET_KEY_RELEASED,
                                      ui::VKEY_LWIN, ui::DomCode::META_LEFT,
                                      ui::EF_NONE, ui::DomKey::META));
  EXPECT_TRUE(ime_keyboard.caps_lock_is_enabled_);

  // Press Search.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::ET_KEY_PRESSED, ui::VKEY_CAPITAL, ui::DomCode::CAPS_LOCK,
                ui::EF_CAPS_LOCK_ON | ui::EF_MOD3_DOWN, ui::DomKey::CAPS_LOCK),
            GetRewrittenEventAsString(rewriter_, ui::ET_KEY_PRESSED,
                                      ui::VKEY_LWIN, ui::DomCode::META_LEFT,
                                      ui::EF_COMMAND_DOWN | ui::EF_CAPS_LOCK_ON,
                                      ui::DomKey::META));
  EXPECT_TRUE(ime_keyboard.caps_lock_is_enabled_);

  // Release Search.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_RELEASED, ui::VKEY_CAPITAL,
                                      ui::DomCode::CAPS_LOCK, ui::EF_NONE,
                                      ui::DomKey::CAPS_LOCK),
            GetRewrittenEventAsString(rewriter_, ui::ET_KEY_RELEASED,
                                      ui::VKEY_LWIN, ui::DomCode::META_LEFT,
                                      ui::EF_NONE, ui::DomKey::META));
  EXPECT_FALSE(ime_keyboard.caps_lock_is_enabled_);

  // Press Caps Lock (on an external keyboard).
  EXPECT_EQ(GetExpectedResultAsString(
                ui::ET_KEY_PRESSED, ui::VKEY_CAPITAL, ui::DomCode::CAPS_LOCK,
                ui::EF_CAPS_LOCK_ON | ui::EF_MOD3_DOWN, ui::DomKey::CAPS_LOCK),
            GetRewrittenEventAsString(rewriter_, ui::ET_KEY_PRESSED,
                                      ui::VKEY_CAPITAL, ui::DomCode::CAPS_LOCK,
                                      ui::EF_CAPS_LOCK_ON | ui::EF_MOD3_DOWN,
                                      ui::DomKey::CAPS_LOCK));
  EXPECT_FALSE(ime_keyboard.caps_lock_is_enabled_);

  // Release Caps Lock (on an external keyboard).
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_RELEASED, ui::VKEY_CAPITAL,
                                      ui::DomCode::CAPS_LOCK, ui::EF_NONE,
                                      ui::DomKey::CAPS_LOCK),
            GetRewrittenEventAsString(rewriter_, ui::ET_KEY_RELEASED,
                                      ui::VKEY_CAPITAL, ui::DomCode::CAPS_LOCK,
                                      ui::EF_NONE, ui::DomKey::CAPS_LOCK));
  EXPECT_TRUE(ime_keyboard.caps_lock_is_enabled_);
}

TEST_F(EventRewriterTest, TestRewriteCapsLock) {
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());

  chromeos::input_method::FakeImeKeyboard ime_keyboard;
  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter_->set_ime_keyboard_for_testing(&ime_keyboard);
  EXPECT_FALSE(ime_keyboard.caps_lock_is_enabled_);

  // On Chrome OS, CapsLock is mapped to CapsLock with Mod3Mask.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::ET_KEY_PRESSED, ui::VKEY_CAPITAL, ui::DomCode::CAPS_LOCK,
                ui::EF_CAPS_LOCK_ON | ui::EF_MOD3_DOWN, ui::DomKey::CAPS_LOCK),
            GetRewrittenEventAsString(rewriter_, ui::ET_KEY_PRESSED,
                                      ui::VKEY_CAPITAL, ui::DomCode::CAPS_LOCK,
                                      ui::EF_MOD3_DOWN, ui::DomKey::CAPS_LOCK));
  EXPECT_FALSE(ime_keyboard.caps_lock_is_enabled_);

  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_RELEASED, ui::VKEY_CAPITAL,
                                      ui::DomCode::CAPS_LOCK, ui::EF_NONE,
                                      ui::DomKey::CAPS_LOCK),
            GetRewrittenEventAsString(rewriter_, ui::ET_KEY_RELEASED,
                                      ui::VKEY_CAPITAL, ui::DomCode::CAPS_LOCK,
                                      ui::EF_MOD3_DOWN, ui::DomKey::CAPS_LOCK));
  EXPECT_TRUE(ime_keyboard.caps_lock_is_enabled_);

  // Remap Caps Lock to Control.
  IntegerPrefMember caps_lock;
  InitModifierKeyPref(&caps_lock, prefs::kLanguageRemapCapsLockKeyTo,
                      ui::chromeos::ModifierKey::kControlKey);

  // Press Caps Lock.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL,
                                      ui::DomCode::CONTROL_LEFT,
                                      ui::EF_CONTROL_DOWN, ui::DomKey::CONTROL),
            GetRewrittenEventAsString(rewriter_, ui::ET_KEY_PRESSED,
                                      ui::VKEY_CAPITAL, ui::DomCode::CAPS_LOCK,
                                      ui::EF_CAPS_LOCK_ON | ui::EF_MOD3_DOWN,
                                      ui::DomKey::CAPS_LOCK));

  // Release Caps Lock.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_RELEASED, ui::VKEY_CONTROL,
                                      ui::DomCode::CONTROL_LEFT, ui::EF_NONE,
                                      ui::DomKey::CONTROL),
            GetRewrittenEventAsString(rewriter_, ui::ET_KEY_RELEASED,
                                      ui::VKEY_CAPITAL, ui::DomCode::CAPS_LOCK,
                                      ui::EF_NONE, ui::DomKey::CAPS_LOCK));
}

TEST_F(EventRewriterTest, TestRewriteCapsLockToControl) {
  // Remap CapsLock to Control.
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember control;
  InitModifierKeyPref(&control, prefs::kLanguageRemapCapsLockKeyTo,
                      ui::chromeos::ModifierKey::kControlKey);

  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");

  KeyTestCase tests[] = {
      // Press CapsLock+a. Confirm that Mod3Mask is rewritten to ControlMask.
      // On Chrome OS, CapsLock works as a Mod3 modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_MOD3_DOWN,
        ui::DomKey::Constant<'a'>::Character},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'a'>::Character}},

      // Press Control+CapsLock+a. Confirm that Mod3Mask is rewritten to
      // ControlMask
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_CONTROL_DOWN | ui::EF_MOD3_DOWN,
        ui::DomKey::Constant<'a'>::Character},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'a'>::Character}},

      // Press Alt+CapsLock+a. Confirm that Mod3Mask is rewritten to
      // ControlMask.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_MOD3_DOWN,
        ui::DomKey::Constant<'a'>::Character},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'a'>::Character}},
  };

  for (const auto& test : tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestRewriteCapsLockMod3InUse) {
  // Remap CapsLock to Control.
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember control;
  InitModifierKeyPref(&control, prefs::kLanguageRemapCapsLockKeyTo,
                      ui::chromeos::ModifierKey::kControlKey);

  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  input_method_manager_mock_->set_mod3_used(true);

  // Press CapsLock+a. Confirm that Mod3Mask is NOT rewritten to ControlMask
  // when Mod3Mask is already in use by the current XKB layout.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_NONE,
                                      ui::DomKey::Constant<'a'>::Character),
            GetRewrittenEventAsString(rewriter_, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_NONE,
                                      ui::DomKey::Constant<'a'>::Character));

  input_method_manager_mock_->set_mod3_used(false);
}

TEST_F(EventRewriterTest, TestRewriteExtendedKeys) {
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());
  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter_->set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);

  KeyTestCase tests[] = {
      // Alt+Backspace -> Delete
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK, ui::DomCode::BACKSPACE, ui::EF_ALT_DOWN,
        ui::DomKey::BACKSPACE},
       {ui::VKEY_DELETE, ui::DomCode::DEL, ui::EF_NONE, ui::DomKey::DEL}},
      // Control+Alt+Backspace -> Control+Delete
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK, ui::DomCode::BACKSPACE,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, ui::DomKey::BACKSPACE},
       {ui::VKEY_DELETE, ui::DomCode::DEL, ui::EF_CONTROL_DOWN,
        ui::DomKey::DEL}},
      // Search+Alt+Backspace -> Alt+Backspace
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK, ui::DomCode::BACKSPACE,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::BACKSPACE},
       {ui::VKEY_BACK, ui::DomCode::BACKSPACE, ui::EF_ALT_DOWN,
        ui::DomKey::BACKSPACE}},
      // Search+Control+Alt+Backspace -> Control+Alt+Backspace
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK, ui::DomCode::BACKSPACE,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::BACKSPACE},
       {ui::VKEY_BACK, ui::DomCode::BACKSPACE,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, ui::DomKey::BACKSPACE}},
      // Alt+Up -> Prior
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_UP, ui::DomCode::ARROW_UP, ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_UP},
       {ui::VKEY_PRIOR, ui::DomCode::PAGE_UP, ui::EF_NONE,
        ui::DomKey::PAGE_UP}},
      // Alt+Down -> Next
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::DomCode::ARROW_DOWN, ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_DOWN},
       {ui::VKEY_NEXT, ui::DomCode::PAGE_DOWN, ui::EF_NONE,
        ui::DomKey::PAGE_DOWN}},
      // Ctrl+Alt+Up -> Home
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_UP, ui::DomCode::ARROW_UP,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, ui::DomKey::ARROW_UP},
       {ui::VKEY_HOME, ui::DomCode::HOME, ui::EF_NONE, ui::DomKey::HOME}},
      // Ctrl+Alt+Down -> End
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::DomCode::ARROW_DOWN,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, ui::DomKey::ARROW_DOWN},
       {ui::VKEY_END, ui::DomCode::END, ui::EF_NONE, ui::DomKey::END}},

      // Search+Ctrl+Alt+Up -> Ctrl+Alt+Up
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_UP, ui::DomCode::ARROW_UP,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::ARROW_UP},
       {ui::VKEY_UP, ui::DomCode::ARROW_UP,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, ui::DomKey::ARROW_UP}},
      // Search+Ctrl+Alt+Down -> Ctrl+Alt+Down
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::DomCode::ARROW_DOWN,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::ARROW_DOWN},
       {ui::VKEY_DOWN, ui::DomCode::ARROW_DOWN,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, ui::DomKey::ARROW_DOWN}},

      // Period -> Period
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PERIOD, ui::DomCode::PERIOD, ui::EF_NONE,
        ui::DomKey::Constant<'.'>::Character},
       {ui::VKEY_OEM_PERIOD, ui::DomCode::PERIOD, ui::EF_NONE,
        ui::DomKey::Constant<'.'>::Character}},

      // Search+Backspace -> Delete
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK, ui::DomCode::BACKSPACE, ui::EF_COMMAND_DOWN,
        ui::DomKey::BACKSPACE},
       {ui::VKEY_DELETE, ui::DomCode::DEL, ui::EF_NONE, ui::DomKey::DEL}},
      // Search+Up -> Prior
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_UP, ui::DomCode::ARROW_UP, ui::EF_COMMAND_DOWN,
        ui::DomKey::ARROW_UP},
       {ui::VKEY_PRIOR, ui::DomCode::PAGE_UP, ui::EF_NONE,
        ui::DomKey::PAGE_UP}},
      // Search+Down -> Next
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::DomCode::ARROW_DOWN, ui::EF_COMMAND_DOWN,
        ui::DomKey::ARROW_DOWN},
       {ui::VKEY_NEXT, ui::DomCode::PAGE_DOWN, ui::EF_NONE,
        ui::DomKey::PAGE_DOWN}},
      // Search+Left -> Home
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LEFT, ui::DomCode::ARROW_LEFT, ui::EF_COMMAND_DOWN,
        ui::DomKey::ARROW_LEFT},
       {ui::VKEY_HOME, ui::DomCode::HOME, ui::EF_NONE, ui::DomKey::HOME}},
      // Control+Search+Left -> Home
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LEFT, ui::DomCode::ARROW_LEFT,
        ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN, ui::DomKey::ARROW_LEFT},
       {ui::VKEY_HOME, ui::DomCode::HOME, ui::EF_CONTROL_DOWN,
        ui::DomKey::HOME}},
      // Search+Right -> End
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_RIGHT, ui::DomCode::ARROW_RIGHT, ui::EF_COMMAND_DOWN,
        ui::DomKey::ARROW_RIGHT},
       {ui::VKEY_END, ui::DomCode::END, ui::EF_NONE, ui::DomKey::END}},
      // Control+Search+Right -> End
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_RIGHT, ui::DomCode::ARROW_RIGHT,
        ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN, ui::DomKey::ARROW_RIGHT},
       {ui::VKEY_END, ui::DomCode::END, ui::EF_CONTROL_DOWN, ui::DomKey::END}},
      // Search+Period -> Insert
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PERIOD, ui::DomCode::PERIOD, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'.'>::Character},
       {ui::VKEY_INSERT, ui::DomCode::INSERT, ui::EF_NONE, ui::DomKey::INSERT}},
      // Control+Search+Period -> Control+Insert
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PERIOD, ui::DomCode::PERIOD,
        ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'.'>::Character},
       {ui::VKEY_INSERT, ui::DomCode::INSERT, ui::EF_CONTROL_DOWN,
        ui::DomKey::INSERT}}};

  for (const auto& test : tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestRewriteFunctionKeys) {
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());
  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");

  KeyTestCase tests[] = {
      // F1 -> Back
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_NONE, ui::DomKey::F1},
       {ui::VKEY_BROWSER_BACK, ui::DomCode::BROWSER_BACK, ui::EF_NONE,
        ui::DomKey::BROWSER_BACK}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_CONTROL_DOWN, ui::DomKey::F1},
       {ui::VKEY_BROWSER_BACK, ui::DomCode::BROWSER_BACK, ui::EF_CONTROL_DOWN,
        ui::DomKey::BROWSER_BACK}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_ALT_DOWN, ui::DomKey::F1},
       {ui::VKEY_BROWSER_BACK, ui::DomCode::BROWSER_BACK, ui::EF_ALT_DOWN,
        ui::DomKey::BROWSER_BACK}},
      // F2 -> Forward
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_NONE, ui::DomKey::F2},
       {ui::VKEY_BROWSER_FORWARD, ui::DomCode::BROWSER_FORWARD, ui::EF_NONE,
        ui::DomKey::BROWSER_FORWARD}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_CONTROL_DOWN, ui::DomKey::F2},
       {ui::VKEY_BROWSER_FORWARD, ui::DomCode::BROWSER_FORWARD,
        ui::EF_CONTROL_DOWN, ui::DomKey::BROWSER_FORWARD}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_ALT_DOWN, ui::DomKey::F2},
       {ui::VKEY_BROWSER_FORWARD, ui::DomCode::BROWSER_FORWARD, ui::EF_ALT_DOWN,
        ui::DomKey::BROWSER_FORWARD}},
      // F3 -> Refresh
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_NONE, ui::DomKey::F3},
       {ui::VKEY_BROWSER_REFRESH, ui::DomCode::BROWSER_REFRESH, ui::EF_NONE,
        ui::DomKey::BROWSER_REFRESH}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_CONTROL_DOWN, ui::DomKey::F3},
       {ui::VKEY_BROWSER_REFRESH, ui::DomCode::BROWSER_REFRESH,
        ui::EF_CONTROL_DOWN, ui::DomKey::BROWSER_REFRESH}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_ALT_DOWN, ui::DomKey::F3},
       {ui::VKEY_BROWSER_REFRESH, ui::DomCode::BROWSER_REFRESH, ui::EF_ALT_DOWN,
        ui::DomKey::BROWSER_REFRESH}},
      // F4 -> Launch App 2
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_NONE, ui::DomKey::F4},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::ZOOM_TOGGLE, ui::EF_NONE,
        ui::DomKey::ZOOM_TOGGLE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_CONTROL_DOWN, ui::DomKey::F4},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::ZOOM_TOGGLE,
        ui::EF_CONTROL_DOWN, ui::DomKey::ZOOM_TOGGLE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_ALT_DOWN, ui::DomKey::F4},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::ZOOM_TOGGLE, ui::EF_ALT_DOWN,
        ui::DomKey::ZOOM_TOGGLE}},
      // F5 -> Launch App 1
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_NONE, ui::DomKey::F5},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::DomCode::SELECT_TASK, ui::EF_NONE,
        ui::DomKey::LAUNCH_MY_COMPUTER}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_CONTROL_DOWN, ui::DomKey::F5},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::DomCode::SELECT_TASK,
        ui::EF_CONTROL_DOWN, ui::DomKey::LAUNCH_MY_COMPUTER}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_ALT_DOWN, ui::DomKey::F5},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::DomCode::SELECT_TASK, ui::EF_ALT_DOWN,
        ui::DomKey::LAUNCH_MY_COMPUTER}},
      // F6 -> Brightness down
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_NONE, ui::DomKey::F6},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::DomCode::BRIGHTNESS_DOWN, ui::EF_NONE,
        ui::DomKey::BRIGHTNESS_DOWN}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_CONTROL_DOWN, ui::DomKey::F6},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::DomCode::BRIGHTNESS_DOWN,
        ui::EF_CONTROL_DOWN, ui::DomKey::BRIGHTNESS_DOWN}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_ALT_DOWN, ui::DomKey::F6},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::DomCode::BRIGHTNESS_DOWN, ui::EF_ALT_DOWN,
        ui::DomKey::BRIGHTNESS_DOWN}},
      // F7 -> Brightness up
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_NONE, ui::DomKey::F7},
       {ui::VKEY_BRIGHTNESS_UP, ui::DomCode::BRIGHTNESS_UP, ui::EF_NONE,
        ui::DomKey::BRIGHTNESS_UP}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_CONTROL_DOWN, ui::DomKey::F7},
       {ui::VKEY_BRIGHTNESS_UP, ui::DomCode::BRIGHTNESS_UP, ui::EF_CONTROL_DOWN,
        ui::DomKey::BRIGHTNESS_UP}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_ALT_DOWN, ui::DomKey::F7},
       {ui::VKEY_BRIGHTNESS_UP, ui::DomCode::BRIGHTNESS_UP, ui::EF_ALT_DOWN,
        ui::DomKey::BRIGHTNESS_UP}},
      // F8 -> Volume Mute
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_NONE, ui::DomKey::F8},
       {ui::VKEY_VOLUME_MUTE, ui::DomCode::VOLUME_MUTE, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_MUTE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_CONTROL_DOWN, ui::DomKey::F8},
       {ui::VKEY_VOLUME_MUTE, ui::DomCode::VOLUME_MUTE, ui::EF_CONTROL_DOWN,
        ui::DomKey::AUDIO_VOLUME_MUTE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_ALT_DOWN, ui::DomKey::F8},
       {ui::VKEY_VOLUME_MUTE, ui::DomCode::VOLUME_MUTE, ui::EF_ALT_DOWN,
        ui::DomKey::AUDIO_VOLUME_MUTE}},
      // F9 -> Volume Down
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_NONE, ui::DomKey::F9},
       {ui::VKEY_VOLUME_DOWN, ui::DomCode::VOLUME_DOWN, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_DOWN}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_CONTROL_DOWN, ui::DomKey::F9},
       {ui::VKEY_VOLUME_DOWN, ui::DomCode::VOLUME_DOWN, ui::EF_CONTROL_DOWN,
        ui::DomKey::AUDIO_VOLUME_DOWN}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_ALT_DOWN, ui::DomKey::F9},
       {ui::VKEY_VOLUME_DOWN, ui::DomCode::VOLUME_DOWN, ui::EF_ALT_DOWN,
        ui::DomKey::AUDIO_VOLUME_DOWN}},
      // F10 -> Volume Up
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_NONE, ui::DomKey::F10},
       {ui::VKEY_VOLUME_UP, ui::DomCode::VOLUME_UP, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_UP}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_CONTROL_DOWN, ui::DomKey::F10},
       {ui::VKEY_VOLUME_UP, ui::DomCode::VOLUME_UP, ui::EF_CONTROL_DOWN,
        ui::DomKey::AUDIO_VOLUME_UP}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_ALT_DOWN, ui::DomKey::F10},
       {ui::VKEY_VOLUME_UP, ui::DomCode::VOLUME_UP, ui::EF_ALT_DOWN,
        ui::DomKey::AUDIO_VOLUME_UP}},
      // F11 -> F11
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_CONTROL_DOWN, ui::DomKey::F11},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_CONTROL_DOWN, ui::DomKey::F11}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_ALT_DOWN, ui::DomKey::F11},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_ALT_DOWN, ui::DomKey::F11}},
      // F12 -> F12
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_NONE, ui::DomKey::F12},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_NONE, ui::DomKey::F12}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_CONTROL_DOWN, ui::DomKey::F12},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_CONTROL_DOWN, ui::DomKey::F12}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_ALT_DOWN, ui::DomKey::F12},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_ALT_DOWN, ui::DomKey::F12}},

      // The number row should not be rewritten without Search key.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_1, ui::DomCode::DIGIT1, ui::EF_NONE,
        ui::DomKey::Constant<'1'>::Character},
       {ui::VKEY_1, ui::DomCode::DIGIT1, ui::EF_NONE,
        ui::DomKey::Constant<'1'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_2, ui::DomCode::DIGIT2, ui::EF_NONE,
        ui::DomKey::Constant<'2'>::Character},
       {ui::VKEY_2, ui::DomCode::DIGIT2, ui::EF_NONE,
        ui::DomKey::Constant<'2'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_3, ui::DomCode::DIGIT3, ui::EF_NONE,
        ui::DomKey::Constant<'3'>::Character},
       {ui::VKEY_3, ui::DomCode::DIGIT3, ui::EF_NONE,
        ui::DomKey::Constant<'3'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_4, ui::DomCode::DIGIT4, ui::EF_NONE,
        ui::DomKey::Constant<'4'>::Character},
       {ui::VKEY_4, ui::DomCode::DIGIT4, ui::EF_NONE,
        ui::DomKey::Constant<'4'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_5, ui::DomCode::DIGIT5, ui::EF_NONE,
        ui::DomKey::Constant<'5'>::Character},
       {ui::VKEY_5, ui::DomCode::DIGIT5, ui::EF_NONE,
        ui::DomKey::Constant<'5'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_6, ui::DomCode::DIGIT6, ui::EF_NONE,
        ui::DomKey::Constant<'6'>::Character},
       {ui::VKEY_6, ui::DomCode::DIGIT6, ui::EF_NONE,
        ui::DomKey::Constant<'6'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_7, ui::DomCode::DIGIT7, ui::EF_NONE,
        ui::DomKey::Constant<'7'>::Character},
       {ui::VKEY_7, ui::DomCode::DIGIT7, ui::EF_NONE,
        ui::DomKey::Constant<'7'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_8, ui::DomCode::DIGIT8, ui::EF_NONE,
        ui::DomKey::Constant<'8'>::Character},
       {ui::VKEY_8, ui::DomCode::DIGIT8, ui::EF_NONE,
        ui::DomKey::Constant<'8'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_9, ui::DomCode::DIGIT9, ui::EF_NONE,
        ui::DomKey::Constant<'9'>::Character},
       {ui::VKEY_9, ui::DomCode::DIGIT9, ui::EF_NONE,
        ui::DomKey::Constant<'9'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_0, ui::DomCode::DIGIT0, ui::EF_NONE,
        ui::DomKey::Constant<'0'>::Character},
       {ui::VKEY_0, ui::DomCode::DIGIT0, ui::EF_NONE,
        ui::DomKey::Constant<'0'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_MINUS, ui::DomCode::MINUS, ui::EF_NONE,
        ui::DomKey::Constant<'-'>::Character},
       {ui::VKEY_OEM_MINUS, ui::DomCode::MINUS, ui::EF_NONE,
        ui::DomKey::Constant<'-'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PLUS, ui::DomCode::EQUAL, ui::EF_NONE,
        ui::DomKey::Constant<'='>::Character},
       {ui::VKEY_OEM_PLUS, ui::DomCode::EQUAL, ui::EF_NONE,
        ui::DomKey::Constant<'='>::Character}},

      // The number row should be rewritten as the F<number> row with Search
      // key.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_1, ui::DomCode::DIGIT1, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'1'>::Character},
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_NONE, ui::DomKey::F1}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_2, ui::DomCode::DIGIT2, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'2'>::Character},
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_NONE, ui::DomKey::F2}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_3, ui::DomCode::DIGIT3, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'3'>::Character},
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_NONE, ui::DomKey::F3}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_4, ui::DomCode::DIGIT4, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'4'>::Character},
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_NONE, ui::DomKey::F4}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_5, ui::DomCode::DIGIT5, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'5'>::Character},
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_NONE, ui::DomKey::F5}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_6, ui::DomCode::DIGIT6, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'6'>::Character},
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_NONE, ui::DomKey::F6}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_7, ui::DomCode::DIGIT7, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'7'>::Character},
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_NONE, ui::DomKey::F7}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_8, ui::DomCode::DIGIT8, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'8'>::Character},
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_NONE, ui::DomKey::F8}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_9, ui::DomCode::DIGIT9, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'9'>::Character},
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_NONE, ui::DomKey::F9}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_0, ui::DomCode::DIGIT0, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'0'>::Character},
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_NONE, ui::DomKey::F10}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_MINUS, ui::DomCode::MINUS, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'-'>::Character},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PLUS, ui::DomCode::EQUAL, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'='>::Character},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_NONE, ui::DomKey::F12}},

      // The function keys should not be rewritten with Search key pressed.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_COMMAND_DOWN, ui::DomKey::F1},
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_NONE, ui::DomKey::F1}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_COMMAND_DOWN, ui::DomKey::F2},
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_NONE, ui::DomKey::F2}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_COMMAND_DOWN, ui::DomKey::F3},
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_NONE, ui::DomKey::F3}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_COMMAND_DOWN, ui::DomKey::F4},
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_NONE, ui::DomKey::F4}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_COMMAND_DOWN, ui::DomKey::F5},
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_NONE, ui::DomKey::F5}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_COMMAND_DOWN, ui::DomKey::F6},
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_NONE, ui::DomKey::F6}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_COMMAND_DOWN, ui::DomKey::F7},
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_NONE, ui::DomKey::F7}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_COMMAND_DOWN, ui::DomKey::F8},
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_NONE, ui::DomKey::F8}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_COMMAND_DOWN, ui::DomKey::F9},
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_NONE, ui::DomKey::F9}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_COMMAND_DOWN, ui::DomKey::F10},
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_NONE, ui::DomKey::F10}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_COMMAND_DOWN, ui::DomKey::F11},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_COMMAND_DOWN, ui::DomKey::F12},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_NONE, ui::DomKey::F12}}};

  for (const auto& test : tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestRewriteFunctionKeysLayout2) {
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());
  rewriter_->KeyboardDeviceAddedForTesting(
      kKeyboardDeviceId, "PC Keyboard",
      ui::EventRewriterChromeOS::kKbdTopRowLayout2);

  KeyTestCase tests[] = {
      // F1 -> Back
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_NONE, ui::DomKey::F1},
       {ui::VKEY_BROWSER_BACK, ui::DomCode::BROWSER_BACK, ui::EF_NONE,
        ui::DomKey::BROWSER_BACK}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_CONTROL_DOWN, ui::DomKey::F1},
       {ui::VKEY_BROWSER_BACK, ui::DomCode::BROWSER_BACK, ui::EF_CONTROL_DOWN,
        ui::DomKey::BROWSER_BACK}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_ALT_DOWN, ui::DomKey::F1},
       {ui::VKEY_BROWSER_BACK, ui::DomCode::BROWSER_BACK, ui::EF_ALT_DOWN,
        ui::DomKey::BROWSER_BACK}},
      // F2 -> Refresh
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_NONE, ui::DomKey::F2},
       {ui::VKEY_BROWSER_REFRESH, ui::DomCode::BROWSER_REFRESH, ui::EF_NONE,
        ui::DomKey::BROWSER_REFRESH}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_CONTROL_DOWN, ui::DomKey::F2},
       {ui::VKEY_BROWSER_REFRESH, ui::DomCode::BROWSER_REFRESH,
        ui::EF_CONTROL_DOWN, ui::DomKey::BROWSER_REFRESH}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_ALT_DOWN, ui::DomKey::F2},
       {ui::VKEY_BROWSER_REFRESH, ui::DomCode::BROWSER_REFRESH, ui::EF_ALT_DOWN,
        ui::DomKey::BROWSER_REFRESH}},
      // F3 -> Launch App 2
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_NONE, ui::DomKey::F3},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::ZOOM_TOGGLE, ui::EF_NONE,
        ui::DomKey::ZOOM_TOGGLE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_CONTROL_DOWN, ui::DomKey::F3},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::ZOOM_TOGGLE,
        ui::EF_CONTROL_DOWN, ui::DomKey::ZOOM_TOGGLE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_ALT_DOWN, ui::DomKey::F3},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::ZOOM_TOGGLE, ui::EF_ALT_DOWN,
        ui::DomKey::ZOOM_TOGGLE}},
      // F4 -> Launch App 1
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_NONE, ui::DomKey::F4},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::DomCode::SELECT_TASK, ui::EF_NONE,
        ui::DomKey::LAUNCH_MY_COMPUTER}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_CONTROL_DOWN, ui::DomKey::F4},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::DomCode::SELECT_TASK,
        ui::EF_CONTROL_DOWN, ui::DomKey::LAUNCH_MY_COMPUTER}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_ALT_DOWN, ui::DomKey::F4},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::DomCode::SELECT_TASK, ui::EF_ALT_DOWN,
        ui::DomKey::LAUNCH_MY_COMPUTER}},
      // F5 -> Brightness down
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_NONE, ui::DomKey::F5},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::DomCode::BRIGHTNESS_DOWN, ui::EF_NONE,
        ui::DomKey::BRIGHTNESS_DOWN}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_CONTROL_DOWN, ui::DomKey::F5},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::DomCode::BRIGHTNESS_DOWN,
        ui::EF_CONTROL_DOWN, ui::DomKey::BRIGHTNESS_DOWN}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_ALT_DOWN, ui::DomKey::F5},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::DomCode::BRIGHTNESS_DOWN, ui::EF_ALT_DOWN,
        ui::DomKey::BRIGHTNESS_DOWN}},
      // F6 -> Brightness up
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_NONE, ui::DomKey::F6},
       {ui::VKEY_BRIGHTNESS_UP, ui::DomCode::BRIGHTNESS_UP, ui::EF_NONE,
        ui::DomKey::BRIGHTNESS_UP}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_CONTROL_DOWN, ui::DomKey::F6},
       {ui::VKEY_BRIGHTNESS_UP, ui::DomCode::BRIGHTNESS_UP, ui::EF_CONTROL_DOWN,
        ui::DomKey::BRIGHTNESS_UP}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_ALT_DOWN, ui::DomKey::F6},
       {ui::VKEY_BRIGHTNESS_UP, ui::DomCode::BRIGHTNESS_UP, ui::EF_ALT_DOWN,
        ui::DomKey::BRIGHTNESS_UP}},
      // F7 -> Media Play/Pause
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_NONE, ui::DomKey::F7},
       {ui::VKEY_MEDIA_PLAY_PAUSE, ui::DomCode::MEDIA_PLAY_PAUSE, ui::EF_NONE,
        ui::DomKey::MEDIA_PLAY_PAUSE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_CONTROL_DOWN, ui::DomKey::F7},
       {ui::VKEY_MEDIA_PLAY_PAUSE, ui::DomCode::MEDIA_PLAY_PAUSE,
        ui::EF_CONTROL_DOWN, ui::DomKey::MEDIA_PLAY_PAUSE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_ALT_DOWN, ui::DomKey::F7},
       {ui::VKEY_MEDIA_PLAY_PAUSE, ui::DomCode::MEDIA_PLAY_PAUSE,
        ui::EF_ALT_DOWN, ui::DomKey::MEDIA_PLAY_PAUSE}},
      // F8 -> Volume Mute
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_NONE, ui::DomKey::F8},
       {ui::VKEY_VOLUME_MUTE, ui::DomCode::VOLUME_MUTE, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_MUTE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_CONTROL_DOWN, ui::DomKey::F8},
       {ui::VKEY_VOLUME_MUTE, ui::DomCode::VOLUME_MUTE, ui::EF_CONTROL_DOWN,
        ui::DomKey::AUDIO_VOLUME_MUTE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_ALT_DOWN, ui::DomKey::F8},
       {ui::VKEY_VOLUME_MUTE, ui::DomCode::VOLUME_MUTE, ui::EF_ALT_DOWN,
        ui::DomKey::AUDIO_VOLUME_MUTE}},
      // F9 -> Volume Down
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_NONE, ui::DomKey::F9},
       {ui::VKEY_VOLUME_DOWN, ui::DomCode::VOLUME_DOWN, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_DOWN}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_CONTROL_DOWN, ui::DomKey::F9},
       {ui::VKEY_VOLUME_DOWN, ui::DomCode::VOLUME_DOWN, ui::EF_CONTROL_DOWN,
        ui::DomKey::AUDIO_VOLUME_DOWN}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_ALT_DOWN, ui::DomKey::F9},
       {ui::VKEY_VOLUME_DOWN, ui::DomCode::VOLUME_DOWN, ui::EF_ALT_DOWN,
        ui::DomKey::AUDIO_VOLUME_DOWN}},
      // F10 -> Volume Up
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_NONE, ui::DomKey::F10},
       {ui::VKEY_VOLUME_UP, ui::DomCode::VOLUME_UP, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_UP}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_CONTROL_DOWN, ui::DomKey::F10},
       {ui::VKEY_VOLUME_UP, ui::DomCode::VOLUME_UP, ui::EF_CONTROL_DOWN,
        ui::DomKey::AUDIO_VOLUME_UP}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_ALT_DOWN, ui::DomKey::F10},
       {ui::VKEY_VOLUME_UP, ui::DomCode::VOLUME_UP, ui::EF_ALT_DOWN,
        ui::DomKey::AUDIO_VOLUME_UP}},
      // F11 -> F11
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_CONTROL_DOWN, ui::DomKey::F11},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_CONTROL_DOWN, ui::DomKey::F11}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_ALT_DOWN, ui::DomKey::F11},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_ALT_DOWN, ui::DomKey::F11}},
      // F12 -> F12
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_NONE, ui::DomKey::F12},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_NONE, ui::DomKey::F12}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_CONTROL_DOWN, ui::DomKey::F12},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_CONTROL_DOWN, ui::DomKey::F12}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_ALT_DOWN, ui::DomKey::F12},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_ALT_DOWN, ui::DomKey::F12}}};

  for (const auto& test : tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestRewriteFunctionKeysLayout3) {
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());
  rewriter_->KeyboardDeviceAddedForTesting(
      kKeyboardDeviceId, "PC Keyboard",
      ui::EventRewriterChromeOS::kKbdTopRowLayoutWilco);

  KeyTestCase tests[] = {
      // F1 -> F1, Search + F1 -> Back
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_NONE, ui::DomKey::F1},
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_NONE, ui::DomKey::F1}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_COMMAND_DOWN, ui::DomKey::F1},
       {ui::VKEY_BROWSER_BACK, ui::DomCode::BROWSER_BACK, ui::EF_NONE,
        ui::DomKey::BROWSER_BACK}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_CONTROL_DOWN, ui::DomKey::F1},
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_CONTROL_DOWN, ui::DomKey::F1}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_ALT_DOWN, ui::DomKey::F1},
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_ALT_DOWN, ui::DomKey::F1}},
      // F2 -> F2, Search + F2 -> Refresh
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_NONE, ui::DomKey::F2},
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_NONE, ui::DomKey::F2}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_COMMAND_DOWN, ui::DomKey::F2},
       {ui::VKEY_BROWSER_REFRESH, ui::DomCode::BROWSER_REFRESH, ui::EF_NONE,
        ui::DomKey::BROWSER_REFRESH}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_CONTROL_DOWN, ui::DomKey::F2},
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_CONTROL_DOWN, ui::DomKey::F2}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_ALT_DOWN, ui::DomKey::F2},
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_ALT_DOWN, ui::DomKey::F2}},
      // F3 -> F3, Search + F3 -> Full Screen
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_NONE, ui::DomKey::F3},
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_NONE, ui::DomKey::F3}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_COMMAND_DOWN, ui::DomKey::F3},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::ZOOM_TOGGLE, ui::EF_NONE,
        ui::DomKey::ZOOM_TOGGLE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_CONTROL_DOWN, ui::DomKey::F3},
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_CONTROL_DOWN, ui::DomKey::F3}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_ALT_DOWN, ui::DomKey::F3},
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_ALT_DOWN, ui::DomKey::F3}},
      // F4 -> F4, Search + F4 -> Launch App 1
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_NONE, ui::DomKey::F4},
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_NONE, ui::DomKey::F4}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_COMMAND_DOWN, ui::DomKey::F4},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::DomCode::F4, ui::EF_NONE,
        ui::DomKey::F4}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_CONTROL_DOWN, ui::DomKey::F4},
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_CONTROL_DOWN, ui::DomKey::F4}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_ALT_DOWN, ui::DomKey::F4},
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_ALT_DOWN, ui::DomKey::F4}},
      // F5 -> F5, Search + F5 -> Brightness down
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_NONE, ui::DomKey::F5},
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_NONE, ui::DomKey::F5}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_COMMAND_DOWN, ui::DomKey::F5},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::DomCode::BRIGHTNESS_DOWN, ui::EF_NONE,
        ui::DomKey::BRIGHTNESS_DOWN}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_CONTROL_DOWN, ui::DomKey::F5},
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_CONTROL_DOWN, ui::DomKey::F5}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_ALT_DOWN, ui::DomKey::F5},
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_ALT_DOWN, ui::DomKey::F5}},
      // F6 -> F6, Search + F6 -> Brightness up
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_NONE, ui::DomKey::F6},
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_NONE, ui::DomKey::F6}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_COMMAND_DOWN, ui::DomKey::F6},
       {ui::VKEY_BRIGHTNESS_UP, ui::DomCode::BRIGHTNESS_UP, ui::EF_NONE,
        ui::DomKey::BRIGHTNESS_UP}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_CONTROL_DOWN, ui::DomKey::F6},
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_CONTROL_DOWN, ui::DomKey::F6}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_ALT_DOWN, ui::DomKey::F6},
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_ALT_DOWN, ui::DomKey::F6}},
      // F7 -> F7, Search + F7 -> Volume mute
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_NONE, ui::DomKey::F7},
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_NONE, ui::DomKey::F7}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_COMMAND_DOWN, ui::DomKey::F7},
       {ui::VKEY_VOLUME_MUTE, ui::DomCode::VOLUME_MUTE, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_MUTE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_CONTROL_DOWN, ui::DomKey::F7},
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_CONTROL_DOWN, ui::DomKey::F7}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_ALT_DOWN, ui::DomKey::F7},
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_ALT_DOWN, ui::DomKey::F7}},
      // F8 -> F8, Search + F8 -> Volume Down
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_NONE, ui::DomKey::F8},
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_NONE, ui::DomKey::F8}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_COMMAND_DOWN, ui::DomKey::F8},
       {ui::VKEY_VOLUME_DOWN, ui::DomCode::VOLUME_DOWN, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_DOWN}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_CONTROL_DOWN, ui::DomKey::F8},
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_CONTROL_DOWN, ui::DomKey::F8}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_ALT_DOWN, ui::DomKey::F8},
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_ALT_DOWN, ui::DomKey::F8}},
      // F9 -> F9, Search + F9 -> Volume Up
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_NONE, ui::DomKey::F9},
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_NONE, ui::DomKey::F9}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_COMMAND_DOWN, ui::DomKey::F9},
       {ui::VKEY_VOLUME_UP, ui::DomCode::VOLUME_UP, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_UP}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_CONTROL_DOWN, ui::DomKey::F9},
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_CONTROL_DOWN, ui::DomKey::F9}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_ALT_DOWN, ui::DomKey::F9},
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_ALT_DOWN, ui::DomKey::F9}},
      // F10 -> F10, Search + F10 -> F10
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_NONE, ui::DomKey::F10},
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_NONE, ui::DomKey::F10}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_COMMAND_DOWN, ui::DomKey::F10},
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_NONE, ui::DomKey::F10}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_CONTROL_DOWN, ui::DomKey::F10},
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_CONTROL_DOWN, ui::DomKey::F10}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_ALT_DOWN, ui::DomKey::F10},
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_ALT_DOWN, ui::DomKey::F10}},
      // F11 -> F11, Search + F11 -> F11
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_COMMAND_DOWN, ui::DomKey::F11},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_CONTROL_DOWN, ui::DomKey::F11},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_CONTROL_DOWN, ui::DomKey::F11}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_ALT_DOWN, ui::DomKey::F11},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_ALT_DOWN, ui::DomKey::F11}},
      // F12 -> F12, Search + F12 -> Ctrl + Launch App 2 (Display toggle)
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_NONE, ui::DomKey::F12},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_NONE, ui::DomKey::F12}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_COMMAND_DOWN, ui::DomKey::F12},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::F12, ui::EF_CONTROL_DOWN,
        ui::DomKey::F12}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_CONTROL_DOWN, ui::DomKey::F12},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_CONTROL_DOWN, ui::DomKey::F12}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_ALT_DOWN, ui::DomKey::F12},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_ALT_DOWN, ui::DomKey::F12}},

      // The number row should not be rewritten without Search key.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_1, ui::DomCode::DIGIT1, ui::EF_NONE,
        ui::DomKey::Constant<'1'>::Character},
       {ui::VKEY_1, ui::DomCode::DIGIT1, ui::EF_NONE,
        ui::DomKey::Constant<'1'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_2, ui::DomCode::DIGIT2, ui::EF_NONE,
        ui::DomKey::Constant<'2'>::Character},
       {ui::VKEY_2, ui::DomCode::DIGIT2, ui::EF_NONE,
        ui::DomKey::Constant<'2'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_3, ui::DomCode::DIGIT3, ui::EF_NONE,
        ui::DomKey::Constant<'3'>::Character},
       {ui::VKEY_3, ui::DomCode::DIGIT3, ui::EF_NONE,
        ui::DomKey::Constant<'3'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_4, ui::DomCode::DIGIT4, ui::EF_NONE,
        ui::DomKey::Constant<'4'>::Character},
       {ui::VKEY_4, ui::DomCode::DIGIT4, ui::EF_NONE,
        ui::DomKey::Constant<'4'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_5, ui::DomCode::DIGIT5, ui::EF_NONE,
        ui::DomKey::Constant<'5'>::Character},
       {ui::VKEY_5, ui::DomCode::DIGIT5, ui::EF_NONE,
        ui::DomKey::Constant<'5'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_6, ui::DomCode::DIGIT6, ui::EF_NONE,
        ui::DomKey::Constant<'6'>::Character},
       {ui::VKEY_6, ui::DomCode::DIGIT6, ui::EF_NONE,
        ui::DomKey::Constant<'6'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_7, ui::DomCode::DIGIT7, ui::EF_NONE,
        ui::DomKey::Constant<'7'>::Character},
       {ui::VKEY_7, ui::DomCode::DIGIT7, ui::EF_NONE,
        ui::DomKey::Constant<'7'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_8, ui::DomCode::DIGIT8, ui::EF_NONE,
        ui::DomKey::Constant<'8'>::Character},
       {ui::VKEY_8, ui::DomCode::DIGIT8, ui::EF_NONE,
        ui::DomKey::Constant<'8'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_9, ui::DomCode::DIGIT9, ui::EF_NONE,
        ui::DomKey::Constant<'9'>::Character},
       {ui::VKEY_9, ui::DomCode::DIGIT9, ui::EF_NONE,
        ui::DomKey::Constant<'9'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_0, ui::DomCode::DIGIT0, ui::EF_NONE,
        ui::DomKey::Constant<'0'>::Character},
       {ui::VKEY_0, ui::DomCode::DIGIT0, ui::EF_NONE,
        ui::DomKey::Constant<'0'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_MINUS, ui::DomCode::MINUS, ui::EF_NONE,
        ui::DomKey::Constant<'-'>::Character},
       {ui::VKEY_OEM_MINUS, ui::DomCode::MINUS, ui::EF_NONE,
        ui::DomKey::Constant<'-'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PLUS, ui::DomCode::EQUAL, ui::EF_NONE,
        ui::DomKey::Constant<'='>::Character},
       {ui::VKEY_OEM_PLUS, ui::DomCode::EQUAL, ui::EF_NONE,
        ui::DomKey::Constant<'='>::Character}},

      // The number row should be rewritten as the F<number> row with Search
      // key.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_1, ui::DomCode::DIGIT1, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'1'>::Character},
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_NONE, ui::DomKey::F1}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_2, ui::DomCode::DIGIT2, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'2'>::Character},
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_NONE, ui::DomKey::F2}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_3, ui::DomCode::DIGIT3, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'3'>::Character},
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_NONE, ui::DomKey::F3}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_4, ui::DomCode::DIGIT4, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'4'>::Character},
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_NONE, ui::DomKey::F4}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_5, ui::DomCode::DIGIT5, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'5'>::Character},
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_NONE, ui::DomKey::F5}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_6, ui::DomCode::DIGIT6, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'6'>::Character},
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_NONE, ui::DomKey::F6}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_7, ui::DomCode::DIGIT7, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'7'>::Character},
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_NONE, ui::DomKey::F7}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_8, ui::DomCode::DIGIT8, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'8'>::Character},
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_NONE, ui::DomKey::F8}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_9, ui::DomCode::DIGIT9, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'9'>::Character},
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_NONE, ui::DomKey::F9}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_0, ui::DomCode::DIGIT0, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'0'>::Character},
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_NONE, ui::DomKey::F10}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_MINUS, ui::DomCode::MINUS, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'-'>::Character},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PLUS, ui::DomCode::EQUAL, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'='>::Character},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_NONE, ui::DomKey::F12}}};

  for (const auto& test : tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestRewriteActionKeysLayout3) {
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());
  rewriter_->KeyboardDeviceAddedForTesting(
      kKeyboardDeviceId, "PC Keyboard",
      ui::EventRewriterChromeOS::kKbdTopRowLayoutWilco);

  KeyTestCase tests[] = {
      // Back -> Back, Search + Back -> F1
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BROWSER_BACK, ui::DomCode::BROWSER_BACK, ui::EF_NONE,
        ui::DomKey::BROWSER_BACK},
       {ui::VKEY_BROWSER_BACK, ui::DomCode::BROWSER_BACK, ui::EF_NONE,
        ui::DomKey::BROWSER_BACK}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BROWSER_BACK, ui::DomCode::BROWSER_BACK, ui::EF_COMMAND_DOWN,
        ui::DomKey::BROWSER_BACK},
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_NONE, ui::DomKey::F1}},
      // Refresh -> Refresh, Search + Refresh -> F2
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BROWSER_REFRESH, ui::DomCode::BROWSER_REFRESH, ui::EF_NONE,
        ui::DomKey::BROWSER_REFRESH},
       {ui::VKEY_BROWSER_REFRESH, ui::DomCode::BROWSER_REFRESH, ui::EF_NONE,
        ui::DomKey::BROWSER_REFRESH}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BROWSER_REFRESH, ui::DomCode::BROWSER_REFRESH,
        ui::EF_COMMAND_DOWN, ui::DomKey::BROWSER_REFRESH},
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_NONE, ui::DomKey::F2}},
      // Full Screen -> Full Screen, Search + Full Screen -> F3
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_ZOOM, ui::DomCode::ZOOM_TOGGLE, ui::EF_NONE,
        ui::DomKey::ZOOM_TOGGLE},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::ZOOM_TOGGLE, ui::EF_NONE,
        ui::DomKey::ZOOM_TOGGLE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_ZOOM, ui::DomCode::ZOOM_TOGGLE, ui::EF_COMMAND_DOWN,
        ui::DomKey::ZOOM_TOGGLE},
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_NONE, ui::DomKey::F3}},
      // Launch App 1 -> Launch App 1, Search + Launch App 1 -> F4
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::DomCode::F4, ui::EF_NONE,
        ui::DomKey::F4},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::DomCode::F4, ui::EF_NONE,
        ui::DomKey::F4}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::DomCode::F4, ui::EF_COMMAND_DOWN,
        ui::DomKey::F4},
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_NONE, ui::DomKey::F4}},
      // Brightness down -> Brightness Down, Search Brightness Down -> F5
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BRIGHTNESS_DOWN, ui::DomCode::BRIGHTNESS_DOWN, ui::EF_NONE,
        ui::DomKey::BRIGHTNESS_DOWN},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::DomCode::BRIGHTNESS_DOWN, ui::EF_NONE,
        ui::DomKey::BRIGHTNESS_DOWN}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BRIGHTNESS_DOWN, ui::DomCode::BRIGHTNESS_DOWN,
        ui::EF_COMMAND_DOWN, ui::DomKey::BRIGHTNESS_DOWN},
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_NONE, ui::DomKey::F5}},
      // Brightness up -> Brightness Up, Search + Brightness Up -> F6
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BRIGHTNESS_UP, ui::DomCode::BRIGHTNESS_UP, ui::EF_NONE,
        ui::DomKey::BRIGHTNESS_UP},
       {ui::VKEY_BRIGHTNESS_UP, ui::DomCode::BRIGHTNESS_UP, ui::EF_NONE,
        ui::DomKey::BRIGHTNESS_UP}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BRIGHTNESS_UP, ui::DomCode::BRIGHTNESS_UP, ui::EF_COMMAND_DOWN,
        ui::DomKey::BRIGHTNESS_UP},
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_NONE, ui::DomKey::F6}},
      // Volume mute -> Volume Mute, Search + Volume Mute -> F7
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_VOLUME_MUTE, ui::DomCode::VOLUME_MUTE, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_MUTE},
       {ui::VKEY_VOLUME_MUTE, ui::DomCode::VOLUME_MUTE, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_MUTE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_VOLUME_MUTE, ui::DomCode::VOLUME_MUTE, ui::EF_COMMAND_DOWN,
        ui::DomKey::AUDIO_VOLUME_MUTE},
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_NONE, ui::DomKey::F7}},
      // Volume Down -> Volume Down, Search + Volume Down -> F8
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_VOLUME_DOWN, ui::DomCode::VOLUME_DOWN, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_DOWN},
       {ui::VKEY_VOLUME_DOWN, ui::DomCode::VOLUME_DOWN, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_DOWN}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_VOLUME_DOWN, ui::DomCode::VOLUME_DOWN, ui::EF_COMMAND_DOWN,
        ui::DomKey::AUDIO_VOLUME_DOWN},
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_NONE, ui::DomKey::F8}},
      // Volume Up -> Volume Up, Search + Volume Up -> F9
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_VOLUME_UP, ui::DomCode::VOLUME_UP, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_UP},
       {ui::VKEY_VOLUME_UP, ui::DomCode::VOLUME_UP, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_UP}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_VOLUME_UP, ui::DomCode::VOLUME_UP, ui::EF_COMMAND_DOWN,
        ui::DomKey::AUDIO_VOLUME_UP},
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_NONE, ui::DomKey::F9}},
      // F10 -> F10
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_NONE, ui::DomKey::F10},
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_NONE, ui::DomKey::F10}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_COMMAND_DOWN, ui::DomKey::F10},
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_NONE, ui::DomKey::F10}},
      // F11 -> F11
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_COMMAND_DOWN, ui::DomKey::F11},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11}},
      // Ctrl + Launch App 1 (Display toggle) -> Unchanged
      // Search + Ctrl + Launch App 1 (Display toggle) -> F12
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::F12, ui::EF_CONTROL_DOWN,
        ui::DomKey::F12},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::F12, ui::EF_CONTROL_DOWN,
        ui::DomKey::F12}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::F12,
        ui::EF_COMMAND_DOWN + ui::EF_CONTROL_DOWN, ui::DomKey::F12},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_NONE, ui::DomKey::F12}}};

  for (const auto& test : tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestTopRowAsFnKeysForKeyboardLayout3) {
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());
  rewriter_->KeyboardDeviceAddedForTesting(
      kKeyboardDeviceId, "PC Keyboard",
      ui::EventRewriterChromeOS::kKbdTopRowLayoutWilco);

  // Enable preference treat-top-row-as-function-keys.
  // That causes action keys to be mapped back to Fn keys, unless the search
  // key is pressed.
  BooleanPrefMember top_row_as_fn_key;
  top_row_as_fn_key.Init(prefs::kLanguageSendFunctionKeys, prefs());
  top_row_as_fn_key.SetValue(true);

  KeyTestCase tests[] = {
      // Back -> F1, Search + Back -> Back
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BROWSER_BACK, ui::DomCode::BROWSER_BACK, ui::EF_NONE,
        ui::DomKey::BROWSER_BACK},
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_NONE, ui::DomKey::F1}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BROWSER_BACK, ui::DomCode::BROWSER_BACK, ui::EF_COMMAND_DOWN,
        ui::DomKey::BROWSER_BACK},
       {ui::VKEY_BROWSER_BACK, ui::DomCode::BROWSER_BACK, ui::EF_NONE,
        ui::DomKey::BROWSER_BACK}},
      // Refresh -> F2, Search + Refresh -> Refresh
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BROWSER_REFRESH, ui::DomCode::BROWSER_REFRESH, ui::EF_NONE,
        ui::DomKey::BROWSER_REFRESH},
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_NONE, ui::DomKey::F2}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BROWSER_REFRESH, ui::DomCode::BROWSER_REFRESH,
        ui::EF_COMMAND_DOWN, ui::DomKey::BROWSER_REFRESH},
       {ui::VKEY_BROWSER_REFRESH, ui::DomCode::BROWSER_REFRESH, ui::EF_NONE,
        ui::DomKey::BROWSER_REFRESH}},
      // Full Screen -> F3, Search + Full Screen -> Full Screen
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_ZOOM, ui::DomCode::ZOOM_TOGGLE, ui::EF_NONE,
        ui::DomKey::ZOOM_TOGGLE},
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_NONE, ui::DomKey::F3}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_ZOOM, ui::DomCode::ZOOM_TOGGLE, ui::EF_COMMAND_DOWN,
        ui::DomKey::ZOOM_TOGGLE},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::ZOOM_TOGGLE, ui::EF_NONE,
        ui::DomKey::ZOOM_TOGGLE}},
      // Launch App 1 -> F4, Search + Launch App 1 -> Launch App 1
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::DomCode::F4, ui::EF_NONE,
        ui::DomKey::F4},
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_NONE, ui::DomKey::F4}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::DomCode::F4, ui::EF_COMMAND_DOWN,
        ui::DomKey::F4},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::DomCode::F4, ui::EF_NONE,
        ui::DomKey::F4}},
      // Brightness down -> F5, Search Brightness Down -> Brightness Down
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BRIGHTNESS_DOWN, ui::DomCode::BRIGHTNESS_DOWN, ui::EF_NONE,
        ui::DomKey::BRIGHTNESS_DOWN},
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_NONE, ui::DomKey::F5}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BRIGHTNESS_DOWN, ui::DomCode::BRIGHTNESS_DOWN,
        ui::EF_COMMAND_DOWN, ui::DomKey::BRIGHTNESS_DOWN},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::DomCode::BRIGHTNESS_DOWN, ui::EF_NONE,
        ui::DomKey::BRIGHTNESS_DOWN}},
      // Brightness up -> F6, Search + Brightness Up -> Brightness Up
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BRIGHTNESS_UP, ui::DomCode::BRIGHTNESS_UP, ui::EF_NONE,
        ui::DomKey::BRIGHTNESS_UP},
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_NONE, ui::DomKey::F6}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BRIGHTNESS_UP, ui::DomCode::BRIGHTNESS_UP, ui::EF_COMMAND_DOWN,
        ui::DomKey::BRIGHTNESS_UP},
       {ui::VKEY_BRIGHTNESS_UP, ui::DomCode::BRIGHTNESS_UP, ui::EF_NONE,
        ui::DomKey::BRIGHTNESS_UP}},
      // Volume mute -> F7, Search + Volume Mute -> Volume Mute
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_VOLUME_MUTE, ui::DomCode::VOLUME_MUTE, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_MUTE},
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_NONE, ui::DomKey::F7}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_VOLUME_MUTE, ui::DomCode::VOLUME_MUTE, ui::EF_COMMAND_DOWN,
        ui::DomKey::AUDIO_VOLUME_MUTE},
       {ui::VKEY_VOLUME_MUTE, ui::DomCode::VOLUME_MUTE, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_MUTE}},
      // Volume Down -> F8, Search + Volume Down -> Volume Down
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_VOLUME_DOWN, ui::DomCode::VOLUME_DOWN, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_DOWN},
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_NONE, ui::DomKey::F8}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_VOLUME_DOWN, ui::DomCode::VOLUME_DOWN, ui::EF_COMMAND_DOWN,
        ui::DomKey::AUDIO_VOLUME_DOWN},
       {ui::VKEY_VOLUME_DOWN, ui::DomCode::VOLUME_DOWN, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_DOWN}},
      // Volume Up -> F9, Search + Volume Up -> Volume Up
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_VOLUME_UP, ui::DomCode::VOLUME_UP, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_UP},
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_NONE, ui::DomKey::F9}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_VOLUME_UP, ui::DomCode::VOLUME_UP, ui::EF_COMMAND_DOWN,
        ui::DomKey::AUDIO_VOLUME_UP},
       {ui::VKEY_VOLUME_UP, ui::DomCode::VOLUME_UP, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_UP}},
      // F10 -> F10
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_NONE, ui::DomKey::F10},
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_NONE, ui::DomKey::F10}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_COMMAND_DOWN, ui::DomKey::F10},
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_NONE, ui::DomKey::F10}},
      // F11 -> F11
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_COMMAND_DOWN, ui::DomKey::F11},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11}},
      // Ctrl + Launch App 1 (Display toggle) -> F12
      // Search + Ctrl + Launch App 1 (Display toggle) -> Unchanged
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::F12, ui::EF_CONTROL_DOWN,
        ui::DomKey::F12},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_NONE, ui::DomKey::F12}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::F12,
        ui::EF_COMMAND_DOWN + ui::EF_CONTROL_DOWN, ui::DomKey::F12},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::F12, ui::EF_CONTROL_DOWN,
        ui::DomKey::F12}}};

  for (const auto& test : tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestRewriteFunctionKeysInvalidLayout) {
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());

  // Not adding a keyboard simulates a failure in getting top row layout, which
  // will fallback to Layout1 in which case keys are rewritten to their default
  // values.
  KeyTestCase invalid_layout_tests[] = {
      // F2 -> Forward
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_NONE, ui::DomKey::F2},
       {ui::VKEY_BROWSER_FORWARD, ui::DomCode::BROWSER_FORWARD, ui::EF_NONE,
        ui::DomKey::BROWSER_FORWARD}},
      // F3 -> Refresh
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_NONE, ui::DomKey::F3},
       {ui::VKEY_BROWSER_REFRESH, ui::DomCode::BROWSER_REFRESH, ui::EF_NONE,
        ui::DomKey::BROWSER_REFRESH}},
      // F4 -> Launch App 2
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_NONE, ui::DomKey::F4},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::ZOOM_TOGGLE, ui::EF_NONE,
        ui::DomKey::ZOOM_TOGGLE}},
      // F7 -> Brightness up
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_NONE, ui::DomKey::F7},
       {ui::VKEY_BRIGHTNESS_UP, ui::DomCode::BRIGHTNESS_UP, ui::EF_NONE,
        ui::DomKey::BRIGHTNESS_UP}}};

  for (const auto& test : invalid_layout_tests)
    CheckKeyTestCase(rewriter_, test);

  // Adding a keyboard with a valid layout will take effect.
  rewriter_->KeyboardDeviceAddedForTesting(
      kKeyboardDeviceId, "PC Keyboard",
      ui::EventRewriterChromeOS::kKbdTopRowLayout2);
  KeyTestCase layout2_tests[] = {
      // F2 -> Refresh
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_NONE, ui::DomKey::F2},
       {ui::VKEY_BROWSER_REFRESH, ui::DomCode::BROWSER_REFRESH, ui::EF_NONE,
        ui::DomKey::BROWSER_REFRESH}},
      // F3 -> Launch App 2
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_NONE, ui::DomKey::F3},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::ZOOM_TOGGLE, ui::EF_NONE,
        ui::DomKey::ZOOM_TOGGLE}},
      // F4 -> Launch App 1
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_NONE, ui::DomKey::F4},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::DomCode::SELECT_TASK, ui::EF_NONE,
        ui::DomKey::LAUNCH_MY_COMPUTER}},
      // F7 -> Media Play/Pause
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_NONE, ui::DomKey::F7},
       {ui::VKEY_MEDIA_PLAY_PAUSE, ui::DomCode::MEDIA_PLAY_PAUSE, ui::EF_NONE,
        ui::DomKey::MEDIA_PLAY_PAUSE}}};

  for (const auto& test : layout2_tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestRewriteExtendedKeysWithSearchRemapped) {
  // Remap Search to Control.
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember search;
  InitModifierKeyPref(&search, prefs::kLanguageRemapSearchKeyTo,
                      ui::chromeos::ModifierKey::kControlKey);

  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");

  KeyTestCase tests[] = {
      // Alt+Search+Down -> End
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::DomCode::ARROW_DOWN,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, ui::DomKey::ARROW_DOWN},
       {ui::VKEY_END, ui::DomCode::END, ui::EF_NONE, ui::DomKey::END}},

      // Shift+Alt+Search+Down -> Shift+End
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::DomCode::ARROW_DOWN,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::ARROW_DOWN},
       {ui::VKEY_END, ui::DomCode::END, ui::EF_SHIFT_DOWN, ui::DomKey::END}},
  };

  for (const auto& test : tests)
    CheckKeyTestCase(rewriter_, test);
}

TEST_F(EventRewriterTest, TestRewriteKeyEventSentByXSendEvent) {
  // Remap Control to Alt.
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember control;
  InitModifierKeyPref(&control, prefs::kLanguageRemapControlKeyTo,
                      ui::chromeos::ModifierKey::kAltKey);

  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");

  // Send left control press.
  {
    ui::KeyEvent keyevent(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL,
                          ui::DomCode::CONTROL_LEFT, ui::EF_FINAL,
                          ui::DomKey::CONTROL, ui::EventTimeForNow());
    std::unique_ptr<ui::Event> new_event;
    // Control should NOT be remapped to Alt if EF_FINAL is set.
    EXPECT_EQ(ui::EVENT_REWRITE_CONTINUE,
              rewriter_->RewriteEvent(keyevent, &new_event));
    EXPECT_FALSE(new_event);
  }
}

TEST_F(EventRewriterTest, TestRewriteNonNativeEvent) {
  // Remap Control to Alt.
  chromeos::Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember control;
  InitModifierKeyPref(&control, prefs::kLanguageRemapControlKeyTo,
                      ui::chromeos::ModifierKey::kAltKey);

  rewriter_->KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");

  const int kTouchId = 2;
  gfx::Point location(0, 0);
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, location, base::TimeTicks(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  press.set_flags(ui::EF_CONTROL_DOWN);

  std::unique_ptr<ui::Event> new_event;
  rewriter_->RewriteEvent(press, &new_event);
  EXPECT_TRUE(new_event);
  // Control should be remapped to Alt.
  EXPECT_EQ(ui::EF_ALT_DOWN,
            new_event->flags() & (ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN));
}

// Keeps a buffer of handled events.
class EventBuffer : public ui::test::TestEventProcessor {
 public:
  EventBuffer() {}
  ~EventBuffer() override {}

  void PopEvents(std::vector<std::unique_ptr<ui::Event>>* events) {
    events->clear();
    events->swap(events_);
  }

 private:
  // ui::EventSink overrides:
  ui::EventDispatchDetails OnEventFromSource(ui::Event* event) override {
    events_.push_back(ui::Event::Clone(*event));
    return ui::EventDispatchDetails();
  }

  std::vector<std::unique_ptr<ui::Event>> events_;

  DISALLOW_COPY_AND_ASSIGN(EventBuffer);
};

// Trivial EventSource that does nothing but send events.
class TestEventSource : public ui::EventSource {
 public:
  explicit TestEventSource(ui::EventProcessor* processor)
      : processor_(processor) {}
  ui::EventSink* GetEventSink() override { return processor_; }
  ui::EventDispatchDetails Send(ui::Event* event) {
    return SendEventToSink(event);
  }

 private:
  ui::EventProcessor* processor_;
};

// Tests of event rewriting that depend on the Ash window manager.
class EventRewriterAshTest : public ChromeAshTestBase {
 public:
  EventRewriterAshTest()
      : source_(&buffer_),
        fake_user_manager_(new chromeos::FakeChromeUserManager),
        user_manager_enabler_(base::WrapUnique(fake_user_manager_)) {}
  ~EventRewriterAshTest() override {}

  bool RewriteFunctionKeys(const ui::Event& event,
                           std::unique_ptr<ui::Event>* rewritten_event) {
    return rewriter_->RewriteEvent(event, rewritten_event);
  }

  ui::EventDispatchDetails Send(ui::Event* event) {
    return source_.Send(event);
  }

  void SendKeyEvent(ui::EventType type,
                    ui::KeyboardCode key_code,
                    ui::DomCode code,
                    ui::DomKey key) {
    SendKeyEvent(type, key_code, code, key, ui::EF_NONE);
  }

  void SendKeyEvent(ui::EventType type,
                    ui::KeyboardCode key_code,
                    ui::DomCode code,
                    ui::DomKey key,
                    int flags) {
    ui::KeyEvent press(type, key_code, code, flags, key, ui::EventTimeForNow());
    ui::EventDispatchDetails details = Send(&press);
    CHECK(!details.dispatcher_destroyed);
  }

  void SendActivateStickyKeyPattern(ui::KeyboardCode key_code,
                                    ui::DomCode code,
                                    ui::DomKey key) {
    SendKeyEvent(ui::ET_KEY_PRESSED, key_code, code, key);
    SendKeyEvent(ui::ET_KEY_RELEASED, key_code, code, key);
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

  void InitModifierKeyPref(IntegerPrefMember* int_pref,
                           const std::string& pref_name,
                           ui::chromeos::ModifierKey modifierKey) {
    int_pref->Init(pref_name, prefs());
    int_pref->SetValue(static_cast<int>(modifierKey));
  }

  void PopEvents(std::vector<std::unique_ptr<ui::Event>>* events) {
    buffer_.PopEvents(events);
  }

  void SetUp() override {
    ChromeAshTestBase::SetUp();
    sticky_keys_controller_ = ash::Shell::Get()->sticky_keys_controller();
    delegate_ = std::make_unique<EventRewriterDelegateImpl>(nullptr);
    delegate_->set_pref_service_for_testing(prefs());
    rewriter_ = std::make_unique<ui::EventRewriterChromeOS>(
        delegate_.get(), sticky_keys_controller_);
    chromeos::Preferences::RegisterProfilePrefs(prefs_.registry());
    source_.AddEventRewriter(rewriter_.get());
    sticky_keys_controller_->Enable(true);
  }

  void TearDown() override {
    rewriter_.reset();
    ChromeAshTestBase::TearDown();
  }

 protected:
  ash::StickyKeysController* sticky_keys_controller_;

 private:
  std::unique_ptr<EventRewriterDelegateImpl> delegate_;
  std::unique_ptr<ui::EventRewriterChromeOS> rewriter_;

  EventBuffer buffer_;
  TestEventSource source_;

  chromeos::FakeChromeUserManager* fake_user_manager_;  // Not owned.
  user_manager::ScopedUserManager user_manager_enabler_;
  sync_preferences::TestingPrefServiceSyncable prefs_;

  DISALLOW_COPY_AND_ASSIGN(EventRewriterAshTest);
};

TEST_F(EventRewriterAshTest, TopRowKeysAreFunctionKeys) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(1));
  wm::ActivateWindow(window.get());
  std::vector<std::unique_ptr<ui::Event>> events;

  // Create a simulated keypress of F1 targetted at the window.
  ui::KeyEvent press_f1(ui::ET_KEY_PRESSED, ui::VKEY_F1, ui::DomCode::F1,
                        ui::EF_NONE, ui::DomKey::F1, ui::EventTimeForNow());

  // The event should also not be rewritten if the send-function-keys pref is
  // additionally set, for both apps v2 and regular windows.
  BooleanPrefMember send_function_keys_pref;
  send_function_keys_pref.Init(prefs::kLanguageSendFunctionKeys, prefs());
  send_function_keys_pref.SetValue(true);
  ui::EventDispatchDetails details = Send(&press_f1);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(
      GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_F1,
                                ui::DomCode::F1, ui::EF_NONE, ui::DomKey::F1),
      GetKeyEventAsString(*static_cast<ui::KeyEvent*>(events[0].get())));

  // If the pref isn't set when an event is sent to a regular window, F1 is
  // rewritten to the back key.
  send_function_keys_pref.SetValue(false);
  details = Send(&press_f1);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_BROWSER_BACK,
                                      ui::DomCode::BROWSER_BACK, ui::EF_NONE,
                                      ui::DomKey::BROWSER_BACK),
            GetKeyEventAsString(*static_cast<ui::KeyEvent*>(events[0].get())));
}

// Parameterized version of test with the same name that accepts the
// event flags that correspond to a right-click. This will be either
// Alt+Click or Search+Click. After a transition period this will
// default to Search+Click and the Alt+Click logic will be removed.
void EventRewriterTest::DontRewriteIfNotRewritten(int right_click_flags) {
  ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  std::vector<ui::InputDevice> touchpad_devices(2);
  constexpr int kTouchpadId1 = 10;
  constexpr int kTouchpadId2 = 11;
  touchpad_devices[0].id = kTouchpadId1;
  touchpad_devices[1].id = kTouchpadId2;
  static_cast<ui::DeviceHotplugEventObserver*>(device_data_manager)
      ->OnTouchpadDevicesUpdated(touchpad_devices);
  std::vector<ui::InputDevice> mouse_devices(1);
  constexpr int kMouseId = 12;
  touchpad_devices[0].id = kMouseId;
  static_cast<ui::DeviceHotplugEventObserver*>(device_data_manager)
      ->OnMouseDevicesUpdated(mouse_devices);

  // Test (Alt|Search) + Left click.
  {
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), right_click_flags,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kTouchpadId1);
    // Sanity check.
    EXPECT_EQ(ui::ET_MOUSE_PRESSED, press.type());
    EXPECT_EQ(right_click_flags, press.flags());
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result = RewriteMouseButtonEvent(press, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_NE(right_click_flags, right_click_flags & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), right_click_flags,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(kTouchpadId1);
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result = RewriteMouseButtonEvent(release, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_NE(right_click_flags, right_click_flags & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }

  // No (ALT|SEARCH) in first click.
  {
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kTouchpadId1);
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result = RewriteMouseButtonEvent(press, &new_event);
    EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), right_click_flags,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(kTouchpadId1);
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result = RewriteMouseButtonEvent(release, &new_event);
    EXPECT_EQ(right_click_flags, right_click_flags & result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }

  // ALT on different device.
  {
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), right_click_flags,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kTouchpadId2);
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result = RewriteMouseButtonEvent(press, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_NE(right_click_flags, right_click_flags & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), right_click_flags,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(kTouchpadId1);
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result = RewriteMouseButtonEvent(release, &new_event);
    EXPECT_EQ(right_click_flags, right_click_flags & result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), right_click_flags,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(kTouchpadId2);
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result = RewriteMouseButtonEvent(release, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_NE(right_click_flags, right_click_flags & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }

  // No rewrite for non-touchpad devices.
  {
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), right_click_flags,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kMouseId);
    EXPECT_EQ(ui::ET_MOUSE_PRESSED, press.type());
    EXPECT_EQ(right_click_flags, press.flags());
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result = RewriteMouseButtonEvent(press, &new_event);
    EXPECT_EQ(right_click_flags, right_click_flags & result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), right_click_flags,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(kMouseId);
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result = RewriteMouseButtonEvent(release, &new_event);
    EXPECT_EQ(right_click_flags, right_click_flags & result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
}

TEST_F(EventRewriterTest, DontRewriteIfNotRewritten_AltClickIsRightClick) {
  DontRewriteIfNotRewritten(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN);
}

TEST_F(EventRewriterTest, DontRewriteIfNotRewritten_SearchClickIsRightClick) {
  scoped_feature_list_.InitAndEnableFeature(
      chromeos::features::kUseSearchClickForRightClick);
  DontRewriteIfNotRewritten(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_COMMAND_DOWN);
}

TEST_F(EventRewriterAshTest, StickyKeyEventDispatchImpl) {
  // Test the actual key event dispatch implementation.
  std::vector<std::unique_ptr<ui::Event>> events;

  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(ui::ET_KEY_PRESSED, events[0]->type());
  EXPECT_EQ(ui::VKEY_CONTROL,
            static_cast<ui::KeyEvent*>(events[0].get())->key_code());

  // Test key press event is correctly modified and modifier release
  // event is sent.
  ui::KeyEvent press(ui::ET_KEY_PRESSED, ui::VKEY_C, ui::DomCode::US_C,
                     ui::EF_NONE, ui::DomKey::Constant<'c'>::Character,
                     ui::EventTimeForNow());
  ui::EventDispatchDetails details = Send(&press);
  PopEvents(&events);
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(ui::ET_KEY_PRESSED, events[0]->type());
  EXPECT_EQ(ui::VKEY_C,
            static_cast<ui::KeyEvent*>(events[0].get())->key_code());
  EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(ui::ET_KEY_RELEASED, events[1]->type());
  EXPECT_EQ(ui::VKEY_CONTROL,
            static_cast<ui::KeyEvent*>(events[1].get())->key_code());

  // Test key release event is not modified.
  ui::KeyEvent release(ui::ET_KEY_RELEASED, ui::VKEY_C, ui::DomCode::US_C,
                       ui::EF_NONE, ui::DomKey::Constant<'c'>::Character,
                       ui::EventTimeForNow());
  details = Send(&release);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(ui::ET_KEY_RELEASED, events[0]->type());
  EXPECT_EQ(ui::VKEY_C,
            static_cast<ui::KeyEvent*>(events[0].get())->key_code());
  EXPECT_FALSE(events[0]->flags() & ui::EF_CONTROL_DOWN);
}

TEST_F(EventRewriterAshTest, MouseEventDispatchImpl) {
  std::vector<std::unique_ptr<ui::Event>> events;

  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  PopEvents(&events);

  // Test mouse press event is correctly modified.
  gfx::Point location(0, 0);
  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, location, location,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  ui::EventDispatchDetails details = Send(&press);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, events[0]->type());
  EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);

  // Test mouse release event is correctly modified and modifier release
  // event is sent. The mouse event should have the correct DIP location.
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, location, location,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  details = Send(&release);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, events[0]->type());
  EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(ui::ET_KEY_RELEASED, events[1]->type());
  EXPECT_EQ(ui::VKEY_CONTROL,
            static_cast<ui::KeyEvent*>(events[1].get())->key_code());
}

TEST_F(EventRewriterAshTest, MouseWheelEventDispatchImpl) {
  std::vector<std::unique_ptr<ui::Event>> events;

  // Test positive mouse wheel event is correctly modified and modifier release
  // event is sent.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  PopEvents(&events);
  gfx::Point location(0, 0);
  ui::MouseWheelEvent positive(
      gfx::Vector2d(0, ui::MouseWheelEvent::kWheelDelta), location, location,
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
      ui::EF_LEFT_MOUSE_BUTTON);
  ui::EventDispatchDetails details = Send(&positive);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(2u, events.size());
  EXPECT_TRUE(events[0]->IsMouseWheelEvent());
  EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(ui::ET_KEY_RELEASED, events[1]->type());
  EXPECT_EQ(ui::VKEY_CONTROL,
            static_cast<ui::KeyEvent*>(events[1].get())->key_code());

  // Test negative mouse wheel event is correctly modified and modifier release
  // event is sent.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  PopEvents(&events);
  ui::MouseWheelEvent negative(
      gfx::Vector2d(0, -ui::MouseWheelEvent::kWheelDelta), location, location,
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
      ui::EF_LEFT_MOUSE_BUTTON);
  details = Send(&negative);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(2u, events.size());
  EXPECT_TRUE(events[0]->IsMouseWheelEvent());
  EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(ui::ET_KEY_RELEASED, events[1]->type());
  EXPECT_EQ(ui::VKEY_CONTROL,
            static_cast<ui::KeyEvent*>(events[1].get())->key_code());
}

// Tests that if modifier keys are remapped, the flags of a mouse wheel event
// will be rewritten properly.
TEST_F(EventRewriterAshTest, MouseWheelEventModifiersRewritten) {
  // Remap Control to Alt.
  IntegerPrefMember control;
  InitModifierKeyPref(&control, prefs::kLanguageRemapControlKeyTo,
                      ui::chromeos::ModifierKey::kAltKey);

  // Generate a mouse wheel event that has a CONTROL_DOWN modifier flag and
  // expect that it will be rewritten to ALT_DOWN.
  std::vector<std::unique_ptr<ui::Event>> events;
  gfx::Point location(0, 0);
  ui::MouseWheelEvent positive(
      gfx::Vector2d(0, ui::MouseWheelEvent::kWheelDelta), location, location,
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON | ui::EF_CONTROL_DOWN,
      ui::EF_LEFT_MOUSE_BUTTON);
  ui::EventDispatchDetails details = Send(&positive);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_TRUE(events[0]->IsMouseWheelEvent());
  EXPECT_FALSE(events[0]->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(events[0]->flags() & ui::EF_ALT_DOWN);
}

// Tests edge cases of key event rewriting (see https://crbug.com/913209).
TEST_F(EventRewriterAshTest, KeyEventRewritingEdgeCases) {
  std::vector<std::unique_ptr<ui::Event>> events;

  // Edge case 1: Press the Launcher button first. Then press the Up Arrow
  // button.
  SendKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_COMMAND, ui::DomCode::META_LEFT,
               ui::DomKey::META);
  SendKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_UP, ui::DomCode::ARROW_UP,
               ui::DomKey::ARROW_UP, ui::EF_COMMAND_DOWN);

  PopEvents(&events);
  EXPECT_EQ(2u, events.size());
  events.clear();

  SendKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_COMMAND, ui::DomCode::META_LEFT,
               ui::DomKey::META);
  PopEvents(&events);

  // When releasing the Launcher button, the rewritten event should be released
  // as well.
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(ui::VKEY_COMMAND,
            static_cast<ui::KeyEvent*>(events[0].get())->key_code());
  EXPECT_EQ(ui::VKEY_PRIOR,
            static_cast<ui::KeyEvent*>(events[1].get())->key_code());

  events.clear();

  // Edge case 2: Press the Up Arrow button first. Then press the Launch button.
  SendKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_UP, ui::DomCode::ARROW_UP,
               ui::DomKey::ARROW_UP);
  SendKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_COMMAND, ui::DomCode::META_LEFT,
               ui::DomKey::META);

  PopEvents(&events);
  EXPECT_EQ(2u, events.size());
  events.clear();

  SendKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_UP, ui::DomCode::ARROW_UP,
               ui::DomKey::ARROW_UP, ui::EF_COMMAND_DOWN);
  PopEvents(&events);

  // When releasing the Up Arrow button, the rewritten event should be blocked.
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(ui::VKEY_UP,
            static_cast<ui::KeyEvent*>(events[0].get())->key_code());
}

class StickyKeysOverlayTest : public EventRewriterAshTest {
 public:
  StickyKeysOverlayTest() : overlay_(NULL) {}

  ~StickyKeysOverlayTest() override {}

  void SetUp() override {
    EventRewriterAshTest::SetUp();
    overlay_ = sticky_keys_controller_->GetOverlayForTest();
    ASSERT_TRUE(overlay_);
  }

  ash::StickyKeysOverlay* overlay_;
};

TEST_F(StickyKeysOverlayTest, OneModifierEnabled) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing modifier key should show overlay.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing a normal key should hide overlay.
  SendActivateStickyKeyPattern(ui::VKEY_T, ui::DomCode::US_T,
                               ui::DomKey::Constant<'t'>::Character);
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
}

TEST_F(StickyKeysOverlayTest, TwoModifiersEnabled) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));

  // Pressing two modifiers should show overlay.
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT,
                               ui::DomKey::SHIFT);
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing a normal key should hide overlay.
  SendActivateStickyKeyPattern(ui::VKEY_N, ui::DomCode::US_N,
                               ui::DomKey::Constant<'n'>::Character);
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
}

TEST_F(StickyKeysOverlayTest, LockedModifier) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));

  // Pressing a modifier key twice should lock modifier and show overlay.
  SendActivateStickyKeyPattern(ui::VKEY_LMENU, ui::DomCode::ALT_LEFT,
                               ui::DomKey::ALT);
  SendActivateStickyKeyPattern(ui::VKEY_LMENU, ui::DomCode::ALT_LEFT,
                               ui::DomKey::ALT);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));

  // Pressing a normal key should not hide overlay.
  SendActivateStickyKeyPattern(ui::VKEY_D, ui::DomCode::US_D,
                               ui::DomKey::Constant<'d'>::Character);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));
}

TEST_F(StickyKeysOverlayTest, LockedAndNormalModifier) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));

  // Pressing a modifier key twice should lock modifier and show overlay.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing another modifier key should still show overlay.
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT,
                               ui::DomKey::SHIFT);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));

  // Pressing a normal key should not hide overlay but disable normal modifier.
  SendActivateStickyKeyPattern(ui::VKEY_D, ui::DomCode::US_D,
                               ui::DomKey::Constant<'d'>::Character);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
}

TEST_F(StickyKeysOverlayTest, ModifiersDisabled) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_COMMAND_DOWN));

  // Enable modifiers.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT,
                               ui::DomKey::SHIFT);
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT,
                               ui::DomKey::SHIFT);
  SendActivateStickyKeyPattern(ui::VKEY_LMENU, ui::DomCode::ALT_LEFT,
                               ui::DomKey::ALT);
  SendActivateStickyKeyPattern(ui::VKEY_COMMAND, ui::DomCode::META_LEFT,
                               ui::DomKey::META);
  SendActivateStickyKeyPattern(ui::VKEY_COMMAND, ui::DomCode::META_LEFT,
                               ui::DomKey::META);

  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_COMMAND_DOWN));

  // Disable modifiers and overlay should be hidden.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT,
                               ui::DomKey::SHIFT);
  SendActivateStickyKeyPattern(ui::VKEY_LMENU, ui::DomCode::ALT_LEFT,
                               ui::DomKey::ALT);
  SendActivateStickyKeyPattern(ui::VKEY_LMENU, ui::DomCode::ALT_LEFT,
                               ui::DomKey::ALT);
  SendActivateStickyKeyPattern(ui::VKEY_COMMAND, ui::DomCode::META_LEFT,
                               ui::DomKey::META);

  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_COMMAND_DOWN));
}

TEST_F(StickyKeysOverlayTest, ModifierVisibility) {
  // All but AltGr and Mod3 should initially be visible.
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_CONTROL_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_SHIFT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_COMMAND_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));

  // Turn all modifiers on.
  sticky_keys_controller_->SetModifiersEnabled(true, true);
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_CONTROL_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_SHIFT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_COMMAND_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));

  // Turn off Mod3.
  sticky_keys_controller_->SetModifiersEnabled(false, true);
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));

  // Turn off AltGr.
  sticky_keys_controller_->SetModifiersEnabled(true, false);
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));

  // Turn off AltGr and Mod3.
  sticky_keys_controller_->SetModifiersEnabled(false, false);
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));
}

}  // namespace chromeos
