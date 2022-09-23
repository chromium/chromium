// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <vector>

#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_suite.h"
#include "ash/webui/diagnostics_ui/backend/input_data_provider.h"
#include "ash/webui/diagnostics_ui/backend/input_data_provider_keyboard.h"
#include "ash/webui/diagnostics_ui/mojom/input_data_provider.mojom-forward.h"
#include "ash/webui/diagnostics_ui/mojom/input_data_provider.mojom-shared.h"
#include "base/files/file_path.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/events/event_rewriter_chromeos.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"

namespace ash::diagnostics {

namespace {

constexpr uint32_t kEvdevId = 5;
constexpr char kFilePath[] = "/dev/input/event5";
constexpr mojom::ConnectionType kConnectionType =
    mojom::ConnectionType::kInternal;

enum VivaldiTopRowScanCode {
  kPreviousTrack = 0x90,
  kFullscreen = 0x91,
  kOverview = 0x92,
  kScreenshot = 0x93,
  kScreenBrightnessDown = 0x94,
  kScreenBrightnessUp = 0x95,
  kPrivacyScreenToggle = 0x96,
  kKeyboardBacklightDown = 0x97,
  kKeyboardBacklightUp = 0x98,
  kNextTrack = 0x99,
  kPlayPause = 0x9A,
  kMicrophoneMute = 0x9B,
  kKeyboardBacklightToggle = 0x9E,
  kVolumeMute = 0xA0,
  kVolumeDown = 0xAE,
  kVolumeUp = 0xB0,
  kForward = 0xE9,
  kBack = 0xEA,
  kRefresh = 0xE7,
};

ui::InputDevice InputDeviceFromCapabilities(
    int device_id,
    const ui::DeviceCapabilities& capabilities) {
  ui::EventDeviceInfo device_info = {};
  ui::CapabilitiesToDeviceInfo(capabilities, &device_info);

  const std::string sys_path =
      base::StringPrintf("/dev/input/event%d-%s", device_id, capabilities.path);

  return ui::InputDevice(device_id, device_info.device_type(),
                         device_info.name(), device_info.phys(),
                         base::FilePath(sys_path), device_info.vendor_id(),
                         device_info.product_id(), device_info.version());
}

// Creates MutableKeyState event for given FKey KeyCode
ui::EventRewriterChromeOS::MutableKeyState MakeMutableKeyStateForFKey(
    ui::KeyboardCode key_code) {
  EXPECT_LE(ui::KeyboardCode::VKEY_F1, key_code);
  EXPECT_GE(ui::KeyboardCode::VKEY_F15, key_code);

  const uint32_t diff = key_code - ui::VKEY_F1;
  return ui::EventRewriterChromeOS::MutableKeyState(
      /*input_flags=*/0,
      ui::DomCode((static_cast<uint32_t>(ui::DomCode::F1) + diff)),
      ui::DomKey(ui::DomKey::F1 + diff), key_code);
}

}  // namespace

class InputDataProviderKeyboardTest : public ash::AshTestBase {
 public:
  InputDataProviderKeyboardTest()
      : AshTestBase(content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {}

  InputDataProviderKeyboardTest(const InputDataProviderKeyboardTest&) = delete;
  InputDataProviderKeyboardTest& operator=(
      const InputDataProviderKeyboardTest&) = delete;
  ~InputDataProviderKeyboardTest() override = default;

  void SetUp() override {
    input_data_provider_keyboard_ =
        std::make_unique<InputDataProviderKeyboard>();

    InitInputDeviceInformation();

    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();

    chromeos::system::StatisticsProvider::SetTestProvider(
        &statistics_provider_);
  }

  void TearDown() override {
    AshTestBase::TearDown();
    keyboard_info_.reset();
  }

  void InitInputDeviceInformation() {
    device_information.evdev_id = kEvdevId;
    device_information.path = base::FilePath(kFilePath);
    device_information.connection_type = kConnectionType;

    ui::CapabilitiesToDeviceInfo(ui::kEveKeyboard,
                                 &device_information.event_device_info);

    device_information.input_device =
        InputDeviceFromCapabilities(kEvdevId, ui::kEveKeyboard);
  }

 protected:
  InputDeviceInformation device_information;
  std::unique_ptr<InputDataProviderKeyboard> input_data_provider_keyboard_;
  chromeos::system::FakeStatisticsProvider statistics_provider_;
  mojom::KeyboardInfoPtr keyboard_info_;
  InputDataProviderKeyboard::AuxData aux_data_;
};

class VivaldiKeyboardTestBase : public InputDataProviderKeyboardTest {
 public:
  void SetUp() override {
    InputDataProviderKeyboardTest::SetUp();

    device_information.keyboard_type =
        ui::EventRewriterChromeOS::kDeviceInternalKeyboard;
    device_information.keyboard_top_row_layout =
        ui::EventRewriterChromeOS::kKbdTopRowLayoutCustom;
  }

  void TearDown() override {
    InputDataProviderKeyboardTest::TearDown();
    keyboard_scan_code_map_.clear();
    top_row_keys_.clear();
  }

  void AddTopRowKey(VivaldiTopRowScanCode scancode,
                    ui::KeyboardCode key_code,
                    mojom::TopRowKey top_row_key) {
    keyboard_scan_code_map_[scancode] = MakeMutableKeyStateForFKey(key_code);
    top_row_keys_.push_back(top_row_key);
  }

  void PopulateCustomScanCodeSet1() {
    AddTopRowKey(VivaldiTopRowScanCode::kBack, ui::VKEY_F1,
                 mojom::TopRowKey::kBack);
    AddTopRowKey(VivaldiTopRowScanCode::kRefresh, ui::VKEY_F2,
                 mojom::TopRowKey::kRefresh);
    AddTopRowKey(VivaldiTopRowScanCode::kFullscreen, ui::VKEY_F3,
                 mojom::TopRowKey::kFullscreen);
    AddTopRowKey(VivaldiTopRowScanCode::kOverview, ui::VKEY_F4,
                 mojom::TopRowKey::kOverview);
    AddTopRowKey(VivaldiTopRowScanCode::kScreenshot, ui::VKEY_F5,
                 mojom::TopRowKey::kScreenshot);
    AddTopRowKey(VivaldiTopRowScanCode::kScreenBrightnessUp, ui::VKEY_F6,
                 mojom::TopRowKey::kScreenBrightnessUp);
    AddTopRowKey(VivaldiTopRowScanCode::kScreenBrightnessDown, ui::VKEY_F7,
                 mojom::TopRowKey::kScreenBrightnessDown);
    AddTopRowKey(VivaldiTopRowScanCode::kPrivacyScreenToggle, ui::VKEY_F8,
                 mojom::TopRowKey::kPrivacyScreenToggle);
    AddTopRowKey(VivaldiTopRowScanCode::kKeyboardBacklightUp, ui::VKEY_F9,
                 mojom::TopRowKey::kKeyboardBacklightUp);
    AddTopRowKey(VivaldiTopRowScanCode::kKeyboardBacklightDown, ui::VKEY_F10,
                 mojom::TopRowKey::kKeyboardBacklightDown);
    AddTopRowKey(VivaldiTopRowScanCode::kKeyboardBacklightToggle, ui::VKEY_F11,
                 mojom::TopRowKey::kKeyboardBacklightToggle);
    AddTopRowKey(VivaldiTopRowScanCode::kNextTrack, ui::VKEY_F12,
                 mojom::TopRowKey::kNextTrack);
    AddTopRowKey(VivaldiTopRowScanCode::kPreviousTrack, ui::VKEY_F13,
                 mojom::TopRowKey::kPreviousTrack);
    AddTopRowKey(VivaldiTopRowScanCode::kPlayPause, ui::VKEY_F14,
                 mojom::TopRowKey::kPlayPause);
    AddTopRowKey(VivaldiTopRowScanCode::kMicrophoneMute, ui::VKEY_F15,
                 mojom::TopRowKey::kMicrophoneMute);

    device_information.keyboard_scan_code_map = keyboard_scan_code_map_;
  }

  void PopulateCustomScanCodeSet2() {
    AddTopRowKey(VivaldiTopRowScanCode::kVolumeMute, ui::VKEY_F1,
                 mojom::TopRowKey::kVolumeMute);
    AddTopRowKey(VivaldiTopRowScanCode::kVolumeDown, ui::VKEY_F2,
                 mojom::TopRowKey::kVolumeDown);
    AddTopRowKey(VivaldiTopRowScanCode::kVolumeUp, ui::VKEY_F3,
                 mojom::TopRowKey::kVolumeUp);
    AddTopRowKey(VivaldiTopRowScanCode::kForward, ui::VKEY_F4,
                 mojom::TopRowKey::kForward);

    device_information.keyboard_scan_code_map = keyboard_scan_code_map_;
  }

 protected:
  base::flat_map<uint32_t, ui::EventRewriterChromeOS::MutableKeyState>
      keyboard_scan_code_map_;
  std::vector<mojom::TopRowKey> top_row_keys_;
};

TEST_F(VivaldiKeyboardTestBase, ScanCodeIndexesSet1) {
  PopulateCustomScanCodeSet1();
  keyboard_info_ = input_data_provider_keyboard_->ConstructKeyboard(
      &device_information, &aux_data_);

  for (const auto& val : aux_data_.top_row_key_scancode_indexes) {
    const uint32_t scancode = val.first;
    const uint32_t index = val.second;

    EXPECT_EQ(static_cast<uint32_t>(keyboard_scan_code_map_[scancode].key_code -
                                    ui::VKEY_F1),
              index);
  }

  EXPECT_EQ(top_row_keys_, keyboard_info_->top_row_keys);
}

TEST_F(VivaldiKeyboardTestBase, ScanCodeIndexesSet2) {
  PopulateCustomScanCodeSet2();
  keyboard_info_ = input_data_provider_keyboard_->ConstructKeyboard(
      &device_information, &aux_data_);

  for (const auto& val : aux_data_.top_row_key_scancode_indexes) {
    const uint32_t scancode = val.first;
    const uint32_t index = val.second;

    EXPECT_EQ(static_cast<uint32_t>(keyboard_scan_code_map_[scancode].key_code -
                                    ui::VKEY_F1),
              index);
  }

  EXPECT_EQ(top_row_keys_, keyboard_info_->top_row_keys);
}

TEST_F(VivaldiKeyboardTestBase, ScanCodeIndexesWithZeroScanCodes) {
  // Populate a broken map where some FKeys are skipped. This happens as a
  // result of TK_ABSENT in the FW code definitions.
  // Example here:
  // https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/release-firmware/fpmcu-nami/board/taniks/keyboard.c;l=40-42;drc=64643a516e916d94e8956a4beb00df709a3efe21
  AddTopRowKey(VivaldiTopRowScanCode::kBack, ui::VKEY_F1,
               mojom::TopRowKey::kBack);
  AddTopRowKey(VivaldiTopRowScanCode::kRefresh, ui::VKEY_F2,
               mojom::TopRowKey::kRefresh);
  keyboard_scan_code_map_[0] = MakeMutableKeyStateForFKey(ui::VKEY_F5);
  AddTopRowKey(VivaldiTopRowScanCode::kFullscreen, ui::VKEY_F6,
               mojom::TopRowKey::kFullscreen);
  device_information.keyboard_scan_code_map = keyboard_scan_code_map_;

  keyboard_info_ = input_data_provider_keyboard_->ConstructKeyboard(
      &device_information, &aux_data_);

  // In the input map, we have Back, Refresh, and Fullscreen in the correct
  // order, but spaced out with a gap from F3-F5. This gap should be removed as
  // a gap in the input map implies the keys do not exist.
  EXPECT_EQ(0u, aux_data_.top_row_key_scancode_indexes[kBack]);
  EXPECT_EQ(1u, aux_data_.top_row_key_scancode_indexes[kRefresh]);
  EXPECT_EQ(2u, aux_data_.top_row_key_scancode_indexes[kFullscreen]);

  EXPECT_EQ(top_row_keys_, keyboard_info_->top_row_keys);
}

class MechanicalLayoutTest
    : public VivaldiKeyboardTestBase,
      public testing::WithParamInterface<
          std::tuple<std::string, mojom::MechanicalLayout>> {
  void SetUp() override {
    VivaldiKeyboardTestBase::SetUp();
    std::tie(layout_string_, expected_layout_) = GetParam();
  }

 protected:
  std::string layout_string_;
  mojom::MechanicalLayout expected_layout_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    MechanicalLayoutTest,
    testing::ValuesIn(
        std::vector<std::tuple<std::string, mojom::MechanicalLayout>>{
            {"ANSI", mojom::MechanicalLayout::kAnsi},
            {"ISO", mojom::MechanicalLayout::kIso},
            {"JIS", mojom::MechanicalLayout::kJis},
            {"JUNK_IDENTIFIER", mojom::MechanicalLayout::kUnknown}}));

TEST_P(MechanicalLayoutTest, CheckLayout) {
  statistics_provider_.SetMachineStatistic(
      chromeos::system::kKeyboardMechanicalLayoutKey, layout_string_);

  keyboard_info_ = input_data_provider_keyboard_->ConstructKeyboard(
      &device_information, &aux_data_);

  EXPECT_EQ(expected_layout_, keyboard_info_->mechanical_layout);
}

}  // namespace ash::diagnostics
