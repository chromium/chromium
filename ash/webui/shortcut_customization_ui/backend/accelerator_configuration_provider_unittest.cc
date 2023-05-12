// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_configuration_provider.h"

#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/accelerator_configuration.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/mojom/accelerator_configuration.mojom.h"
#include "ash/public/mojom/accelerator_info.mojom-shared.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom-forward.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom-test-utils.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {
using NonConfigurableActionToParts =
    const std::map<NonConfigurableActions,
                   const std::vector<mojom::TextAcceleratorPartPtr&>>;

namespace {
using shortcut_customization::mojom::AcceleratorResultDataPtr;
using shortcut_customization::mojom::SimpleAccelerator;
using shortcut_customization::mojom::SimpleAcceleratorPtr;

using mojom::AcceleratorConfigResult;

constexpr char kKbdTopRowPropertyName[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";
constexpr char kKbdTopRowLayout2Tag[] = "2";

class FakeDeviceManager {
 public:
  FakeDeviceManager() = default;
  FakeDeviceManager(const FakeDeviceManager&) = delete;
  FakeDeviceManager& operator=(const FakeDeviceManager&) = delete;
  ~FakeDeviceManager() = default;

  // Add a fake keyboard to DeviceDataManagerTestApi and provide layout info to
  // fake udev.
  void AddFakeKeyboard(const ui::KeyboardDevice& fake_keyboard,
                       const std::string& layout) {
    fake_keyboard_devices_.push_back(fake_keyboard);

    ui::DeviceDataManagerTestApi().SetKeyboardDevices({});
    ui::DeviceDataManagerTestApi().SetKeyboardDevices(fake_keyboard_devices_);
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

    std::map<std::string, std::string> sysfs_properties;
    std::map<std::string, std::string> sysfs_attributes;
    sysfs_properties[kKbdTopRowPropertyName] = layout;
    fake_udev_.AddFakeDevice(fake_keyboard.name, fake_keyboard.sys_path.value(),
                             /*subsystem=*/"input", /*devnode=*/absl::nullopt,
                             /*devtype=*/absl::nullopt,
                             std::move(sysfs_attributes),
                             std::move(sysfs_properties));
  }

  void RemoveAllDevices() {
    fake_udev_.Reset();
    fake_keyboard_devices_.clear();
  }

 private:
  testing::FakeUdevLoader fake_udev_;
  std::vector<ui::KeyboardDevice> fake_keyboard_devices_;
};
class FakeAcceleratorsUpdatedObserver
    : public shortcut_ui::AcceleratorConfigurationProvider::
          AcceleratorsUpdatedObserver {
 public:
  FakeAcceleratorsUpdatedObserver() = default;
  ~FakeAcceleratorsUpdatedObserver() override = default;

  int num_times_notified() const { return num_times_notified_; }

  void clear_num_times_notified() { num_times_notified_ = 0; }

  // AcceleratorConfigurationProvider::AcceleratorsUpdatedObserver:
  void OnAcceleratorsUpdated(
      shortcut_ui::AcceleratorConfigurationProvider::AcceleratorConfigurationMap
          config) override {
    ++num_times_notified_;
  }

 private:
  int num_times_notified_ = 0;
};

class FakeAcceleratorsUpdatedMojoObserver
    : public shortcut_customization::mojom::AcceleratorsUpdatedObserver {
 public:
  void OnAcceleratorsUpdated(
      shortcut_ui::AcceleratorConfigurationProvider::AcceleratorConfigurationMap
          config) override {
    config_ = std::move(config);
    ++num_times_notified_;
  }

  mojo::PendingRemote<
      shortcut_customization::mojom::AcceleratorsUpdatedObserver>
  pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  int num_times_notified() { return num_times_notified_; }

  void clear_num_times_notified() { num_times_notified_ = 0; }

  shortcut_ui::AcceleratorConfigurationProvider::AcceleratorConfigurationMap
  config() {
    return mojo::Clone(config_);
  }

 private:
  mojo::Receiver<shortcut_customization::mojom::AcceleratorsUpdatedObserver>
      receiver_{this};
  shortcut_ui::AcceleratorConfigurationProvider::AcceleratorConfigurationMap
      config_;
  int num_times_notified_ = 0;
};

bool AreAcceleratorsEqual(const ui::Accelerator& expected_accelerator,
                          const mojom::AcceleratorInfoPtr& actual_info) {
  const bool accelerator_equals =
      expected_accelerator ==
      actual_info->layout_properties->get_standard_accelerator()->accelerator;
  const bool key_display_equals =
      ash::GetKeyDisplay(expected_accelerator.key_code()) ==
      actual_info->layout_properties->get_standard_accelerator()->key_display;
  return accelerator_equals && key_display_equals;
}

bool CompareAccelerators(const ash::AcceleratorData& expected_data,
                         const mojom::AcceleratorInfoPtr& actual_info) {
  ui::Accelerator expected_accelerator(expected_data.keycode,
                                       expected_data.modifiers);
  return AreAcceleratorsEqual(expected_accelerator, actual_info);
}

bool CompareAccelerators(const ui::Accelerator& expected_accelerator,
                         const mojom::AcceleratorInfoPtr& actual_info) {
  return AreAcceleratorsEqual(expected_accelerator, actual_info);
}

void CompareInputDevices(const ui::KeyboardDevice& expected,
                         const ui::KeyboardDevice& actual) {
  EXPECT_EQ(expected.type, actual.type);
  EXPECT_EQ(expected.id, actual.id);
  EXPECT_EQ(expected.name, actual.name);
}

void ExpectMojomAcceleratorsEqual(
    ash::mojom::AcceleratorSource source,
    const base::span<const ash::AcceleratorData>& expected,
    const ash::shortcut_ui::AcceleratorConfigurationProvider::
        AcceleratorConfigurationMap& actual_config) {
  const auto& source_config_iter = actual_config.find(source);
  EXPECT_TRUE(source_config_iter != actual_config.end());
  for (const auto& [action_id, actual_accels] : source_config_iter->second) {
    for (const auto& actual_info : actual_accels) {
      bool found_match = false;
      for (const auto& expected_data : expected) {
        found_match =
            CompareAccelerators(expected_data, mojo::Clone(actual_info));
        if (found_match) {
          break;
        }
      }
      EXPECT_TRUE(found_match);
    }
  }
}

// Validates that the passed in layout infos have matching accelerator layouts
// in `kAcceleratorLayouts`. If this throws an expectation error it means that
// the there is a inconsistency between the layouts in `kAcceleratorLayouts` and
// the data provided by `AcceleratorConfigurationProvider`.
void ValidateAcceleratorLayouts(
    const std::vector<ash::mojom::AcceleratorLayoutInfoPtr>&
        actual_layout_infos) {
  for (const auto& actual : actual_layout_infos) {
    // Iterate through `kAcceleratorLayouts` to find the matching action.
    bool found_match = false;
    for (const auto& expected_layout : kAcceleratorLayouts) {
      if (expected_layout.action_id == actual->action &&
          expected_layout.source == actual->source) {
        EXPECT_EQ(expected_layout.category, actual->category);
        EXPECT_EQ(expected_layout.sub_category, actual->sub_category);
        EXPECT_EQ(expected_layout.layout_style, actual->style);
        EXPECT_EQ(expected_layout.source, actual->source);
        EXPECT_EQ(
            l10n_util::GetStringUTF16(expected_layout.description_string_id),
            actual->description);
        found_match = true;
        break;
      }
    }
    EXPECT_TRUE(found_match);
  }
}

void ValidateTextAccelerators(const TextAcceleratorPart& lhs,
                              const mojom::TextAcceleratorPartPtr& rhs) {
  EXPECT_EQ(lhs.text, rhs->text);
  EXPECT_EQ(lhs.type, rhs->type);
}

std::vector<mojom::TextAcceleratorPartPtr> RemovePlainTextParts(
    const std::vector<mojom::TextAcceleratorPartPtr>& parts) {
  std::vector<mojom::TextAcceleratorPartPtr> res;
  for (const auto& part : parts) {
    if (part->type == mojom::TextAcceleratorPartType::kPlainText) {
      continue;
    }
    res.push_back(mojo::Clone(part));
  }
  return res;
}

std::vector<std::u16string> SplitStringOnOffsets(
    const std::u16string& input,
    const std::vector<size_t>& offsets) {
  DCHECK(std::is_sorted(offsets.begin(), offsets.end()));

  std::vector<std::u16string> parts;
  // At most there will be len(offsets) + 1 text parts.
  parts.reserve(offsets.size() + 1);
  size_t upto = 0;

  for (auto offset : offsets) {
    DCHECK_LE(offset, input.size());

    if (offset == upto) {
      continue;
    }

    DCHECK(offset >= upto);
    parts.push_back(input.substr(upto, offset - upto));
    upto = offset;
  }

  // Handles the case where there's plain text after the last replacement.
  if (upto < input.size()) {
    parts.push_back(input.substr(upto));
  }

  return parts;
}

}  // namespace

namespace shortcut_ui {

class AcceleratorConfigurationProviderTest : public AshTestBase {
 public:
  AcceleratorConfigurationProviderTest() = default;
  ~AcceleratorConfigurationProviderTest() override = default;

  class TestInputMethodManager : public input_method::MockInputMethodManager {
   public:
    void AddObserver(
        input_method::InputMethodManager::Observer* observer) override {
      observers_.AddObserver(observer);
    }

    void RemoveObserver(
        input_method::InputMethodManager::Observer* observer) override {
      observers_.RemoveObserver(observer);
    }

    // Calls all observers with Observer::InputMethodChanged
    void NotifyInputMethodChanged() {
      for (auto& observer : observers_) {
        observer.InputMethodChanged(
            /*manager=*/this, /*profile=*/nullptr, /*show_message=*/false);
      }
    }

    base::ObserverList<InputMethodManager::Observer>::Unchecked observers_;
  };

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {::features::kImprovedKeyboardShortcuts,
         ::features::kShortcutCustomization},
        {});
    input_method_manager_ = new TestInputMethodManager();
    input_method::InputMethodManager::Initialize(input_method_manager_);

    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();

    provider_ = std::make_unique<AcceleratorConfigurationProvider>();
    provider_->AddObserver(&observer_);
    // After the provider is constructed, the observer should not have been
    // notified yet.
    EXPECT_EQ(0, observer_.num_times_notified());
    non_configurable_actions_map_ =
        provider_->GetNonConfigurableAcceleratorsForTesting();

    fake_keyboard_manager_ = std::make_unique<FakeDeviceManager>();
    // Add a fake layout2 keyboard.
    ui::KeyboardDevice fake_keyboard(
        /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
        /*name=*/"fake_Keyboard");
    fake_keyboard.sys_path = base::FilePath("path1");
    fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard,
                                            kKbdTopRowLayout2Tag);

    provider_->ignore_layouts_for_testing_ = true;
    base::RunLoop().RunUntilIdle();
    // After adding a fake keyboard, clear the observer call count.
    observer_.clear_num_times_notified();
    EXPECT_EQ(0, observer_.num_times_notified());
  }

  void TearDown() override {
    fake_keyboard_manager_->RemoveAllDevices();
    provider_->RemoveObserver(&observer_);
    // `provider_` has a dependency on `input_method_manager_`.
    provider_.reset();
    AshTestBase::TearDown();
    input_method::InputMethodManager::Shutdown();
    input_method_manager_ = nullptr;
  }

 protected:
  const std::vector<ui::KeyboardDevice>& GetConnectedKeyboards() {
    return provider_->connected_keyboards_;
  }

  void SetUpObserver(FakeAcceleratorsUpdatedMojoObserver* mojo_observer) {
    provider_->AddObserver(mojo_observer->pending_remote());
    base::RunLoop().RunUntilIdle();
  }

  void SetLayoutDetailsMap(
      const std::vector<AcceleratorLayoutDetails>& layouts) {
    provider_->SetLayoutDetailsMapForTesting(layouts);
  }

  const std::vector<ui::Accelerator>& GetAcceleratorsForAction(int action_id) {
    return non_configurable_actions_map_
        .find(static_cast<ash::NonConfigurableActions>(action_id))
        ->second.accelerators.value();
  }

  const std::vector<TextAcceleratorPart>& GetReplacementsForAction(
      int action_id) {
    return non_configurable_actions_map_
        .find(static_cast<ash::NonConfigurableActions>(action_id))
        ->second.replacements.value();
  }

  bool TextAccelContainsReplacements(int action_id) {
    return non_configurable_actions_map_
        .find(static_cast<ash::NonConfigurableActions>(action_id))
        ->second.replacements.has_value();
  }

  int GetMessageIdForTextAccel(int action_id) {
    return non_configurable_actions_map_
        .find(static_cast<ash::NonConfigurableActions>(action_id))
        ->second.message_id.value();
  }

  const std::vector<ui::Accelerator>& GetNonConfigurableAcceleratorsForActionId(
      uint32_t id) {
    const auto accelerator_iter =
        provider_->id_to_non_configurable_accelerators_.find(id);
    DCHECK(accelerator_iter !=
           provider_->id_to_non_configurable_accelerators_.end());
    return accelerator_iter->second;
  }

  uint32_t GetNonConfigurableIdFromAccelerator(ui::Accelerator accelerator) {
    return provider_->non_configurable_accelerator_to_id_.Get(accelerator);
  }

  std::unique_ptr<AcceleratorConfigurationProvider> provider_;
  NonConfigurableActionsMap non_configurable_actions_map_;
  base::test::ScopedFeatureList scoped_feature_list_;
  // Test global singleton. Delete is handled by InputMethodManager::Shutdown().
  raw_ptr<TestInputMethodManager> input_method_manager_;
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_;
  FakeAcceleratorsUpdatedObserver observer_;
};

TEST_F(AcceleratorConfigurationProviderTest, ResetReceiverOnBindInterface) {
  mojo::Remote<shortcut_customization::mojom::AcceleratorConfigurationProvider>
      remote;
  provider_->BindInterface(remote.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();

  remote.reset();

  provider_->BindInterface(remote.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AcceleratorConfigurationProviderTest, BrowserIsMutable) {
  // Verify that requesting IsMutable state for Browser accelerators returns
  // false.
  provider_->IsMutable(ash::mojom::AcceleratorSource::kBrowser,
                       base::BindLambdaForTesting([&](bool is_mutable) {
                         // Browser accelerators are not mutable.
                         EXPECT_FALSE(is_mutable);
                       }));
  base::RunLoop().RunUntilIdle();
}

TEST_F(AcceleratorConfigurationProviderTest, AshIsMutable) {
  // Verify that requesting IsMutable state for Ash accelerators returns true.
  provider_->IsMutable(ash::mojom::AcceleratorSource::kAsh,
                       base::BindLambdaForTesting([&](bool is_mutable) {
                         // Ash accelerators are mutable.
                         EXPECT_TRUE(is_mutable);
                       }));
  base::RunLoop().RunUntilIdle();
}

TEST_F(AcceleratorConfigurationProviderTest, InitialAccelInitCalls) {
  FakeAcceleratorsUpdatedMojoObserver mojo_observer;
  SetUpObserver(&mojo_observer);
  EXPECT_EQ(0, mojo_observer.num_times_notified());
  EXPECT_EQ(0, observer_.num_times_notified());

  Shell::Get()->ash_accelerator_configuration()->Initialize();
  base::RunLoop().RunUntilIdle();

  // Observer is initially notified twice, one for ash accelerators and the
  // other for deprecated accelerators.
  EXPECT_EQ(2, mojo_observer.num_times_notified());
  EXPECT_EQ(2, observer_.num_times_notified());
}

TEST_F(AcceleratorConfigurationProviderTest, AshAcceleratorsUpdated) {
  FakeAcceleratorsUpdatedMojoObserver mojo_observer;
  SetUpObserver(&mojo_observer);
  EXPECT_EQ(0, mojo_observer.num_times_notified());
  EXPECT_EQ(0, observer_.num_times_notified());

  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kShowTaskManager},
  };
  Shell::Get()->ash_accelerator_configuration()->Initialize(test_data);
  base::RunLoop().RunUntilIdle();
  // Notified once after instantiating the accelerators.
  EXPECT_EQ(1, mojo_observer.num_times_notified());
  EXPECT_EQ(1, observer_.num_times_notified());
  // Verify observer received the correct accelerators.
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh, test_data,
                               mojo_observer.config());

  // Initialize with a new set of accelerators.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_ALT_DOWN,
       AcceleratorAction::kSwapPrimaryDisplay},
      {/*trigger_on_press=*/true, ui::VKEY_MEDIA_LAUNCH_APP1,
       ui::EF_CONTROL_DOWN, AcceleratorAction::kTakeScreenshot},
  };
  Shell::Get()->ash_accelerator_configuration()->Initialize(updated_test_data);
  base::RunLoop().RunUntilIdle();
  // Observers are notified again after a new set of accelerators are provided.
  EXPECT_EQ(2, mojo_observer.num_times_notified());
  EXPECT_EQ(2, observer_.num_times_notified());
  // Verify observer has been updated with the new set of accelerators.
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh,
                               updated_test_data, mojo_observer.config());
}

TEST_F(AcceleratorConfigurationProviderTest, ConnectedKeyboardsUpdated) {
  // Ensure there are no keyboards plugged in at first.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({});
  FakeAcceleratorsUpdatedMojoObserver mojo_observer;
  SetUpObserver(&mojo_observer);

  EXPECT_EQ(0, mojo_observer.num_times_notified());

  const std::vector<ui::KeyboardDevice>& actual_devices =
      GetConnectedKeyboards();
  EXPECT_EQ(0u, actual_devices.size());

  ui::KeyboardDevice expected_test_keyboard(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "Keyboard");

  std::vector<ui::KeyboardDevice> keyboard_devices;
  keyboard_devices.push_back(expected_test_keyboard);

  ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboard_devices);

  const std::vector<ui::KeyboardDevice>& actual_devices2 =
      GetConnectedKeyboards();
  EXPECT_EQ(1u, actual_devices2.size());
  CompareInputDevices(expected_test_keyboard, actual_devices[0]);

  base::RunLoop().RunUntilIdle();
  // Adding a new keyboard should trigger the UpdatedAccelerators observer.
  EXPECT_EQ(1, mojo_observer.num_times_notified());
}

TEST_F(AcceleratorConfigurationProviderTest, ValidateAllAcceleratorLayouts) {
  // Initialize with all default accelerators.
  Shell::Get()->ash_accelerator_configuration()->Initialize();
  base::RunLoop().RunUntilIdle();

  // Get all default accelerator layout infos and verify that they have the
  // correctly mapped layout details
  provider_->GetAcceleratorLayoutInfos(base::BindLambdaForTesting(
      [&](std::vector<mojom::AcceleratorLayoutInfoPtr> actual_layout_infos) {
        ValidateAcceleratorLayouts(actual_layout_infos);
      }));

  // Validate that the non-callback version of this method returns correct data.
  ValidateAcceleratorLayouts(provider_->GetAcceleratorLayoutInfos());
}

TEST_F(AcceleratorConfigurationProviderTest, FilterOutHiddenAccelerators) {
  FakeAcceleratorsUpdatedMojoObserver mojo_observer;
  SetUpObserver(&mojo_observer);
  EXPECT_EQ(0, mojo_observer.num_times_notified());
  EXPECT_EQ(0, observer_.num_times_notified());

  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kShowTaskManager},
      // Accelerators that should be hidden from display.
      {/*trigger_on_press=*/true, ui::VKEY_BROWSER_SEARCH, ui::EF_SHIFT_DOWN,
       AcceleratorAction::kToggleAppList},
      {/*trigger_on_press=*/false, ui::VKEY_LWIN, ui::EF_SHIFT_DOWN,
       AcceleratorAction::kToggleAppList},
      {/*trigger_on_press=*/true, ui::VKEY_F14, ui::EF_NONE,
       AcceleratorAction::kShowShortcutViewer},
      {/*trigger_on_press=*/true, ui::VKEY_OEM_2,
       ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kShowShortcutViewer},
      {/*trigger_on_press=*/true, ui::VKEY_OEM_2,
       ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
       AcceleratorAction::kOpenGetHelp},
      {/*trigger_on_press=*/false, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_SHIFT_DOWN,
       AcceleratorAction::kToggleFullscreen},
  };

  // Initialize with a set of accelerators that include hidden accelerators.
  Shell::Get()->ash_accelerator_configuration()->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  const AcceleratorData expected_test_data[]{
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kShowTaskManager},
  };
  EXPECT_EQ(1, observer_.num_times_notified());

  // Verify observer won't receive hidden accelerators.
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh,
                               expected_test_data, mojo_observer.config());
}

TEST_F(AcceleratorConfigurationProviderTest, TopRowKeyAcceleratorRemapped) {
  // Add a fake layout2 keyboard.
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"fake_Keyboard");
  fake_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout2Tag);

  // Disable TopRowKeysAreFKeys.
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
        prefs::kSendFunctionKeys, false);
    EXPECT_FALSE(Shell::Get()->keyboard_capability()->TopRowKeysAreFKeys());
  } else {
    auto settings = Shell::Get()
                        ->input_device_settings_controller()
                        ->GetKeyboardSettings(fake_keyboard.id)
                        ->Clone();
    settings->top_row_are_fkeys = false;
    Shell::Get()->input_device_settings_controller()->SetKeyboardSettings(
        fake_keyboard.id, std::move(settings));
  }

  FakeAcceleratorsUpdatedMojoObserver mojo_observer;
  SetUpObserver(&mojo_observer);
  EXPECT_EQ(0, mojo_observer.num_times_notified());

  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kShowTaskManager},
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_SHIFT_DOWN,
       AcceleratorAction::kToggleFullscreen},
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_NONE,
       AcceleratorAction::kToggleFullscreen},
      {/*trigger_on_press=*/true, ui::VKEY_BRIGHTNESS_UP, ui::EF_NONE,
       AcceleratorAction::kBrightnessUp},
      {/*trigger_on_press=*/true, ui::VKEY_BRIGHTNESS_UP, ui::EF_ALT_DOWN,
       AcceleratorAction::kKeyboardBrightnessUp},
      // Fake accelerator data - [search] is part of the original accelerator.
      {/*trigger_on_press=*/true, ui::VKEY_BRIGHTNESS_UP,
       ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
       AcceleratorAction::kKeyboardBrightnessUp},
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kToggleFullscreen},
  };

  Shell::Get()->ash_accelerator_configuration()->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  // Notified after instantiating the accelerators.
  EXPECT_EQ(1, mojo_observer.num_times_notified());
  // Verify observer received the correct accelerators.
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh, test_data,
                               mojo_observer.config());

  // Enable TopRowKeysAreFKeys.
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
        prefs::kSendFunctionKeys, true);
    EXPECT_TRUE(Shell::Get()->keyboard_capability()->TopRowKeysAreFKeys());
  } else {
    auto settings = Shell::Get()
                        ->input_device_settings_controller()
                        ->GetKeyboardSettings(fake_keyboard.id)
                        ->Clone();
    settings->top_row_are_fkeys = true;
    Shell::Get()->input_device_settings_controller()->SetKeyboardSettings(
        fake_keyboard.id, std::move(settings));
  }
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, mojo_observer.num_times_notified());

  // Initialize the same test_data again, but with
  // TopRowKeysAsFunctionKeysEnabled.
  Shell::Get()->ash_accelerator_configuration()->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  // When TopRowKeysAsFunctionKeys enabled, top row shortcut will become [Fkey]
  // + [search] + [modifier].
  const AcceleratorData expected_test_data[] = {
      // alt + tab -> alt + tab
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      // alt + shift + tab -> alt + shift + tab
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
      // search + esc -> search + esc
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kShowTaskManager},
      // shift + zoom -> shift + search + VKEY_ZOOM
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM,
       ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN,
       AcceleratorAction::kToggleFullscreen},
      // zoom -> search + VKEY_ZOOM
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kToggleFullscreen},
      // brightness_up -> search + VKEY_BRIGHTNESS_UP
      {/*trigger_on_press=*/true, ui::VKEY_BRIGHTNESS_UP, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kBrightnessUp},
      // alt + brightness_up -> alt + search + VKEY_BRIGHTNESS_UP
      {/*trigger_on_press=*/true, ui::VKEY_BRIGHTNESS_UP,
       ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
       AcceleratorAction::kKeyboardBrightnessUp},
  };

  EXPECT_EQ(3, mojo_observer.num_times_notified());
  // Verify observer received the top-row-remapped accelerators.
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh,
                               expected_test_data, mojo_observer.config());
}

TEST_F(AcceleratorConfigurationProviderTest, SixPackKeyAcceleratorRemapped) {
  FakeAcceleratorsUpdatedMojoObserver mojo_observer;
  SetUpObserver(&mojo_observer);
  EXPECT_EQ(0, mojo_observer.num_times_notified());

  // kImprovedKeyboardShortcuts is enabled.
  EXPECT_TRUE(::features::IsImprovedKeyboardShortcutsEnabled());

  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      // Below are fake shortcuts, only used for testing.
      {/*trigger_on_press=*/true, ui::VKEY_DELETE, ui::EF_NONE,
       AcceleratorAction::kCycleBackwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_HOME, ui::EF_NONE,
       AcceleratorAction::kTakeWindowScreenshot},
      {/*trigger_on_press=*/true, ui::VKEY_HOME, ui::EF_ALT_DOWN,
       AcceleratorAction::kKeyboardBrightnessUp},
      {/*trigger_on_press=*/true, ui::VKEY_END, ui::EF_SHIFT_DOWN,
       AcceleratorAction::kDisableCapsLock},
      {/*trigger_on_press=*/true, ui::VKEY_NEXT, ui::EF_ALT_DOWN,
       AcceleratorAction::kNewTab},
      {/*trigger_on_press=*/true, ui::VKEY_INSERT, ui::EF_NONE,
       AcceleratorAction::kNewTab},
      {/*trigger_on_press=*/true, ui::VKEY_INSERT, ui::EF_ALT_DOWN,
       AcceleratorAction::kNewTab},
      // When [search] is part of the original accelerator.
      {/*trigger_on_press=*/true, ui::VKEY_HOME,
       ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
       AcceleratorAction::kKeyboardBrightnessUp},
      {/*trigger_on_press=*/true, ui::VKEY_END,
       ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN,
       AcceleratorAction::kDisableCapsLock},
      //  Edge case: [Shift] + [Delete].
      {/*trigger_on_press=*/true, ui::VKEY_DELETE, ui::EF_SHIFT_DOWN,
       AcceleratorAction::kDesksNewDesk},
  };

  const AcceleratorData expected_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      // Below are fake shortcuts, only used for testing.
      {/*trigger_on_press=*/true, ui::VKEY_DELETE, ui::EF_NONE,
       AcceleratorAction::kCycleBackwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_HOME, ui::EF_NONE,
       AcceleratorAction::kTakeWindowScreenshot},
      {/*trigger_on_press=*/true, ui::VKEY_HOME, ui::EF_ALT_DOWN,
       AcceleratorAction::kKeyboardBrightnessUp},
      {/*trigger_on_press=*/true, ui::VKEY_END, ui::EF_SHIFT_DOWN,
       AcceleratorAction::kDisableCapsLock},
      {/*trigger_on_press=*/true, ui::VKEY_NEXT, ui::EF_ALT_DOWN,
       AcceleratorAction::kNewTab},
      {/*trigger_on_press=*/true, ui::VKEY_INSERT, ui::EF_NONE,
       AcceleratorAction::kNewTab},
      {/*trigger_on_press=*/true, ui::VKEY_INSERT, ui::EF_ALT_DOWN,
       AcceleratorAction::kNewTab},

      // When [search] is part of the original accelerator. No remapping is
      // done. Search+Alt+Home -> Search+Alt+Home.
      {/*trigger_on_press=*/true, ui::VKEY_HOME,
       ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
       AcceleratorAction::kKeyboardBrightnessUp},
      // Search+Shift+End -> Search+Shift+End.
      {/*trigger_on_press=*/true, ui::VKEY_END,
       ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN,
       AcceleratorAction::kDisableCapsLock},

      // Edge case: [Shift] + [Delete]. It should not remapped to
      // [Shift]+[Search]+[Back](aka, Insert).
      //  Shift+Delete -> Shift+Delete
      {/*trigger_on_press=*/true, ui::VKEY_DELETE, ui::EF_SHIFT_DOWN,
       AcceleratorAction::kDesksNewDesk},

      // Additional six-pack remapped accelerators.
      // Delete -> Search+Backspace
      {/*trigger_on_press=*/true, ui::VKEY_BACK, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kCycleBackwardMru},
      // Home -> Search+Left
      {/*trigger_on_press=*/true, ui::VKEY_LEFT, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kTakeWindowScreenshot},
      // Alt+Home -> Search+Alt+Left
      {/*trigger_on_press=*/true, ui::VKEY_LEFT,
       ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
       AcceleratorAction::kKeyboardBrightnessUp},
      // Shift+End -> Search+Shift+Right
      {/*trigger_on_press=*/true, ui::VKEY_RIGHT,
       ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN,
       AcceleratorAction::kDisableCapsLock},
      // Alt+Next -> Search+Alt+Down
      {/*trigger_on_press=*/true, ui::VKEY_DOWN,
       ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, AcceleratorAction::kNewTab},
      // Insert -> Search+Shift+Backspace
      {/*trigger_on_press=*/true, ui::VKEY_BACK,
       ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN, AcceleratorAction::kNewTab},
      // Alt+Insert -> Search+Shift+Alt+Backspace
      {/*trigger_on_press=*/true, ui::VKEY_BACK,
       ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
       AcceleratorAction::kNewTab},
  };

  Shell::Get()->ash_accelerator_configuration()->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, mojo_observer.num_times_notified());
  // Verify observer received the correct remapped accelerators.
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh, expected_data,
                               mojo_observer.config());
}

TEST_F(AcceleratorConfigurationProviderTest,
       ReversedSixPackKeyAcceleratorRemapped) {
  FakeAcceleratorsUpdatedMojoObserver mojo_observer;
  SetUpObserver(&mojo_observer);
  EXPECT_EQ(0, mojo_observer.num_times_notified());

  // kImprovedKeyboardShortcuts is enabled.
  EXPECT_TRUE(::features::IsImprovedKeyboardShortcutsEnabled());

  const AcceleratorData test_data[] = {
      // Below are fake shortcuts, only used for testing.
      {/*trigger_on_press=*/true, ui::VKEY_LEFT, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_LEFT, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kNewTab},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kDisableCapsLock},

      {/*trigger_on_press=*/true, ui::VKEY_LEFT,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kKeyboardBrightnessUp},
      {/*trigger_on_press=*/true, ui::VKEY_LEFT,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN,
       AcceleratorAction::kTakeWindowScreenshot},
      {/*trigger_on_press=*/true, ui::VKEY_UP,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, AcceleratorAction::kDesksNewDesk},
      {/*trigger_on_press=*/true, ui::VKEY_RIGHT,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN,
       AcceleratorAction::kToggleFullscreen},
      {/*trigger_on_press=*/true, ui::VKEY_DOWN,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kKeyboardBrightnessDown},

      {/*trigger_on_press=*/true, ui::VKEY_BACK,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_BACK,
       ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kShowTaskManager},
      {/*trigger_on_press=*/true, ui::VKEY_BACK,
       ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
       AcceleratorAction::kBrightnessUp},
  };

  const AcceleratorData expected_data[] = {
      // When [Search] is not part of original accelerator, no remapping is
      // done. [Left]+[Alt]>[Left]+[Alt].
      {/*trigger_on_press=*/true, ui::VKEY_LEFT, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
      // When [Search] is the only modifier, [Left]+[Search]->[Home].
      {/*trigger_on_press=*/true, ui::VKEY_LEFT, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kNewTab},
      {/*trigger_on_press=*/true, ui::VKEY_HOME, ui::EF_NONE,
       AcceleratorAction::kNewTab},
      // When key code is not reversed six pack key, no remapping is done.
      // [Tab]+[Search]+[Alt]->[Tab]+[Search]+[Alt].
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kDisableCapsLock},

      // [Left]+[Search]+[Alt]->[Home]+[Alt].
      {/*trigger_on_press=*/true, ui::VKEY_LEFT,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kKeyboardBrightnessUp},
      {/*trigger_on_press=*/true, ui::VKEY_HOME, ui::EF_ALT_DOWN,
       AcceleratorAction::kKeyboardBrightnessUp},
      // [Left]+[Search]+[Shift]+[Alt]->[Home]+[Shift]+[Alt].
      {/*trigger_on_press=*/true, ui::VKEY_LEFT,
       ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kTakeWindowScreenshot},
      {/*trigger_on_press=*/true, ui::VKEY_HOME,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kTakeWindowScreenshot},
      // [Up]+[Search]+[Alt]->[Prior]+[Alt].
      {/*trigger_on_press=*/true, ui::VKEY_UP,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, AcceleratorAction::kDesksNewDesk},
      {/*trigger_on_press=*/true, ui::VKEY_PRIOR, ui::EF_ALT_DOWN,
       AcceleratorAction::kDesksNewDesk},
      // [Right]+[Search]+[Shift]+[Alt]->[End]+[Shift]+[Alt].
      {/*trigger_on_press=*/true, ui::VKEY_RIGHT,
       ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kToggleFullscreen},
      {/*trigger_on_press=*/true, ui::VKEY_END,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kToggleFullscreen},
      // [Down]+[Search]+[Alt]->[Next]+[Alt].
      {/*trigger_on_press=*/true, ui::VKEY_DOWN,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kKeyboardBrightnessDown},
      {/*trigger_on_press=*/true, ui::VKEY_NEXT, ui::EF_ALT_DOWN,
       AcceleratorAction::kKeyboardBrightnessDown},

      // [Back]+[Search]+[Alt]->[Delete]+[Alt].
      {/*trigger_on_press=*/true, ui::VKEY_BACK,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_DELETE, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      // [Back]+[Search]+[Shift]+[Alt]->[Insert]+[Alt].
      {/*trigger_on_press=*/true, ui::VKEY_BACK,
       ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kShowTaskManager},
      {/*trigger_on_press=*/true, ui::VKEY_INSERT, ui::EF_ALT_DOWN,
       AcceleratorAction::kShowTaskManager},
      // [Back]+[Search]+[Shift] -> [Insert].
      {/*trigger_on_press=*/true, ui::VKEY_BACK,
       ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
       AcceleratorAction::kBrightnessUp},
      {/*trigger_on_press=*/true, ui::VKEY_INSERT, ui::EF_NONE,
       AcceleratorAction::kBrightnessUp},
  };

  Shell::Get()->ash_accelerator_configuration()->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, mojo_observer.num_times_notified());
  // Verify observer received the correct remapped accelerators.
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh, expected_data,
                               mojo_observer.config());
}

TEST_F(AcceleratorConfigurationProviderTest, InputMethodChanged) {
  FakeAcceleratorsUpdatedMojoObserver mojo_observer;
  SetUpObserver(&mojo_observer);
  EXPECT_EQ(0, mojo_observer.num_times_notified());
  Shell::Get()->ash_accelerator_configuration()->Initialize();
  base::RunLoop().RunUntilIdle();
  // Clear extraneous observer calls.
  mojo_observer.clear_num_times_notified();
  EXPECT_EQ(0, mojo_observer.num_times_notified());

  // Change input method, expect observer to be called.
  input_method_manager_->NotifyInputMethodChanged();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mojo_observer.num_times_notified());
}

TEST_F(AcceleratorConfigurationProviderTest, TestGetKeyDisplay) {
  EXPECT_EQ(u"c", ash::GetKeyDisplay(ui::VKEY_C));
  EXPECT_EQ(u"MicrophoneMuteToggle",
            ash::GetKeyDisplay(ui::VKEY_MICROPHONE_MUTE_TOGGLE));
  EXPECT_EQ(u"ToggleWifi", ash::GetKeyDisplay(ui::VKEY_WLAN));
  EXPECT_EQ(u"tab", ash::GetKeyDisplay(ui::VKEY_TAB));
  EXPECT_EQ(u"esc", ash::GetKeyDisplay(ui::VKEY_ESCAPE));
  EXPECT_EQ(u"backspace", ash::GetKeyDisplay(ui::VKEY_BACK));
  EXPECT_EQ(u"enter", ash::GetKeyDisplay(ui::VKEY_RETURN));
  EXPECT_EQ(u"space", ash::GetKeyDisplay(ui::VKEY_SPACE));
  EXPECT_EQ(u"home", ash::GetKeyDisplay(ui::VKEY_HOME));
  EXPECT_EQ(u"end", ash::GetKeyDisplay(ui::VKEY_END));
  EXPECT_EQ(u"delete", ash::GetKeyDisplay(ui::VKEY_DELETE));
  EXPECT_EQ(u"insert", ash::GetKeyDisplay(ui::VKEY_INSERT));
  EXPECT_EQ(u"page up", ash::GetKeyDisplay(ui::VKEY_PRIOR));
  EXPECT_EQ(u"page down", ash::GetKeyDisplay(ui::VKEY_NEXT));
  EXPECT_EQ(u"meta", ash::GetKeyDisplay(ui::VKEY_LWIN));
  EXPECT_EQ(u"alt", ash::GetKeyDisplay(ui::VKEY_MENU));
  EXPECT_EQ(u"MediaPlay", ash::GetKeyDisplay(ui::VKEY_MEDIA_PLAY));
}

TEST_F(AcceleratorConfigurationProviderTest, NonConfigurableActions) {
  FakeAcceleratorsUpdatedMojoObserver mojo_observer;
  SetUpObserver(&mojo_observer);
  base::RunLoop().RunUntilIdle();
  auto config = mojo_observer.config();
  for (const auto& [id, accel_infos] :
       config[mojom::AcceleratorSource::kAmbient]) {
    for (const auto& info : accel_infos) {
      if (info->layout_properties->is_standard_accelerator()) {
        bool found_match = false;
        for (const auto& expected_data : GetAcceleratorsForAction(id)) {
          found_match = CompareAccelerators(expected_data, mojo::Clone(info));
          if (found_match) {
            break;
          }
        }
        // Matching Accelerator was found.
        EXPECT_TRUE(found_match);
      } else {
        const auto& text_accel =
            info->layout_properties->get_text_accelerator()->parts;
        if (!TextAccelContainsReplacements(id)) {
          // Ambient accelerators that contain no replacements e.g., Drag the
          // link to the tab's address bar
          EXPECT_EQ(text_accel[0]->text,
                    l10n_util::GetStringUTF16(GetMessageIdForTextAccel(id)));
          continue;
        }
        // We're only concerned with validating the replacements
        // (keys/modifiers). Validating the plain text parts is handled by the
        // paramaterized tests below.
        const auto& text_accel_parts = RemovePlainTextParts(text_accel);
        const auto& replacement_parts = GetReplacementsForAction(id);
        for (size_t i = 0; i < replacement_parts.size(); i++) {
          ValidateTextAccelerators(replacement_parts[i], text_accel_parts[i]);
        }
      }
    }
  }
}

// Tests that standard non-configurable look up is correctly configured and
// matches the predefined non-configurable list.
TEST_F(AcceleratorConfigurationProviderTest, NonConfigurableLookup) {
  base::RunLoop().RunUntilIdle();
  for (const auto& [ambient_action_id, accelerators_details] :
       non_configurable_actions_map_) {
    // Only standard accelerators are present in the lookup maps.
    if (accelerators_details.IsStandardAccelerator()) {
      std::vector<ui::Accelerator> actual_accelerators =
          GetNonConfigurableAcceleratorsForActionId(
              static_cast<uint32_t>(ambient_action_id));
      EXPECT_TRUE(base::ranges::is_permutation(
          actual_accelerators, accelerators_details.accelerators.value()));
    }
  }
}

// Tests that standard non-configurable reverse look up is correctly configured
// and matches the predefined non-configurable list.
TEST_F(AcceleratorConfigurationProviderTest, NonConfigurableReverseLookup) {
  base::RunLoop().RunUntilIdle();
  for (const auto& [ambient_action_id, accelerators_details] :
       non_configurable_actions_map_) {
    // Only standard accelerators are present in the lookup maps.
    if (accelerators_details.IsStandardAccelerator()) {
      for (const auto& accelerator :
           accelerators_details.accelerators.value()) {
        const uint32_t found_id =
            GetNonConfigurableIdFromAccelerator(accelerator);
        EXPECT_EQ(ambient_action_id, found_id);
      }
    }
  }
}

TEST_F(AcceleratorConfigurationProviderTest, RemoveAccelerator) {
  FakeAcceleratorsUpdatedMojoObserver observer;
  SetUpObserver(&observer);

  // Initialize with all custom accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToNextIme},
  };
  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  // Verify accelerators are populated.
  EXPECT_EQ(sizeof(test_data) / sizeof(AcceleratorData),
            config->GetAllAccelerators().size());

  // Remove the accelerator.
  provider_->RemoveAccelerator(
      mojom::AcceleratorSource::kAsh, AcceleratorAction::kSwitchToNextIme,
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN),
      base::BindLambdaForTesting([&](AcceleratorResultDataPtr result) {
        EXPECT_EQ(AcceleratorConfigResult::kSuccess, result->result);
        EXPECT_FALSE(result->shortcut_name.has_value());
        // Verify the accelerator was removed.
        std::vector<ui::Accelerator> updated_accelerators =
            config->GetAllAccelerators();
        EXPECT_EQ(0u, updated_accelerators.size());

        // Now verify that removing the default for
        // `AcceleratorAction::kSwitchToNextIme` will only disable it from the
        // config.
        base::RunLoop().RunUntilIdle();
        AcceleratorConfigurationProvider::AcceleratorConfigurationMap
            actual_config = observer.config();
        ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh, test_data,
                                     actual_config);
        std::vector<mojom::AcceleratorInfoPtr> actual_infos(
            mojo::Clone(actual_config[mojom::AcceleratorSource::kAsh]
                                     [AcceleratorAction::kSwitchToNextIme]));
        EXPECT_EQ(1u, actual_infos.size());
        // A disabled default accelerator should be marked as `kDisabledByUser`.
        EXPECT_EQ(mojom::AcceleratorState::kDisabledByUser,
                  actual_infos[0]->state);
        EXPECT_EQ(mojom::AcceleratorType::kDefault, actual_infos[0]->type);
      }));
  base::RunLoop().RunUntilIdle();
}

TEST_F(AcceleratorConfigurationProviderTest, RemoveAcceleratorThatDoesntExist) {
  // Initialize with all custom accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_ALT_DOWN,
       AcceleratorAction::kSwapPrimaryDisplay},
  };
  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  // Remove the accelerator.
  provider_->RemoveAccelerator(
      mojom::AcceleratorSource::kAsh, AcceleratorAction::kToggleMirrorMode,
      ui::Accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN),
      base::BindLambdaForTesting([&](AcceleratorResultDataPtr result) {
        EXPECT_EQ(AcceleratorConfigResult::kNotFound, result->result);
        EXPECT_FALSE(result->shortcut_name.has_value());
      }));
  base::RunLoop().RunUntilIdle();
}

TEST_F(AcceleratorConfigurationProviderTest, RemoveAcceleratorNonAsh) {
  // Initialize with all custom accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_ALT_DOWN,
       AcceleratorAction::kSwapPrimaryDisplay},
  };
  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  // Remove the accelerator.
  provider_->RemoveAccelerator(
      mojom::AcceleratorSource::kBrowser, AcceleratorAction::kToggleMirrorMode,
      ui::Accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN),
      base::BindLambdaForTesting([&](AcceleratorResultDataPtr result) {
        EXPECT_EQ(AcceleratorConfigResult::kActionLocked, result->result);
        EXPECT_FALSE(result->shortcut_name.has_value());
      }));
  base::RunLoop().RunUntilIdle();
}

TEST_F(AcceleratorConfigurationProviderTest, RemoveAndRestoreAllDefaults) {
  FakeAcceleratorsUpdatedMojoObserver observer;
  SetUpObserver(&observer);

  // Initialize with all custom accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToNextIme},
  };
  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);
  config->InitializeDeprecatedAccelerators({}, {});
  base::RunLoop().RunUntilIdle();

  // Verify accelerators are populated.
  EXPECT_EQ(sizeof(test_data) / sizeof(AcceleratorData),
            config->GetAllAccelerators().size());

  AcceleratorResultDataPtr result;
  // Remove the accelerator.
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .RemoveAccelerator(
              mojom::AcceleratorSource::kAsh,
              AcceleratorAction::kSwitchToNextIme,
              ui::Accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN), &result);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result->result);
  EXPECT_FALSE(result->shortcut_name.has_value());
  // Verify the accelerator was removed.
  std::vector<ui::Accelerator> updated_accelerators =
      config->GetAllAccelerators();
  EXPECT_EQ(0u, updated_accelerators.size());

  // Now verify that removing the default for
  // `AcceleratorAction::kSwitchToNextIme` will only disable it from the config.
  base::RunLoop().RunUntilIdle();
  AcceleratorConfigurationProvider::AcceleratorConfigurationMap actual_config =
      observer.config();
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh, test_data,
                               actual_config);
  std::vector<mojom::AcceleratorInfoPtr> actual_infos(
      mojo::Clone(actual_config[mojom::AcceleratorSource::kAsh]
                               [AcceleratorAction::kSwitchToNextIme]));
  EXPECT_EQ(1u, actual_infos.size());
  // A disabled default accelerator should be marked as `kDisabledByUser`.
  EXPECT_EQ(mojom::AcceleratorState::kDisabledByUser, actual_infos[0]->state);
  EXPECT_EQ(mojom::AcceleratorType::kDefault, actual_infos[0]->type);

  // Restore all defaults.
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .RestoreAllDefaults(&result);
  EXPECT_EQ(mojom::AcceleratorConfigResult::kSuccess, result->result);

  base::RunLoop().RunUntilIdle();

  // Verify accelerators were restored.
  updated_accelerators = config->GetAllAccelerators();
  EXPECT_EQ(1u, updated_accelerators.size());

  // Verify that the accelerator is back to their default states.
  actual_config = observer.config();
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh, test_data,
                               actual_config);
  actual_infos =
      mojo::Clone(actual_config[mojom::AcceleratorSource::kAsh]
                               [AcceleratorAction::kSwitchToNextIme]);
  EXPECT_EQ(1u, actual_infos.size());
  // Resetting to default will reset it back to `kEnabled`.
  EXPECT_EQ(mojom::AcceleratorState::kEnabled, actual_infos[0]->state);
  EXPECT_EQ(mojom::AcceleratorType::kDefault, actual_infos[0]->type);
}

TEST_F(AcceleratorConfigurationProviderTest, RemoveAndResoreDefault) {
  FakeAcceleratorsUpdatedMojoObserver observer;
  SetUpObserver(&observer);

  // Initialize with all custom accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
  };
  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  // Verify accelerators are populated.
  EXPECT_EQ(sizeof(test_data) / sizeof(AcceleratorData),
            config->GetAllAccelerators().size());

  AcceleratorResultDataPtr result;
  // Remove the accelerator.
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .RemoveAccelerator(
              mojom::AcceleratorSource::kAsh,
              AcceleratorAction::kToggleMirrorMode,
              ui::Accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN), &result);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result->result);
  EXPECT_FALSE(result->shortcut_name.has_value());
  // Verify the accelerator was removed.
  std::vector<ui::Accelerator> updated_accelerators =
      config->GetAllAccelerators();
  EXPECT_EQ(0u, updated_accelerators.size());

  // Now verify that removing the default for
  // `AcceleratorAction::kToggleMirrorMode` will only disable it from the
  // config.
  base::RunLoop().RunUntilIdle();
  AcceleratorConfigurationProvider::AcceleratorConfigurationMap actual_config =
      observer.config();
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh, test_data,
                               actual_config);
  std::vector<mojom::AcceleratorInfoPtr> actual_infos(
      mojo::Clone(actual_config[mojom::AcceleratorSource::kAsh]
                               [AcceleratorAction::kToggleMirrorMode]));
  EXPECT_EQ(1u, actual_infos.size());
  // A disabled default accelerator should be marked as `kDisabledByUser`.
  EXPECT_EQ(mojom::AcceleratorState::kDisabledByUser, actual_infos[0]->state);
  EXPECT_EQ(mojom::AcceleratorType::kDefault, actual_infos[0]->type);

  // Restore all defaults.
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .RestoreDefault(mojom::AcceleratorSource::kAsh,
                          AcceleratorAction::kToggleMirrorMode, &result);

  base::RunLoop().RunUntilIdle();

  // Verify the accelerator was restored.
  updated_accelerators = config->GetAllAccelerators();
  EXPECT_EQ(1u, updated_accelerators.size());

  // Verify that the accelerator is back to their default states.
  actual_config = observer.config();
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh, test_data,
                               actual_config);
  actual_infos =
      mojo::Clone(actual_config[mojom::AcceleratorSource::kAsh]
                               [AcceleratorAction::kToggleMirrorMode]);
  EXPECT_EQ(1u, actual_infos.size());
  // Resetting to default will reset it back to `kEnabled`.
  EXPECT_EQ(mojom::AcceleratorState::kEnabled, actual_infos[0]->state);
  EXPECT_EQ(mojom::AcceleratorType::kDefault, actual_infos[0]->type);
}

TEST_F(AcceleratorConfigurationProviderTest, RestoreDefaultNonAsh) {
  // Initialize with all custom accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_ALT_DOWN,
       AcceleratorAction::kSwapPrimaryDisplay},
  };
  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  // Remove the accelerator.
  provider_->RestoreDefault(
      mojom::AcceleratorSource::kBrowser, AcceleratorAction::kToggleMirrorMode,
      base::BindLambdaForTesting([&](AcceleratorResultDataPtr result) {
        EXPECT_EQ(AcceleratorConfigResult::kActionLocked, result->result);
        EXPECT_FALSE(result->shortcut_name.has_value());
      }));
  base::RunLoop().RunUntilIdle();
}

TEST_F(AcceleratorConfigurationProviderTest, AddAcceleratorBadSource) {
  // Initialize default accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
  };

  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  AcceleratorResultDataPtr result;
  const ui::Accelerator accelerator(ui::VKEY_M, ui::EF_COMMAND_DOWN);
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .AddAccelerator(mojom::AcceleratorSource::kBrowser, /*action_id=*/0,
                          accelerator, &result);
  EXPECT_EQ(mojom::AcceleratorConfigResult::kActionLocked, result->result);
}

TEST_F(AcceleratorConfigurationProviderTest, AddSameAccelerator) {
  // Initialize default accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
  };

  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  AcceleratorResultDataPtr result;
  const ui::Accelerator accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .AddAccelerator(mojom::AcceleratorSource::kAsh,
                          AcceleratorAction::kToggleMirrorMode, accelerator,
                          &result);
  EXPECT_EQ(mojom::AcceleratorConfigResult::kConflict, result->result);
}

TEST_F(AcceleratorConfigurationProviderTest, AddAcceleratorBadAccelerator) {
  // Initialize default accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
  };

  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  AcceleratorResultDataPtr result;
  // Missing modifier results in an error.
  const ui::Accelerator accelerator(ui::VKEY_M, ui::EF_NONE);
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .AddAccelerator(mojom::AcceleratorSource::kAsh,
                          AcceleratorAction::kToggleMirrorMode, accelerator,
                          &result);
  EXPECT_EQ(mojom::AcceleratorConfigResult::kMissingModifier, result->result);

  // Shift as the only modifier is an error.
  const ui::Accelerator shift_only_accelerator(ui::VKEY_M, ui::EF_SHIFT_DOWN);
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .AddAccelerator(mojom::AcceleratorSource::kAsh,
                          AcceleratorAction::kToggleMirrorMode,
                          shift_only_accelerator, &result);
  EXPECT_EQ(mojom::AcceleratorConfigResult::kShiftOnlyNotAllowed,
            result->result);

  // Top-row key cannot be used as the key
  const ui::Accelerator top_row_accelerator(ui::VKEY_BRIGHTNESS_DOWN,
                                            ui::EF_CONTROL_DOWN);
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .AddAccelerator(mojom::AcceleratorSource::kAsh,
                          AcceleratorAction::kToggleMirrorMode,
                          top_row_accelerator, &result);
  EXPECT_EQ(mojom::AcceleratorConfigResult::kKeyNotAllowed, result->result);
}

TEST_F(AcceleratorConfigurationProviderTest, AddAcceleratorExceedsMaximum) {
  // Initialize default accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_A, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_S, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_D, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_F, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
  };

  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  AcceleratorResultDataPtr result;
  // Attempting to add a 6th accelerator will result in an error.
  const ui::Accelerator accelerator(ui::VKEY_M, ui::EF_CONTROL_DOWN);
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .AddAccelerator(mojom::AcceleratorSource::kAsh,
                          AcceleratorAction::kToggleMirrorMode, accelerator,
                          &result);
  EXPECT_EQ(mojom::AcceleratorConfigResult::kMaximumAcceleratorsReached,
            result->result);
}

TEST_F(AcceleratorConfigurationProviderTest, ReservedKeysNotAllowed) {
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       kToggleMirrorMode},
  };

  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  AcceleratorResultDataPtr result;
  // Power key.
  const ui::Accelerator power_accelerator(ui::VKEY_POWER, ui::EF_COMMAND_DOWN);
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .AddAccelerator(mojom::AcceleratorSource::kAsh, kToggleMirrorMode,
                          power_accelerator, &result);
  EXPECT_EQ(mojom::AcceleratorConfigResult::kKeyNotAllowed, result->result);

  // Sleep key.
  const ui::Accelerator sleep_accelerator(ui::VKEY_SLEEP, ui::EF_COMMAND_DOWN);
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .AddAccelerator(mojom::AcceleratorSource::kAsh, kToggleMirrorMode,
                          sleep_accelerator, &result);
  EXPECT_EQ(mojom::AcceleratorConfigResult::kKeyNotAllowed, result->result);

  // Lock/f13 key.
  const ui::Accelerator lock_accelerator(ui::VKEY_F13, ui::EF_COMMAND_DOWN);
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .AddAccelerator(mojom::AcceleratorSource::kAsh, kToggleMirrorMode,
                          lock_accelerator, &result);
  EXPECT_EQ(mojom::AcceleratorConfigResult::kKeyNotAllowed, result->result);
}

TEST_F(AcceleratorConfigurationProviderTest, AddAcceleratorNonConfigConflict) {
  // Initialize default accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
  };

  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);

  // Ctrl + H is used by the Browser Shortcut, Open History Page.
  const ui::Accelerator accelerator(ui::VKEY_H, ui::EF_CONTROL_DOWN);
  NonConfigurableActionsMap non_config_map = {
      {NonConfigurableActions::kBrowserShowHistory,
       NonConfigurableAcceleratorDetails({accelerator})}};

  base::RunLoop().RunUntilIdle();

  AcceleratorResultDataPtr result;
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .AddAccelerator(mojom::AcceleratorSource::kAsh,
                          AcceleratorAction::kToggleMirrorMode, accelerator,
                          &result);
  EXPECT_EQ(mojom::AcceleratorConfigResult::kConflict, result->result);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_BROWSER_ACCELERATOR_DESCRIPTION_SHOW_HISTORY),
            result->shortcut_name);
}

TEST_F(AcceleratorConfigurationProviderTest, AddAcceleratorNoConflict) {
  FakeAcceleratorsUpdatedMojoObserver observer;
  SetUpObserver(&observer);

  // Initialize default accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
  };

  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);

  // Override non-configurable accelerators.
  const ui::Accelerator browser_accelerator(ui::VKEY_H, ui::EF_CONTROL_DOWN);
  NonConfigurableActionsMap non_config_map = {
      {NonConfigurableActions::kBrowserShowHistory,
       NonConfigurableAcceleratorDetails({browser_accelerator})}};

  base::RunLoop().RunUntilIdle();

  const ui::Accelerator good_accelerator(ui::VKEY_M, ui::EF_COMMAND_DOWN);
  AcceleratorResultDataPtr result;
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .AddAccelerator(mojom::AcceleratorSource::kAsh,
                          AcceleratorAction::kToggleMirrorMode,
                          good_accelerator, &result);

  EXPECT_EQ(mojom::AcceleratorConfigResult::kSuccess, result->result);

  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_M, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kToggleMirrorMode},
  };
  AcceleratorConfigurationProvider::AcceleratorConfigurationMap actual_config =
      observer.config();
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh,
                               updated_test_data, actual_config);
}

TEST_F(AcceleratorConfigurationProviderTest, AddAcceleratorConflictLocked) {
  // Initialize default accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_M, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kOpenCrosh},
  };

  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);
  SetLayoutDetailsMap(
      {{AcceleratorAction::kOpenCrosh,
        IDS_ASH_ACCELERATOR_DESCRIPTION_OPEN_CROSH,
        mojom::AcceleratorCategory::kGeneral,
        mojom::AcceleratorSubcategory::kGeneralControls,
        /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
        mojom::AcceleratorSource::kAsh}});

  // Override non-configurable accelerators.
  const ui::Accelerator browser_accelerator(ui::VKEY_H, ui::EF_CONTROL_DOWN);
  NonConfigurableActionsMap non_config_map = {
      {NonConfigurableActions::kBrowserShowHistory,
       NonConfigurableAcceleratorDetails({browser_accelerator})}};

  base::RunLoop().RunUntilIdle();

  const ui::Accelerator conflict_accelerator(ui::VKEY_M, ui::EF_COMMAND_DOWN);
  AcceleratorResultDataPtr result;
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .AddAccelerator(mojom::AcceleratorSource::kAsh,
                          AcceleratorAction::kToggleMirrorMode,
                          conflict_accelerator, &result);

  EXPECT_EQ(mojom::AcceleratorConfigResult::kActionLocked, result->result);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_ACCELERATOR_DESCRIPTION_OPEN_CROSH),
      result->shortcut_name);
}

TEST_F(AcceleratorConfigurationProviderTest, AddAcceleratorConflictReset) {
  FakeAcceleratorsUpdatedMojoObserver observer;
  SetUpObserver(&observer);

  // Initialize default accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_M, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kOpenCrosh},
  };

  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);

  base::RunLoop().RunUntilIdle();

  const ui::Accelerator conflict_accelerator(ui::VKEY_M, ui::EF_COMMAND_DOWN);
  AcceleratorResultDataPtr result;
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .AddAccelerator(mojom::AcceleratorSource::kAsh,
                          AcceleratorAction::kToggleMirrorMode,
                          conflict_accelerator, &result);

  EXPECT_EQ(mojom::AcceleratorConfigResult::kConflictCanOverride,
            result->result);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_ACCELERATOR_DESCRIPTION_OPEN_CROSH),
      result->shortcut_name);

  AcceleratorConfigurationProvider::AcceleratorConfigurationMap actual_config =
      observer.config();
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh, test_data,
                               actual_config);

  // Simulate the user is no longer capturing the input, expect that re-pressing
  // the accelerato will still give the same error
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .PreventProcessingAccelerators(false);

  // Press the same accelerator, expect the same error.
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .AddAccelerator(mojom::AcceleratorSource::kAsh,
                          AcceleratorAction::kToggleMirrorMode,
                          conflict_accelerator, &result);

  EXPECT_EQ(mojom::AcceleratorConfigResult::kConflictCanOverride,
            result->result);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_ACCELERATOR_DESCRIPTION_OPEN_CROSH),
      result->shortcut_name);

  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh, test_data,
                               observer.config());
}

TEST_F(AcceleratorConfigurationProviderTest, AddAcceleratorConflictOverride) {
  FakeAcceleratorsUpdatedMojoObserver observer;
  SetUpObserver(&observer);

  // Initialize default accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_M, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kOpenCrosh},
  };

  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);

  // Override non-configurable accelerators.
  const ui::Accelerator browser_accelerator(ui::VKEY_H, ui::EF_CONTROL_DOWN);
  NonConfigurableActionsMap non_config_map = {
      {NonConfigurableActions::kBrowserShowHistory,
       NonConfigurableAcceleratorDetails({browser_accelerator})}};

  base::RunLoop().RunUntilIdle();

  const ui::Accelerator conflict_accelerator(ui::VKEY_M, ui::EF_COMMAND_DOWN);
  AcceleratorResultDataPtr result;
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .AddAccelerator(mojom::AcceleratorSource::kAsh,
                          AcceleratorAction::kToggleMirrorMode,
                          conflict_accelerator, &result);

  EXPECT_EQ(mojom::AcceleratorConfigResult::kConflictCanOverride,
            result->result);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_ACCELERATOR_DESCRIPTION_OPEN_CROSH),
      result->shortcut_name);

  AcceleratorConfigurationProvider::AcceleratorConfigurationMap actual_config =
      observer.config();
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh, test_data,
                               actual_config);

  // Now override the accelerator.
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .AddAccelerator(mojom::AcceleratorSource::kAsh,
                          AcceleratorAction::kToggleMirrorMode,
                          conflict_accelerator, &result);

  EXPECT_EQ(mojom::AcceleratorConfigResult::kSuccess, result->result);

  // Since this is an overridable accelerator, nothing should change at first.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_M, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kToggleMirrorMode},
  };

  actual_config = observer.config();
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh,
                               updated_test_data, actual_config);
}

TEST_F(AcceleratorConfigurationProviderTest, ReplaceBadSourceOrAction) {
  FakeAcceleratorsUpdatedMojoObserver observer;
  SetUpObserver(&observer);

  // Initialize default accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
  };

  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  AcceleratorResultDataPtr result;
  const ui::Accelerator old_accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  const ui::Accelerator new_accelerator(ui::VKEY_M, ui::EF_ALT_DOWN);
  // Browser is a locked source.
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .ReplaceAccelerator(mojom::AcceleratorSource::kBrowser,
                              AcceleratorAction::kToggleMirrorMode,
                              old_accelerator, new_accelerator, &result);
  EXPECT_EQ(mojom::AcceleratorConfigResult::kActionLocked, result->result);

  // AcceleratorAction::kToggleCalendar does not exist.
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .ReplaceAccelerator(mojom::AcceleratorSource::kAsh,
                              AcceleratorAction::kToggleCalendar,
                              old_accelerator, new_accelerator, &result);
  EXPECT_EQ(mojom::AcceleratorConfigResult::kNotFound, result->result);
}

TEST_F(AcceleratorConfigurationProviderTest, AddAcceleratorConflictThenGood) {
  FakeAcceleratorsUpdatedMojoObserver observer;
  SetUpObserver(&observer);

  // Initialize default accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_M, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kOpenCrosh},
  };

  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);

  // Override non-configurable accelerators.
  const ui::Accelerator browser_accelerator(ui::VKEY_H, ui::EF_CONTROL_DOWN);
  NonConfigurableActionsMap non_config_map = {
      {NonConfigurableActions::kBrowserShowHistory,
       NonConfigurableAcceleratorDetails({browser_accelerator})}};

  base::RunLoop().RunUntilIdle();

  const ui::Accelerator conflict_accelerator(ui::VKEY_M, ui::EF_COMMAND_DOWN);
  AcceleratorResultDataPtr result;
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .AddAccelerator(mojom::AcceleratorSource::kAsh,
                          AcceleratorAction::kToggleMirrorMode,
                          conflict_accelerator, &result);

  EXPECT_EQ(mojom::AcceleratorConfigResult::kConflictCanOverride,
            result->result);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_ACCELERATOR_DESCRIPTION_OPEN_CROSH),
      result->shortcut_name);

  AcceleratorConfigurationProvider::AcceleratorConfigurationMap actual_config =
      observer.config();
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh, test_data,
                               actual_config);

  // Now use a non-conflicting accelerator.
  const ui::Accelerator good_accelerator(ui::VKEY_K, ui::EF_COMMAND_DOWN);
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .AddAccelerator(mojom::AcceleratorSource::kAsh,
                          AcceleratorAction::kToggleMirrorMode,
                          good_accelerator, &result);

  EXPECT_EQ(mojom::AcceleratorConfigResult::kSuccess, result->result);

  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_K, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_M, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kOpenCrosh},
  };

  actual_config = observer.config();
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh,
                               updated_test_data, actual_config);
}

TEST_F(AcceleratorConfigurationProviderTest, PreventProcessingAccelerators) {
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .PreventProcessingAccelerators(true);
  EXPECT_TRUE(Shell::Get()
                  ->accelerator_controller()
                  ->ShouldPreventProcessingAccelerators());

  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .PreventProcessingAccelerators(false);
  EXPECT_FALSE(Shell::Get()
                   ->accelerator_controller()
                   ->ShouldPreventProcessingAccelerators());
}

TEST_F(AcceleratorConfigurationProviderTest, ReplaceDefaultAccelerator) {
  FakeAcceleratorsUpdatedMojoObserver observer;
  SetUpObserver(&observer);

  // Initialize default accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
  };

  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  AcceleratorResultDataPtr result;
  const ui::Accelerator old_accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  const ui::Accelerator new_accelerator(ui::VKEY_M, ui::EF_ALT_DOWN);
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .ReplaceAccelerator(mojom::AcceleratorSource::kAsh,
                              AcceleratorAction::kToggleMirrorMode,
                              old_accelerator, new_accelerator, &result);
  EXPECT_EQ(mojom::AcceleratorConfigResult::kSuccess, result->result);

  base::RunLoop().RunUntilIdle();

  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_M, ui::EF_ALT_DOWN,
       AcceleratorAction::kToggleMirrorMode},
  };

  // Replacing a default will result in disabling the default and then adding
  // the new accelerator.
  AcceleratorConfigurationProvider::AcceleratorConfigurationMap actual_config =
      observer.config();
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh,
                               updated_test_data, mojo::Clone(actual_config));

  std::vector<mojom::AcceleratorInfoPtr> actual_infos(
      mojo::Clone(actual_config[mojom::AcceleratorSource::kAsh]
                               [AcceleratorAction::kToggleMirrorMode]));
  EXPECT_EQ(2u, actual_infos.size());
  // A disabled default accelerator should be marked as `kDisabledByUser`.
  EXPECT_EQ(mojom::AcceleratorState::kDisabledByUser, actual_infos[0]->state);
  EXPECT_EQ(mojom::AcceleratorType::kDefault, actual_infos[0]->type);
}

TEST_F(AcceleratorConfigurationProviderTest, ReplaceAcceleratorDoesNotExist) {
  FakeAcceleratorsUpdatedMojoObserver observer;
  SetUpObserver(&observer);

  // Initialize default accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
  };

  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  AcceleratorResultDataPtr result;
  const ui::Accelerator old_accelerator(ui::VKEY_J, ui::EF_CONTROL_DOWN);
  const ui::Accelerator new_accelerator(ui::VKEY_M, ui::EF_ALT_DOWN);
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .ReplaceAccelerator(mojom::AcceleratorSource::kAsh,
                              AcceleratorAction::kToggleMirrorMode,
                              old_accelerator, new_accelerator, &result);
  EXPECT_EQ(mojom::AcceleratorConfigResult::kNotFound, result->result);
}

TEST_F(AcceleratorConfigurationProviderTest, AddThenReplaceAccelerator) {
  FakeAcceleratorsUpdatedMojoObserver observer;
  SetUpObserver(&observer);

  // Initialize default accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
  };

  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  AcceleratorResultDataPtr result;
  const ui::Accelerator accelerator(ui::VKEY_M, ui::EF_ALT_DOWN);
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .AddAccelerator(mojom::AcceleratorSource::kAsh,
                          AcceleratorAction::kToggleMirrorMode, accelerator,
                          &result);
  EXPECT_EQ(mojom::AcceleratorConfigResult::kSuccess, result->result);

  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_M, ui::EF_ALT_DOWN,
       AcceleratorAction::kToggleMirrorMode},
  };

  AcceleratorConfigurationProvider::AcceleratorConfigurationMap actual_config =
      observer.config();
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh,
                               updated_test_data, mojo::Clone(actual_config));

  // Now replace the newly added accelerator.
  const ui::Accelerator new_accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN);
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .ReplaceAccelerator(mojom::AcceleratorSource::kAsh,
                              AcceleratorAction::kToggleMirrorMode, accelerator,
                              new_accelerator, &result);
  EXPECT_EQ(mojom::AcceleratorConfigResult::kSuccess, result->result);

  const AcceleratorData updated_test_data2[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_C, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kToggleMirrorMode},
  };

  actual_config = observer.config();
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh,
                               updated_test_data2, mojo::Clone(actual_config));
}

TEST_F(AcceleratorConfigurationProviderTest,
       ReplaceOtherDefaultAcceleratorAction) {
  FakeAcceleratorsUpdatedMojoObserver observer;
  SetUpObserver(&observer);

  // Initialize default accelerators.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_D, ui::EF_ALT_DOWN,
       AcceleratorAction::kToggleCalendar},
  };

  AshAcceleratorConfiguration* config =
      Shell::Get()->ash_accelerator_configuration();
  config->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  // Replace the default of `AcceleratorAction::kToggleCalendar` with that of
  // the default of `AcceleratorAction::kToggleMirrorMode`. This results in
  // `AcceleratorAction::kToggleMirrorMode` to have only the disabled default
  // accelerator, `AcceleratorAction::kToggleCalendar` will also have disabled
  // default accelerator but also a new accelerator added.
  AcceleratorResultDataPtr result;
  const ui::Accelerator old_accelerator(ui::VKEY_D, ui::EF_ALT_DOWN);
  const ui::Accelerator new_accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .ReplaceAccelerator(mojom::AcceleratorSource::kAsh,
                              AcceleratorAction::kToggleCalendar,
                              old_accelerator, new_accelerator, &result);
  // Overridable accelerator, but will need to re-call `ReplaceAccelerator` to
  // confirm the override.
  EXPECT_EQ(mojom::AcceleratorConfigResult::kConflictCanOverride,
            result->result);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_ACCELERATOR_DESCRIPTION_TOGGLE_MIRROR_MODE),
            result->shortcut_name);

  ash::shortcut_customization::mojom::
      AcceleratorConfigurationProviderAsyncWaiter(provider_.get())
          .ReplaceAccelerator(mojom::AcceleratorSource::kAsh,
                              AcceleratorAction::kToggleCalendar,
                              old_accelerator, new_accelerator, &result);

  EXPECT_EQ(mojom::AcceleratorConfigResult::kSuccess, result->result);

  base::RunLoop().RunUntilIdle();

  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_D, ui::EF_ALT_DOWN,
       AcceleratorAction::kToggleCalendar},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleCalendar},
  };

  AcceleratorConfigurationProvider::AcceleratorConfigurationMap actual_config =
      observer.config();
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh,
                               updated_test_data, mojo::Clone(actual_config));

  std::vector<mojom::AcceleratorInfoPtr> actual_infos(
      mojo::Clone(actual_config[mojom::AcceleratorSource::kAsh]
                               [AcceleratorAction::kToggleMirrorMode]));
  EXPECT_EQ(1u, actual_infos.size());
  EXPECT_EQ(mojom::AcceleratorState::kDisabledByUser, actual_infos[0]->state);
  EXPECT_EQ(mojom::AcceleratorType::kDefault, actual_infos[0]->type);

  std::vector<mojom::AcceleratorInfoPtr> actual_infos2(
      mojo::Clone(actual_config[mojom::AcceleratorSource::kAsh]
                               [AcceleratorAction::kToggleCalendar]));
  EXPECT_EQ(2u, actual_infos2.size());
  EXPECT_EQ(mojom::AcceleratorState::kDisabledByUser, actual_infos2[0]->state);
  EXPECT_EQ(mojom::AcceleratorType::kDefault, actual_infos2[0]->type);
  EXPECT_EQ(mojom::AcceleratorState::kEnabled, actual_infos2[1]->state);
  EXPECT_EQ(mojom::AcceleratorType::kDefault, actual_infos2[1]->type);
}

TEST_F(AcceleratorConfigurationProviderTest,
       VerifyAllAcceleratorsHaveKeyString) {
  // The following is a set of VKEYs that are ignored in this test. If there is
  // a keycode that should be ignored, please add it to the following set. Only
  // do this if the keycode does not require an icon, otherwise there is a risk
  // that the shortcut app will display an empty icon for the accelerator.
  std::set<ui::KeyboardCode> ignore_list = {
      ui::KeyboardCode::VKEY_LSHIFT,  // kDisableCapsLock
      ui::KeyboardCode::VKEY_RSHIFT,  // kDisableCapsLock
  };

  AshAcceleratorConfiguration* ash_config =
      Shell::Get()->ash_accelerator_configuration();
  ash_config->Initialize();
  base::RunLoop().RunUntilIdle();

  AcceleratorConfigurationProvider::AcceleratorConfigurationMap config =
      provider_->GetAcceleratorConfig();
  // Iterate through the entire config and check that all accelerators have a
  // non-empty key string.
  for (const auto& [source, id_to_accelerator_info] : config) {
    for (const auto& [action_id, accelerators] : id_to_accelerator_info) {
      for (const auto& accelerator : accelerators) {
        if (!accelerator->layout_properties->is_standard_accelerator()) {
          continue;
        }

        ui::KeyboardCode key_code =
            accelerator->layout_properties->get_standard_accelerator()
                ->accelerator.key_code();
        // Ignore accelerators that have ignored keycodes or are disabled.
        if (ignore_list.contains(key_code) ||
            accelerator->state ==
                mojom::AcceleratorState::kDisabledByUnavailableKeys) {
          continue;
        }

        EXPECT_FALSE(ash::GetKeyDisplay(key_code).empty())
            << "Missing display string for the keycode: " << key_code
            << " if you are adding a new accelerator, "
            << "please add a new mapping to `GetKeyDisplayMap()` in "
            << "ash/webui/shortcut_customization_ui/backend/"
            << "accelerator_layout_table.h along with the corresponding icon. "
            << "See examples at "
            << "ash/webui/shortcut_customization_ui/resources/js/input_key.ts.";
      }
    }
  }
}

using FlagsKeyboardCodesVariant =
    std::variant<ui::EventFlags, ui::KeyboardCode, TextAcceleratorDelimiter>;
using FlagsKeyboardCodeStringVariant = std::variant<ui::EventFlags,
                                                    ui::KeyboardCode,
                                                    std::u16string,
                                                    TextAcceleratorDelimiter>;

class TextAcceleratorParsingTest
    : public AcceleratorConfigurationProviderTest,
      public testing::WithParamInterface<
          std::tuple<std::u16string,
                     std::vector<FlagsKeyboardCodesVariant>,
                     std::vector<FlagsKeyboardCodeStringVariant>>> {
 public:
  void SetUp() override {
    AcceleratorConfigurationProviderTest::SetUp();
    std::vector<FlagsKeyboardCodesVariant> replacements_parts;
    std::vector<FlagsKeyboardCodeStringVariant> variants;
    std::tie(replacement_string_, replacements_parts, variants) = GetParam();

    for (const auto& r : replacements_parts) {
      if (std::holds_alternative<ui::KeyboardCode>(r)) {
        replacements_.emplace_back(std::get<ui::KeyboardCode>(r));
      } else if (std::holds_alternative<ui::EventFlags>(r)) {
        replacements_.emplace_back(std::get<ui::EventFlags>(r));
      } else {
        replacements_.emplace_back(std::get<TextAcceleratorDelimiter>(r));
      }
    }

    for (const auto& v : variants) {
      if (std::holds_alternative<std::u16string>(v)) {
        expected_parts_.emplace_back(std::get<std::u16string>(v));
      } else if (std::holds_alternative<ui::KeyboardCode>(v)) {
        expected_parts_.emplace_back(std::get<ui::KeyboardCode>(v));
      } else if (std::holds_alternative<ui::EventFlags>(v)) {
        expected_parts_.emplace_back(std::get<ui::EventFlags>(v));
      } else {
        expected_parts_.emplace_back(std::get<TextAcceleratorDelimiter>(v));
      }
    }
  }

 protected:
  std::u16string replacement_string_;
  std::vector<TextAcceleratorPart> replacements_;
  std::vector<TextAcceleratorPart> expected_parts_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    TextAcceleratorParsingTest,
    // The tuple contains an example replacement string, the replacements to
    // use in the string (keys, modifiers, plain text), and the result we
    // expect to receive given these inputs.
    testing::ValuesIn(
        std::vector<std::tuple<std::u16string,
                               std::vector<FlagsKeyboardCodesVariant>,
                               std::vector<FlagsKeyboardCodeStringVariant>>>{
            {
                u"$1 $2 $3 through $4",
                {ui::EF_CONTROL_DOWN, TextAcceleratorDelimiter::kPlusSign,
                 ui::VKEY_1, ui::VKEY_8},
                {ui::EF_CONTROL_DOWN, u" ", TextAcceleratorDelimiter::kPlusSign,
                 u" ", ui::VKEY_1, u" through ", ui::VKEY_8},
            },
            {
                u"Press $1 and $2",
                {ui::EF_CONTROL_DOWN, ui::VKEY_C},
                {u"Press ", ui::EF_CONTROL_DOWN, u" and ", ui::VKEY_C},
            },
            {
                u"Press $1 $2 $3",
                {ui::VKEY_A, ui::VKEY_B, ui::VKEY_C},
                {u"Press ", ui::VKEY_A, u" ", ui::VKEY_B, u" ", ui::VKEY_C},
            },
            {
                u"$1 $2 $3 Press",
                {ui::VKEY_A, ui::VKEY_B, ui::VKEY_C},
                {ui::VKEY_A, u" ", ui::VKEY_B, u" ", ui::VKEY_C, u" Press"},
            },
            {
                u"$1$2$3",
                {ui::VKEY_A, ui::VKEY_B, ui::VKEY_C},
                {ui::VKEY_A, ui::VKEY_B, ui::VKEY_C},
            },
            {
                u"$1 and $2",
                {ui::VKEY_A, ui::VKEY_B},
                {ui::VKEY_A, u" and ", ui::VKEY_B},
            },
            {
                u"A $1 $2 D",
                {ui::VKEY_B, ui::VKEY_C},
                {u"A ", ui::VKEY_B, u" ", ui::VKEY_C, u" D"},
            },
            {
                u"$1",
                {ui::VKEY_B},
                {ui::VKEY_B},
            },
            {
                u"$1 ",
                {ui::VKEY_B},
                {ui::VKEY_B, u" "},
            },
            {
                u" $1",
                {ui::VKEY_B},
                {u" ", ui::VKEY_B},
            },
            {
                u"$1",
                {TextAcceleratorDelimiter::kPlusSign},
                {TextAcceleratorDelimiter::kPlusSign},
            },
            {
                u"$1 ",
                {TextAcceleratorDelimiter::kPlusSign},
                {TextAcceleratorDelimiter::kPlusSign, u" "},
            },
            {
                u" $1",
                {TextAcceleratorDelimiter::kPlusSign},
                {u" ", TextAcceleratorDelimiter::kPlusSign},
            },
            {
                u"Drag the link to a blank area on the tab strip",
                {},
                {u"Drag the link to a blank area on the tab strip"},
            },
            {
                u"$1a$2$3bc",
                {ui::EF_SHIFT_DOWN, ui::VKEY_B, ui::VKEY_C},
                {ui::EF_SHIFT_DOWN, u"a", ui::VKEY_B, ui::VKEY_C, u"bc"},
            }}));

TEST_P(TextAcceleratorParsingTest, TextAcceleratorParsing) {
  auto& bundle = ui::ResourceBundle::GetSharedInstance();
  int FAKE_RESOURCE_ID = 1;
  bundle.OverrideLocaleStringResource(FAKE_RESOURCE_ID, replacement_string_);
  const auto text_accelerator = provider_->CreateTextAcceleratorProperties(
      {FAKE_RESOURCE_ID, replacements_});
  EXPECT_EQ(expected_parts_.size(), text_accelerator->parts.size());
  for (size_t i = 0; i < expected_parts_.size(); i++) {
    ValidateTextAccelerators(expected_parts_[i], text_accelerator->parts[i]);
  }
}

class GetPlainTextPartsTest : public AcceleratorConfigurationProviderTest,
                              public testing::WithParamInterface<
                                  std::tuple<std::u16string,
                                             std::vector<size_t>,
                                             std::vector<std::u16string>>> {
 public:
  void SetUp() override {
    AcceleratorConfigurationProviderTest::SetUp();
    std::tie(input_, offsets_, expected_output_) = GetParam();
  }

 protected:
  std::u16string input_;
  std::vector<size_t> offsets_;
  std::vector<std::u16string> expected_output_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    GetPlainTextPartsTest,
    // The tuple contains an example replacement string with the placeholders
    // removed, the expected list of offsets which must be sorted and contains
    // the start points of our replacements, and the plain text parts we expect
    // receive given these inputs.
    testing::ValuesIn(std::vector<std::tuple<std::u16string,
                                             std::vector<size_t>,
                                             std::vector<std::u16string>>>{

        {u"abc", {0, 1, 1}, {u"a", u"bc"}},
        {u"abc", {0, 1, 2}, {u"a", u"b", u"c"}},
        {u"a b", {0, 1}, {u"a", u" b"}},
        {u"a b", {0, 2}, {u"a ", u"b"}},
        {u"Press  and ", {6, 11}, {u"Press ", u" and "}},
        {u"", {0}, {}},
        {u"No replacements", {}, {u"No replacements"}},
        {u"a and bc", {0, 6}, {u"a and ", u"bc"}},

    }));

TEST_P(GetPlainTextPartsTest, GetPlainTextParts) {
  const auto parts = SplitStringOnOffsets(input_, offsets_);
  EXPECT_EQ(expected_output_, parts);
}
}  // namespace shortcut_ui

}  // namespace ash
