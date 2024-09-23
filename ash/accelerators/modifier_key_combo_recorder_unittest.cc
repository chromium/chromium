// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/modifier_key_combo_recorder.h"

#include "ash/public/cpp/accelerators_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"

namespace ash {

namespace {

using ModifierFlag = ModifierKeyComboRecorder::ModifierFlag;

}  // namespace

class ModifierKeyComboRecorderTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    modifier_key_combo_recorder_ = std::make_unique<ModifierKeyComboRecorder>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    modifier_key_combo_recorder_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<ModifierKeyComboRecorder> modifier_key_combo_recorder_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(ModifierKeyComboRecorderTest, ModifierLocation) {
  ui::KeyboardDevice keyboard(1, ui::INPUT_DEVICE_INTERNAL, "Keyboard");
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({keyboard});

  ui::KeyboardCapability* keyboard_capability =
      Shell::Get()->keyboard_capability();
  ui::KeyboardCapability::KeyboardInfo keyboard_info;
  keyboard_info.device_type =
      ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
  keyboard_capability->DisableKeyboardInfoTrimmingForTesting();
  keyboard_capability->SetKeyboardInfoForTesting(keyboard,
                                                 std::move(keyboard_info));

  ui::KeyEvent control_event(ui::EventType::kKeyPressed, ui::VKEY_RCONTROL,
                             ui::DomCode::CONTROL_RIGHT, ui::EF_CONTROL_DOWN);
  control_event.set_source_device_id(keyboard.id);
  modifier_key_combo_recorder_->OnPrerewriteKeyInputEvent(control_event);
  histogram_tester_->ExpectBucketCount(
      "ChromeOS.Inputs.ModifierKeyCombo.Internal",
      static_cast<uint32_t>(AcceleratorKeyInputType::kControlRight), 1);

  ui::KeyEvent alpha_event(ui::EventType::kKeyPressed, ui::VKEY_Z,
                           ui::EF_CONTROL_DOWN);
  alpha_event.set_source_device_id(keyboard.id);
  modifier_key_combo_recorder_->OnPrerewriteKeyInputEvent(alpha_event);

  const uint32_t modifier_flag =
      1 << static_cast<uint32_t>(ModifierFlag::kControlRight);
  const uint32_t expected_hash =
      static_cast<uint32_t>(AcceleratorKeyInputType::kAlpha) +
      (modifier_flag << 16);
  histogram_tester_->ExpectBucketCount(
      "ChromeOS.Inputs.ModifierKeyCombo.Internal", expected_hash, 1);
}

TEST_F(ModifierKeyComboRecorderTest, AltGrModifier) {
  ui::KeyboardDevice keyboard(1, ui::INPUT_DEVICE_INTERNAL, "Keyboard");
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({keyboard});

  ui::KeyboardCapability* keyboard_capability =
      Shell::Get()->keyboard_capability();
  ui::KeyboardCapability::KeyboardInfo keyboard_info;
  keyboard_info.device_type =
      ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
  keyboard_capability->DisableKeyboardInfoTrimmingForTesting();
  keyboard_capability->SetKeyboardInfoForTesting(keyboard,
                                                 std::move(keyboard_info));

  ui::KeyEvent altgr_event(ui::EventType::kKeyPressed, ui::VKEY_ALTGR,
                           ui::DomCode::ALT_RIGHT, ui::EF_ALTGR_DOWN);
  altgr_event.set_source_device_id(keyboard.id);
  modifier_key_combo_recorder_->OnPrerewriteKeyInputEvent(altgr_event);
  histogram_tester_->ExpectBucketCount(
      "ChromeOS.Inputs.ModifierKeyCombo.Internal",
      static_cast<uint32_t>(AcceleratorKeyInputType::kAltGr), 1);

  ui::KeyEvent alpha_event(ui::EventType::kKeyPressed, ui::VKEY_Z,
                           ui::EF_ALTGR_DOWN);
  alpha_event.set_source_device_id(keyboard.id);
  modifier_key_combo_recorder_->OnPrerewriteKeyInputEvent(alpha_event);

  const uint32_t modifier_flag = 1
                                 << static_cast<uint32_t>(ModifierFlag::kAltGr);
  const uint32_t expected_hash =
      static_cast<uint32_t>(AcceleratorKeyInputType::kAlpha) +
      (modifier_flag << 16);
  histogram_tester_->ExpectBucketCount(
      "ChromeOS.Inputs.ModifierKeyCombo.Internal", expected_hash, 1);
}

TEST_F(ModifierKeyComboRecorderTest, AlphaOrDigitKeysWithShift) {
  ui::KeyboardDevice keyboard(1, ui::INPUT_DEVICE_INTERNAL, "Keyboard");
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({keyboard});

  ui::KeyboardCapability* keyboard_capability =
      Shell::Get()->keyboard_capability();
  ui::KeyboardCapability::KeyboardInfo keyboard_info;
  keyboard_info.device_type =
      ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
  keyboard_capability->DisableKeyboardInfoTrimmingForTesting();
  keyboard_capability->SetKeyboardInfoForTesting(keyboard,
                                                 std::move(keyboard_info));

  ui::KeyEvent shift_c_event(ui::EventType::kKeyPressed, ui::VKEY_C,
                             ui::EF_SHIFT_DOWN);
  shift_c_event.set_source_device_id(keyboard.id);
  modifier_key_combo_recorder_->OnPrerewriteKeyInputEvent(shift_c_event);
  // No metric should be recorded if the input was an alpha key + shift.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.ModifierKeyCombo.Internal", 0);

  ui::KeyEvent shift_nine_event(ui::EventType::kKeyPressed, ui::VKEY_9,
                                ui::EF_SHIFT_DOWN);
  shift_nine_event.set_source_device_id(keyboard.id);
  modifier_key_combo_recorder_->OnPrerewriteKeyInputEvent(shift_nine_event);
  // No metric should be recorded if the input was a digit key + shift.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.ModifierKeyCombo.Internal", 0);

  ui::KeyEvent ctrl_shift_c_event(ui::EventType::kKeyPressed, ui::VKEY_C,
                                  ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
  ctrl_shift_c_event.set_source_device_id(keyboard.id);
  modifier_key_combo_recorder_->OnPrerewriteKeyInputEvent(ctrl_shift_c_event);
  // ShiftLeft and ControlLeft are the default locations for these modifiers.
  const uint32_t modifier_flag =
      1 << static_cast<uint32_t>(ModifierFlag::kShiftLeft) |
      1 << static_cast<uint32_t>(ModifierFlag::kControlLeft);
  const uint32_t expected_hash =
      static_cast<uint32_t>(AcceleratorKeyInputType::kAlpha) +
      (modifier_flag << 16);
  // Metric should be recorded if shift and another modifier are held.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.ModifierKeyCombo.Internal", 1);
  histogram_tester_->ExpectBucketCount(
      "ChromeOS.Inputs.ModifierKeyCombo.Internal", expected_hash, 1);
}

class ModifierKeyComboRecorderParameterizedTest
    : public ModifierKeyComboRecorderTest,
      public testing::WithParamInterface<std::tuple<ui::KeyEvent, uint32_t>> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    ModifierKeyComboRecorderParameterizedTest,
    testing::ValuesIn(std::vector<std::tuple<ui::KeyEvent, uint32_t>>{
        {ui::KeyEvent(ui::EventType::kKeyPressed,
                      ui::VKEY_NUMPAD0,
                      ui::EF_NONE),
         static_cast<uint32_t>(AcceleratorKeyInputType::kNumberPad)},
        {ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_HOME, ui::EF_NONE),
         static_cast<uint32_t>(AcceleratorKeyInputType::kSixPack)},
        {ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE, ui::EF_NONE),
         static_cast<uint32_t>(AcceleratorKeyInputType::kEscape)},
        {ui::KeyEvent(ui::EventType::kKeyPressed,
                      ui::VKEY_BROWSER_BACK,
                      ui::EF_NONE),
         static_cast<uint32_t>(AcceleratorKeyInputType::kTopRow)},
        {ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_BACK, ui::EF_NONE),
         static_cast<uint32_t>(AcceleratorKeyInputType::kBackspace)},
        {ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_UP, ui::EF_NONE),
         static_cast<uint32_t>(AcceleratorKeyInputType::kUpArrow)},
        {ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_TAB, ui::EF_NONE),
         static_cast<uint32_t>(AcceleratorKeyInputType::kTab)},
        {ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_ALTGR, ui::EF_NONE),
         static_cast<uint32_t>(AcceleratorKeyInputType::kAltGr)},
        {ui::KeyEvent(ui::EventType::kKeyPressed,
                      ui::VKEY_LWIN,
                      ui::DomCode::META_LEFT,
                      ui::EF_NONE),
         static_cast<uint32_t>(AcceleratorKeyInputType::kMetaLeft)},
        {ui::KeyEvent(ui::EventType::kKeyPressed,
                      ui::VKEY_RWIN,
                      ui::DomCode::META_RIGHT,
                      ui::EF_NONE),
         static_cast<uint32_t>(AcceleratorKeyInputType::kMetaRight)}}));

TEST_P(ModifierKeyComboRecorderParameterizedTest, InternalKeyboard) {
  auto [key_event, hash] = GetParam();

  ui::KeyboardDevice keyboard(1, ui::INPUT_DEVICE_INTERNAL, "Keyboard");
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({keyboard});

  ui::KeyboardCapability* keyboard_capability =
      Shell::Get()->keyboard_capability();
  ui::KeyboardCapability::KeyboardInfo keyboard_info;
  keyboard_info.device_type =
      ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
  keyboard_capability->DisableKeyboardInfoTrimmingForTesting();
  keyboard_capability->SetKeyboardInfoForTesting(keyboard,
                                                 std::move(keyboard_info));

  key_event.set_source_device_id(keyboard.id);
  modifier_key_combo_recorder_->OnPrerewriteKeyInputEvent(key_event);
  histogram_tester_->ExpectUniqueSample(
      "ChromeOS.Inputs.ModifierKeyCombo.Internal", hash, 1);
}

TEST_P(ModifierKeyComboRecorderParameterizedTest, ExternalKeyboard) {
  auto [key_event, hash] = GetParam();

  ui::KeyboardDevice keyboard(1, ui::INPUT_DEVICE_INTERNAL, "Keyboard");
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({keyboard});

  ui::KeyboardCapability* keyboard_capability =
      Shell::Get()->keyboard_capability();
  ui::KeyboardCapability::KeyboardInfo keyboard_info;
  keyboard_info.device_type =
      ui::KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard;
  keyboard_capability->SetKeyboardInfoForTesting(keyboard,
                                                 std::move(keyboard_info));

  key_event.set_source_device_id(keyboard.id);
  modifier_key_combo_recorder_->OnPrerewriteKeyInputEvent(key_event);
  histogram_tester_->ExpectUniqueSample(
      "ChromeOS.Inputs.ModifierKeyCombo.External", hash, 1);
}

TEST_P(ModifierKeyComboRecorderParameterizedTest, ExternalChromeOSKeyboard) {
  auto [key_event, hash] = GetParam();

  ui::KeyboardDevice keyboard(1, ui::INPUT_DEVICE_INTERNAL, "Keyboard");
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({keyboard});

  ui::KeyboardCapability* keyboard_capability =
      Shell::Get()->keyboard_capability();
  ui::KeyboardCapability::KeyboardInfo keyboard_info;
  keyboard_info.device_type =
      ui::KeyboardCapability::DeviceType::kDeviceExternalChromeOsKeyboard;
  keyboard_capability->SetKeyboardInfoForTesting(keyboard,
                                                 std::move(keyboard_info));

  key_event.set_source_device_id(keyboard.id);
  modifier_key_combo_recorder_->OnPrerewriteKeyInputEvent(key_event);
  histogram_tester_->ExpectUniqueSample(
      "ChromeOS.Inputs.ModifierKeyCombo.CrOSExternal", hash, 1);
}

}  // namespace ash
