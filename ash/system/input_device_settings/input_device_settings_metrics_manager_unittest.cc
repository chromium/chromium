// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_metrics_manager.h"

#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"

namespace ash {

namespace {
constexpr int kExternalKeyboardId = 1;
constexpr int kExternalChromeOSKeyboardId = 2;
constexpr int kInternalKeyboardId = 3;

constexpr char kUser1[] = "user1@gmail.com";
constexpr char kUser2[] = "user2@gmail.com";
}  // namespace

class InputDeviceSettingsMetricsManagerTest : public AshTestBase {
 public:
  InputDeviceSettingsMetricsManagerTest() = default;
  InputDeviceSettingsMetricsManagerTest(
      const InputDeviceSettingsMetricsManagerTest&) = delete;
  InputDeviceSettingsMetricsManagerTest& operator=(
      const InputDeviceSettingsMetricsManagerTest&) = delete;
  ~InputDeviceSettingsMetricsManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    manager_ = std::make_unique<InputDeviceSettingsMetricsManager>();
  }

  void TearDown() override {
    manager_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<InputDeviceSettingsMetricsManager> manager_;
};

TEST_F(InputDeviceSettingsMetricsManagerTest, RecordsKeyboardSettings) {
  mojom::Keyboard keyboard_external;
  keyboard_external.id = kExternalKeyboardId;
  keyboard_external.is_external = true;
  keyboard_external.meta_key = mojom::MetaKey::kCommand;
  keyboard_external.settings = mojom::KeyboardSettings::New();
  auto& settings_external = *keyboard_external.settings;
  settings_external.top_row_are_fkeys = true;

  mojom::Keyboard keyboard_external_chromeos;
  keyboard_external_chromeos.id = kExternalChromeOSKeyboardId;
  keyboard_external_chromeos.is_external = true;
  keyboard_external_chromeos.meta_key = mojom::MetaKey::kSearch;
  keyboard_external_chromeos.settings = mojom::KeyboardSettings::New();
  auto& settings_external_chromeos = *keyboard_external_chromeos.settings;
  settings_external_chromeos.top_row_are_fkeys = false;

  mojom::Keyboard keyboard_internal;
  keyboard_internal.id = kInternalKeyboardId;
  keyboard_internal.is_external = false;
  keyboard_internal.settings = mojom::KeyboardSettings::New();
  auto& settings_internal = *keyboard_internal.settings;
  settings_internal.top_row_are_fkeys = true;

  // Initially expect no user preferences recorded.
  base::HistogramTester histogram_tester;
  manager_.get()->RecordKeyboardInitialMetrics(keyboard_external);

  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.External.TopRowAreFKeys.Initial",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.ExternalChromeOS.TopRowAreFKeys."
      "Initial",
      /*expected_count=*/0u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.TopRowAreFKeys.Initial",
      /*expected_count=*/0u);

  manager_.get()->RecordKeyboardInitialMetrics(keyboard_external_chromeos);

  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.External.TopRowAreFKeys.Initial",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.ExternalChromeOS.TopRowAreFKeys."
      "Initial",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.TopRowAreFKeys.Initial",
      /*expected_count=*/0u);

  manager_.get()->RecordKeyboardInitialMetrics(keyboard_internal);

  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.External.TopRowAreFKeys.Initial",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.ExternalChromeOS.TopRowAreFKeys."
      "Initial",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.TopRowAreFKeys.Initial",
      /*expected_count=*/1u);
}

TEST_F(InputDeviceSettingsMetricsManagerTest, RecordMetricOncePerKeyboard) {
  mojom::Keyboard keyboard_external;
  keyboard_external.id = kExternalKeyboardId;
  keyboard_external.is_external = true;
  keyboard_external.meta_key = mojom::MetaKey::kCommand;
  keyboard_external.settings = mojom::KeyboardSettings::New();
  auto& settings_external = *keyboard_external.settings;
  settings_external.top_row_are_fkeys = true;

  mojom::Keyboard keyboard_internal;
  keyboard_internal.id = kInternalKeyboardId;
  keyboard_internal.is_external = false;
  keyboard_internal.settings = mojom::KeyboardSettings::New();
  auto& settings_internal = *keyboard_internal.settings;
  settings_internal.top_row_are_fkeys = true;

  base::HistogramTester histogram_tester;
  SimulateUserLogin(kUser1);
  manager_.get()->RecordKeyboardInitialMetrics(keyboard_external);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.External.TopRowAreFKeys.Initial",
      /*expected_count=*/1u);

  manager_.get()->RecordKeyboardInitialMetrics(keyboard_internal);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.TopRowAreFKeys.Initial",
      /*expected_count=*/1u);

  // Call RecordKeyboardInitialMetrics with the same user and same keyboard,
  // ExpectTotalCount for Internal metric won't increase.
  manager_.get()->RecordKeyboardInitialMetrics(keyboard_internal);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.TopRowAreFKeys.Initial",
      /*expected_count=*/1u);

  // Call RecordKeyboardInitialMetrics with the different user but same
  // keyboard, ExpectTotalCount for Internal metric will increase.
  SimulateUserLogin(kUser2);
  manager_.get()->RecordKeyboardInitialMetrics(keyboard_internal);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.TopRowAreFKeys.Initial",
      /*expected_count=*/2u);
}

}  // namespace ash