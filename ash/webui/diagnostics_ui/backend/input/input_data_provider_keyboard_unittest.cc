// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input/input_data_provider_keyboard.h"

#include <algorithm>
#include <string_view>
#include <tuple>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/diagnostics_ui/backend/input/input_data_provider.h"
#include "ash/webui/diagnostics_ui/mojom/input_data_provider.mojom-forward.h"
#include "ash/webui/diagnostics_ui/mojom/input_data_provider.mojom-shared.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "content/public/test/browser_task_environment.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/ash/event_rewriter_ash.h"
#include "ui/events/ash/keyboard_capability.h"
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

// Value that marks a key is not in the top row when returned from
// `ConstructInputKeyEvent`
constexpr int kNotInTopRowValue = -1;

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

// xkb layout ids for both turkish Q-Type and F-Type layouts.
constexpr std::string_view kTurkishKeyboardLayoutId = "xkb:tr::tur";
constexpr std::string_view kTurkishKeyboardFLayoutId = "xkb:tr:f:tur";
constexpr std::string_view kTurkeyRegionCode = "tr";
constexpr std::string_view kTurkeyFLayoutRegionCode = "tr.f";

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

void SetCurrentImeId(const std::string& current_ime_id) {
  // Only data relavent is the ime id.
  ImeInfo ime_info;
  ime_info.id = current_ime_id;
  ime_info.name = u"English";
  ime_info.short_name = u"US";
  ime_info.third_party = false;

  Shell::Get()->ime_controller()->RefreshIme(
      current_ime_id, /*available_imes=*/{ime_info}, /*menu_items=*/{});
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

    InitInputDeviceInformation(ui::kEveKeyboard);

    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();

    system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    statistics_provider_.SetMachineStatistic(
        system::kKeyboardMechanicalLayoutKey, "ANSI");
    statistics_provider_.SetMachineStatistic(system::kRegionKey, "us");

    keyboard_info_ = input_data_provider_keyboard_->ConstructKeyboard(
        &device_information, &aux_data_);
  }

  void TearDown() override {
    AshTestBase::TearDown();
    keyboard_info_.reset();
  }

  void InitInputDeviceInformation(ui::DeviceCapabilities capabilities) {
    device_information.evdev_id = kEvdevId;
    device_information.path = base::FilePath(kFilePath);
    device_information.connection_type = kConnectionType;

    ui::CapabilitiesToDeviceInfo(capabilities,
                                 &device_information.event_device_info);
    device_information.keyboard_type =
        ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
    device_information.keyboard_top_row_layout =
        ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom;

    device_information.input_device =
        InputDeviceFromCapabilities(kEvdevId, capabilities);
  }

 protected:
  InputDeviceInformation device_information;
  std::unique_ptr<InputDataProviderKeyboard> input_data_provider_keyboard_;
  system::FakeStatisticsProvider statistics_provider_;
  mojom::KeyboardInfoPtr keyboard_info_;
  InputDataProviderKeyboard::AuxData aux_data_;
};

class VivaldiKeyboardTestBase : public InputDataProviderKeyboardTest {
 public:
  void SetUp() override {
    InputDataProviderKeyboardTest::SetUp();

    device_information.keyboard_type =
        ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
    device_information.keyboard_top_row_layout =
        ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom;
  }

  void TearDown() override {
    InputDataProviderKeyboardTest::TearDown();
    top_row_scan_codes_.clear();
    top_row_keys_.clear();
  }

  void AddTopRowKey(VivaldiTopRowScanCode scancode,
                    ui::KeyboardCode key_code,
                    mojom::TopRowKey top_row_key,
                    ui::TopRowActionKey action_key) {
    top_row_scan_codes_.push_back(scancode);
    top_row_keys_.push_back(top_row_key);
    action_keys.push_back(action_key);
  }

  void PopulateCustomScanCodeSet1() {
    AddTopRowKey(VivaldiTopRowScanCode::kBack, ui::VKEY_F1,
                 mojom::TopRowKey::kBack, ui::TopRowActionKey::kBack);
    AddTopRowKey(VivaldiTopRowScanCode::kRefresh, ui::VKEY_F2,
                 mojom::TopRowKey::kRefresh, ui::TopRowActionKey::kRefresh);
    AddTopRowKey(VivaldiTopRowScanCode::kFullscreen, ui::VKEY_F3,
                 mojom::TopRowKey::kFullscreen,
                 ui::TopRowActionKey::kFullscreen);
    AddTopRowKey(VivaldiTopRowScanCode::kOverview, ui::VKEY_F4,
                 mojom::TopRowKey::kOverview, ui::TopRowActionKey::kOverview);
    AddTopRowKey(VivaldiTopRowScanCode::kScreenshot, ui::VKEY_F5,
                 mojom::TopRowKey::kScreenshot,
                 ui::TopRowActionKey::kScreenshot);
    AddTopRowKey(VivaldiTopRowScanCode::kScreenBrightnessUp, ui::VKEY_F6,
                 mojom::TopRowKey::kScreenBrightnessUp,
                 ui::TopRowActionKey::kScreenBrightnessUp);
    AddTopRowKey(VivaldiTopRowScanCode::kScreenBrightnessDown, ui::VKEY_F7,
                 mojom::TopRowKey::kScreenBrightnessDown,
                 ui::TopRowActionKey::kScreenBrightnessDown);
    AddTopRowKey(VivaldiTopRowScanCode::kKeyboardBacklightUp, ui::VKEY_F8,
                 mojom::TopRowKey::kKeyboardBacklightUp,
                 ui::TopRowActionKey::kKeyboardBacklightUp);
    AddTopRowKey(VivaldiTopRowScanCode::kKeyboardBacklightDown, ui::VKEY_F9,
                 mojom::TopRowKey::kKeyboardBacklightDown,
                 ui::TopRowActionKey::kKeyboardBacklightDown);
    AddTopRowKey(VivaldiTopRowScanCode::kKeyboardBacklightToggle, ui::VKEY_F10,
                 mojom::TopRowKey::kKeyboardBacklightToggle,
                 ui::TopRowActionKey::kKeyboardBacklightToggle);
    AddTopRowKey(VivaldiTopRowScanCode::kNextTrack, ui::VKEY_F11,
                 mojom::TopRowKey::kNextTrack, ui::TopRowActionKey::kNextTrack);
    AddTopRowKey(VivaldiTopRowScanCode::kPreviousTrack, ui::VKEY_F12,
                 mojom::TopRowKey::kPreviousTrack,
                 ui::TopRowActionKey::kPreviousTrack);
    AddTopRowKey(VivaldiTopRowScanCode::kPlayPause, ui::VKEY_F13,
                 mojom::TopRowKey::kPlayPause, ui::TopRowActionKey::kPlayPause);
    AddTopRowKey(VivaldiTopRowScanCode::kMicrophoneMute, ui::VKEY_F14,
                 mojom::TopRowKey::kMicrophoneMute,
                 ui::TopRowActionKey::kMicrophoneMute);

    device_information.keyboard_scan_codes = top_row_scan_codes_;
  }

  void PopulateCustomScanCodeSet2() {
    AddTopRowKey(VivaldiTopRowScanCode::kVolumeMute, ui::VKEY_F1,
                 mojom::TopRowKey::kVolumeMute,
                 ui::TopRowActionKey::kVolumeMute);
    AddTopRowKey(VivaldiTopRowScanCode::kVolumeDown, ui::VKEY_F2,
                 mojom::TopRowKey::kVolumeDown,
                 ui::TopRowActionKey::kVolumeDown);
    AddTopRowKey(VivaldiTopRowScanCode::kVolumeUp, ui::VKEY_F3,
                 mojom::TopRowKey::kVolumeUp, ui::TopRowActionKey::kVolumeUp);
    AddTopRowKey(VivaldiTopRowScanCode::kForward, ui::VKEY_F4,
                 mojom::TopRowKey::kForward, ui::TopRowActionKey::kForward);

    device_information.keyboard_scan_codes = top_row_scan_codes_;
  }

 protected:
  std::vector<uint32_t> top_row_scan_codes_;
  std::vector<mojom::TopRowKey> top_row_keys_;
  std::vector<ui::TopRowActionKey> action_keys;
};

TEST_F(VivaldiKeyboardTestBase, ScanCodeIndexesSet1) {
  PopulateCustomScanCodeSet1();

  ui::KeyboardCapability::KeyboardInfo info;
  info.top_row_scan_codes = top_row_scan_codes_;
  info.top_row_action_keys = action_keys;
  info.device_type =
      ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
  info.top_row_layout =
      ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom;
  Shell::Get()->keyboard_capability()->SetKeyboardInfoForTesting(
      ui::KeyboardDevice(device_information.input_device), std::move(info));

  keyboard_info_ = input_data_provider_keyboard_->ConstructKeyboard(
      &device_information, &aux_data_);

  for (const auto& val : aux_data_.top_row_key_scancode_indexes) {
    const uint32_t scancode = val.first;
    const uint32_t index = val.second;
    EXPECT_EQ(top_row_scan_codes_[index], scancode);
  }

  EXPECT_EQ(top_row_keys_, keyboard_info_->top_row_keys);
}

TEST_F(VivaldiKeyboardTestBase, ScanCodeIndexesSet2) {
  PopulateCustomScanCodeSet2();

  ui::KeyboardCapability::KeyboardInfo info;
  info.top_row_scan_codes = top_row_scan_codes_;
  info.top_row_action_keys = action_keys;
  info.device_type =
      ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
  info.top_row_layout =
      ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom;
  Shell::Get()->keyboard_capability()->SetKeyboardInfoForTesting(
      ui::KeyboardDevice(device_information.input_device), std::move(info));

  keyboard_info_ = input_data_provider_keyboard_->ConstructKeyboard(
      &device_information, &aux_data_);

  for (const auto& val : aux_data_.top_row_key_scancode_indexes) {
    const uint32_t scancode = val.first;
    const uint32_t index = val.second;
    EXPECT_EQ(top_row_scan_codes_[index], scancode);
  }

  EXPECT_EQ(top_row_keys_, keyboard_info_->top_row_keys);
}

TEST_F(VivaldiKeyboardTestBase, ScanCodeIndexesWithZeroScanCodes) {
  // Populate a broken map where some FKeys are skipped. This happens as a
  // result of TK_ABSENT in the FW code definitions.
  // Example here:
  // https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/release-firmware/fpmcu-nami/board/taniks/keyboard.c;l=40-42;drc=64643a516e916d94e8956a4beb00df709a3efe21
  AddTopRowKey(VivaldiTopRowScanCode::kBack, ui::VKEY_F1,
               mojom::TopRowKey::kBack, ui::TopRowActionKey::kBack);
  AddTopRowKey(VivaldiTopRowScanCode::kRefresh, ui::VKEY_F2,
               mojom::TopRowKey::kRefresh, ui::TopRowActionKey::kRefresh);
  // Populate with TK_ABSENT for F3-F5
  top_row_scan_codes_.push_back(0);
  action_keys.push_back(ui::TopRowActionKey::kNone);
  top_row_scan_codes_.push_back(0);
  action_keys.push_back(ui::TopRowActionKey::kNone);
  top_row_scan_codes_.push_back(0);
  action_keys.push_back(ui::TopRowActionKey::kNone);
  AddTopRowKey(VivaldiTopRowScanCode::kFullscreen, ui::VKEY_F6,
               mojom::TopRowKey::kFullscreen, ui::TopRowActionKey::kFullscreen);

  ui::KeyboardCapability::KeyboardInfo info;
  info.top_row_scan_codes = top_row_scan_codes_;
  info.top_row_action_keys = action_keys;
  info.device_type =
      ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
  info.top_row_layout =
      ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom;
  Shell::Get()->keyboard_capability()->SetKeyboardInfoForTesting(
      ui::KeyboardDevice(device_information.input_device), std::move(info));

  device_information.keyboard_scan_codes = top_row_scan_codes_;

  keyboard_info_ = input_data_provider_keyboard_->ConstructKeyboard(
      &device_information, &aux_data_);

  // In the input map, we have Back, Refresh, and Fullscreen in the correct
  // order, but spaced out with a gap from F3-F5. This gap should be removed
  // as a gap in the input map implies the keys do not exist.
  EXPECT_EQ(0u, aux_data_.top_row_key_scancode_indexes[kBack]);
  EXPECT_EQ(1u, aux_data_.top_row_key_scancode_indexes[kRefresh]);
  EXPECT_EQ(2u, aux_data_.top_row_key_scancode_indexes[kFullscreen]);

  EXPECT_EQ(top_row_keys_, keyboard_info_->top_row_keys);
}

TEST_F(VivaldiKeyboardTestBase, TurkishNoFLayout) {
  SetCurrentImeId(std::string(kTurkishKeyboardLayoutId));
  statistics_provider_.SetMachineStatistic(system::kRegionKey,
                                           std::string(kTurkeyRegionCode));

  keyboard_info_ = input_data_provider_keyboard_->ConstructKeyboard(
      &device_information, &aux_data_);

  EXPECT_EQ(kTurkeyRegionCode, keyboard_info_->region_code);
}

TEST_F(VivaldiKeyboardTestBase, TurkishFLayout) {
  SetCurrentImeId(std::string(kTurkishKeyboardFLayoutId));
  statistics_provider_.SetMachineStatistic(system::kRegionKey,
                                           std::string(kTurkeyRegionCode));

  keyboard_info_ = input_data_provider_keyboard_->ConstructKeyboard(
      &device_information, &aux_data_);

  EXPECT_EQ(kTurkeyFLayoutRegionCode, keyboard_info_->region_code);
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
            {"JUNK_IDENTIFIER", mojom::MechanicalLayout::kUnknown}}),
    [](const testing::TestParamInfo<MechanicalLayoutTest::ParamType>& info) {
      return std::get<0>(info.param);
    });

TEST_P(MechanicalLayoutTest, CheckLayout) {
  statistics_provider_.SetMachineStatistic(system::kKeyboardMechanicalLayoutKey,
                                           layout_string_);

  keyboard_info_ = input_data_provider_keyboard_->ConstructKeyboard(
      &device_information, &aux_data_);

  EXPECT_EQ(expected_layout_, keyboard_info_->mechanical_layout);
}

class RegionCodeTest : public VivaldiKeyboardTestBase,
                       public testing::WithParamInterface<std::string> {
  void SetUp() override {
    VivaldiKeyboardTestBase::SetUp();
    region_code_ = GetParam();
  }

 protected:
  std::string region_code_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    RegionCodeTest,
    testing::Values("us", "jp", "de"),
    [](const testing::TestParamInfo<RegionCodeTest::ParamType>& info) {
      return info.param;
    });

TEST_P(RegionCodeTest, CheckRegionCode) {
  statistics_provider_.SetMachineStatistic(system::kRegionKey, region_code_);

  keyboard_info_ = input_data_provider_keyboard_->ConstructKeyboard(
      &device_information, &aux_data_);

  EXPECT_EQ(region_code_, keyboard_info_->region_code);
}

class HasNumpadTest : public VivaldiKeyboardTestBase {};
TEST_F(HasNumpadTest, SwitchEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kHasNumberPad);

  keyboard_info_ = input_data_provider_keyboard_->ConstructKeyboard(
      &device_information, &aux_data_);

  EXPECT_EQ(mojom::NumberPadPresence::kPresent,
            keyboard_info_->number_pad_present);
}

TEST_F(HasNumpadTest, SwitchDisabled) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(switches::kHasNumberPad);

  keyboard_info_ = input_data_provider_keyboard_->ConstructKeyboard(
      &device_information, &aux_data_);

  EXPECT_EQ(mojom::NumberPadPresence::kNotPresent,
            keyboard_info_->number_pad_present);
}

class RevenBoardTest : public VivaldiKeyboardTestBase {};
TEST_F(RevenBoardTest, SwitchEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kRevenBranding);

  keyboard_info_ = input_data_provider_keyboard_->ConstructKeyboard(
      &device_information, &aux_data_);

  EXPECT_EQ(mojom::ConnectionType::kUnknown, keyboard_info_->connection_type);
  EXPECT_EQ(mojom::PhysicalLayout::kUnknown, keyboard_info_->physical_layout);
}

TEST_F(RevenBoardTest, SwitchDisabled) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kRevenBranding);

  keyboard_info_ = input_data_provider_keyboard_->ConstructKeyboard(
      &device_information, &aux_data_);

  EXPECT_EQ(mojom::ConnectionType::kInternal, keyboard_info_->connection_type);
  EXPECT_EQ(mojom::PhysicalLayout::kChromeOS, keyboard_info_->physical_layout);
}

class AssistantKeyTest : public VivaldiKeyboardTestBase {};
TEST_F(AssistantKeyTest, DrobitNoAssistantKey) {
  InitInputDeviceInformation(ui::kDrobitKeyboard);

  keyboard_info_ = input_data_provider_keyboard_->ConstructKeyboard(
      &device_information, &aux_data_);

  EXPECT_FALSE(keyboard_info_->has_assistant_key);
}

TEST_F(AssistantKeyTest, EveHasAssistantKey) {
  InitInputDeviceInformation(ui::kEveKeyboard);

  keyboard_info_ = input_data_provider_keyboard_->ConstructKeyboard(
      &device_information, &aux_data_);

  EXPECT_TRUE(keyboard_info_->has_assistant_key);
}

class ConstructInputKeyEventTest
    : public VivaldiKeyboardTestBase,
      public testing::WithParamInterface<
          std::tuple<std::tuple<uint32_t, uint32_t>, bool>> {
 public:
  void SetUp() override {
    VivaldiKeyboardTestBase::SetUp();
    InitAuxData();
    std::tie(key_code_, scan_code_) = std::get<0>(GetParam());
    down_ = std::get<1>(GetParam());
  }

  void InitAuxData() {
    base::flat_map<uint32_t, uint32_t>& map =
        aux_data_.top_row_key_scancode_indexes;
    map.clear();
    map[VivaldiTopRowScanCode::kBack] = 0;
    map[VivaldiTopRowScanCode::kRefresh] = 1;
    map[VivaldiTopRowScanCode::kFullscreen] = 2;
    map[VivaldiTopRowScanCode::kOverview] = 3;
    map[VivaldiTopRowScanCode::kScreenshot] = 4;
    map[VivaldiTopRowScanCode::kScreenBrightnessDown] = 5;
    map[VivaldiTopRowScanCode::kScreenBrightnessUp] = 6;
  }

 protected:
  uint32_t key_code_;
  uint32_t scan_code_;
  bool down_;
};

class NormalKeyConstructInputKeyEventTest : public ConstructInputKeyEventTest {
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    NormalKeyConstructInputKeyEventTest,
    testing::Combine(
        testing::ValuesIn(std::vector<std::tuple<uint32_t, uint32_t>>{
            {KEY_A, ui::VKEY_A},
            {KEY_5, ui::VKEY_5},
            {KEY_ESC, ui::VKEY_ESCAPE},
            {KEY_SPACE, ui::VKEY_SPACE}}),
        testing::Bool()));

TEST_P(NormalKeyConstructInputKeyEventTest, NormalKey) {
  auto result = input_data_provider_keyboard_->ConstructInputKeyEvent(
      keyboard_info_, &aux_data_, key_code_, scan_code_, down_);

  EXPECT_EQ(keyboard_info_->id, result->id);
  EXPECT_EQ(key_code_, result->key_code);
  EXPECT_EQ(scan_code_, result->scan_code);
  EXPECT_EQ(down_ ? mojom::KeyEventType::kPress : mojom::KeyEventType::kRelease,
            result->type);

  // ConstructInputKeyEvent returns -1 when the key is not in the top row
  EXPECT_EQ(kNotInTopRowValue, result->top_row_position);
}

class TopRowKeyConstructInputKeyEventTest : public ConstructInputKeyEventTest {
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    TopRowKeyConstructInputKeyEventTest,
    testing::Combine(
        testing::ValuesIn(std::vector<std::tuple<uint32_t, uint32_t>>{
            {KEY_BACK, VivaldiTopRowScanCode::kBack},
            {KEY_REFRESH, VivaldiTopRowScanCode::kRefresh},
            {KEY_FULL_SCREEN, VivaldiTopRowScanCode::kFullscreen},
            {KEY_SCALE, VivaldiTopRowScanCode::kOverview}}),
        testing::Bool()));

TEST_P(TopRowKeyConstructInputKeyEventTest, TopRowKey) {
  auto result = input_data_provider_keyboard_->ConstructInputKeyEvent(
      keyboard_info_, &aux_data_, key_code_, scan_code_, down_);

  EXPECT_EQ(keyboard_info_->id, result->id);
  EXPECT_EQ(key_code_, result->key_code);
  EXPECT_EQ(scan_code_, result->scan_code);
  EXPECT_EQ(down_ ? mojom::KeyEventType::kPress : mojom::KeyEventType::kRelease,
            result->type);

  EXPECT_EQ(
      static_cast<int32_t>(aux_data_.top_row_key_scancode_indexes[scan_code_]),
      result->top_row_position);
}

class FKeyConstructInputKeyEventTest : public ConstructInputKeyEventTest {};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    FKeyConstructInputKeyEventTest,
    testing::Combine(
        testing::ValuesIn(std::vector<std::tuple<uint32_t, uint32_t>>{
            {KEY_F1, ui::VKEY_F1},
            {KEY_F2, ui::VKEY_F2},
            {KEY_F3, ui::VKEY_F3},
            {KEY_F4, ui::VKEY_F4},
            {KEY_F5, ui::VKEY_F5},
            {KEY_F6, ui::VKEY_F6},
            {KEY_F7, ui::VKEY_F7},
            {KEY_F8, ui::VKEY_F8},
            {KEY_F9, ui::VKEY_F9},
            {KEY_F10, ui::VKEY_F10},
            {KEY_F11, ui::VKEY_F11},
            {KEY_F12, ui::VKEY_F12},
            {KEY_F13, ui::VKEY_F13},
            {KEY_F14, ui::VKEY_F14},
            {KEY_F15, ui::VKEY_F15}}),
        testing::Bool()));

TEST_P(FKeyConstructInputKeyEventTest, FKey) {
  auto result = input_data_provider_keyboard_->ConstructInputKeyEvent(
      keyboard_info_, &aux_data_, key_code_, scan_code_, down_);

  EXPECT_EQ(keyboard_info_->id, result->id);
  EXPECT_EQ(key_code_, result->key_code);
  EXPECT_EQ(scan_code_, result->scan_code);
  EXPECT_EQ(down_ ? mojom::KeyEventType::kPress : mojom::KeyEventType::kRelease,
            result->type);

  EXPECT_EQ(static_cast<int>(scan_code_ - ui::VKEY_F1),
            result->top_row_position);
}

}  // namespace ash::diagnostics
