// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/device_reporting_settings_lacros.h"

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "components/policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::IsNull;

namespace reporting {
namespace {

// Fake delegate that stubs out device settings retrieval for testing purposes.
class TestDelegate : public DeviceReportingSettingsLacros::Delegate {
 public:
  TestDelegate()
      : device_settings_owned_(crosapi::mojom::DeviceSettings::New()),
        device_settings_(device_settings_owned_.get()) {}
  TestDelegate(const TestDelegate& other) = delete;
  TestDelegate& operator=(const TestDelegate& other) = delete;
  ~TestDelegate() override = default;

  void RegisterObserverWithCrosApiClient(
      DeviceReportingSettingsLacros* const instance) override {
    device_reporting_settings_ = instance;
  }

  crosapi::mojom::DeviceSettings* GetDeviceSettings() override {
    return device_settings_;
  }

  // Updates device settings and notifies the `DeviceSettingsObserver` of this
  // change.
  void UpdateDeviceSettings(crosapi::mojom::DeviceSettings* device_settings) {
    device_settings_ = device_settings;
    device_reporting_settings_->OnDeviceSettingsUpdated();
    device_settings_owned_.reset();
  }

 private:
  raw_ptr<DeviceReportingSettingsLacros> device_reporting_settings_;
  crosapi::mojom::DeviceSettingsPtr device_settings_owned_;
  raw_ptr<crosapi::mojom::DeviceSettings> device_settings_;
};

class DeviceReportingSettingsLacrosTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto test_delegate = std::make_unique<TestDelegate>();
    delegate_ = test_delegate.get();
    device_reporting_settings_ =
        DeviceReportingSettingsLacros::CreateForTest(std::move(test_delegate));
  }

  base::test::TaskEnvironment task_environment_;
  raw_ptr<TestDelegate> delegate_;
  std::unique_ptr<DeviceReportingSettingsLacros> device_reporting_settings_;
};

TEST_F(DeviceReportingSettingsLacrosTest, GetInvalidDeviceSetting) {
  // Cannot get numeric setting as a boolean.
  bool bool_value;
  EXPECT_FALSE(device_reporting_settings_->GetBoolean(
      ::policy::key::kReportUploadFrequency, &bool_value));

  // Cannot get boolean setting as an integer.
  int int_value;
  EXPECT_FALSE(device_reporting_settings_->GetInteger(
      ::policy::key::kReportDeviceNetworkStatus, &int_value));
}

TEST_F(DeviceReportingSettingsLacrosTest, GetBoolean) {
  crosapi::mojom::DeviceSettingsPtr device_settings_ptr =
      crosapi::mojom::DeviceSettings::New();
  device_settings_ptr->report_device_network_status =
      crosapi::mojom::DeviceSettings::OptionalBool::kTrue;
  delegate_->UpdateDeviceSettings(device_settings_ptr.get());

  bool value = false;
  ASSERT_TRUE(device_reporting_settings_->GetBoolean(
      ::policy::key::kReportDeviceNetworkStatus, &value));
  EXPECT_TRUE(value);
}

TEST_F(DeviceReportingSettingsLacrosTest, GetInteger) {
  constexpr int kUploadFrequency = 100;
  crosapi::mojom::NullableInt64Ptr upload_frequency =
      crosapi::mojom::NullableInt64::New();
  upload_frequency->value = kUploadFrequency;

  crosapi::mojom::DeviceSettingsPtr device_settings_ptr =
      crosapi::mojom::DeviceSettings::New();
  device_settings_ptr->report_upload_frequency = std::move(upload_frequency);
  delegate_->UpdateDeviceSettings(device_settings_ptr.get());

  int value = -1;
  ASSERT_TRUE(device_reporting_settings_->GetInteger(
      ::policy::key::kReportUploadFrequency, &value));
  EXPECT_EQ(value, kUploadFrequency);
}

TEST_F(DeviceReportingSettingsLacrosTest, GetList) {
  static constexpr char kTestSettingPath[] = "test_setting";
  const base::Value::List* list_value = nullptr;
  ASSERT_FALSE(
      device_reporting_settings_->GetList(kTestSettingPath, &list_value));
  EXPECT_THAT(list_value, IsNull());
}

TEST_F(DeviceReportingSettingsLacrosTest,
       NotifyObserverOnRelevantSettingsUpdate) {
  bool callback_called = false;
  base::CallbackListSubscription subscription =
      device_reporting_settings_->AddSettingsObserver(
          ::policy::key::kReportDeviceNetworkStatus,
          base::BindLambdaForTesting(
              [&callback_called]() { callback_called = true; }));

  // Verify callback is triggered after we update device settings.
  crosapi::mojom::DeviceSettingsPtr device_settings_ptr =
      crosapi::mojom::DeviceSettings::New();
  device_settings_ptr->report_device_network_status =
      crosapi::mojom::DeviceSettings::OptionalBool::kTrue;
  delegate_->UpdateDeviceSettings(device_settings_ptr.get());
  EXPECT_TRUE(callback_called);

  // Verify we can retrieve the updated setting now.
  bool value = false;
  ASSERT_TRUE(device_reporting_settings_->GetBoolean(
      ::policy::key::kReportDeviceNetworkStatus, &value));
  EXPECT_TRUE(value);
}

TEST_F(DeviceReportingSettingsLacrosTest,
       ShouldNotNotifyObserverOnIrrelevantSettingsUpdate) {
  bool callback_called = false;
  base::CallbackListSubscription subscription =
      device_reporting_settings_->AddSettingsObserver(
          ::policy::key::kReportDeviceNetworkStatus,
          base::BindLambdaForTesting(
              [&callback_called]() { callback_called = true; }));

  // Verify callback isn't triggered after we update an irrelevant setting.
  crosapi::mojom::DeviceSettingsPtr device_settings_ptr =
      crosapi::mojom::DeviceSettings::New();
  crosapi::mojom::NullableInt64Ptr upload_frequency =
      crosapi::mojom::NullableInt64::New();
  upload_frequency->value = 100;
  device_settings_ptr->report_upload_frequency = std::move(upload_frequency);
  delegate_->UpdateDeviceSettings(device_settings_ptr.get());
  EXPECT_FALSE(callback_called);
}

TEST_F(DeviceReportingSettingsLacrosTest,
       ShouldNotNotifyObserverWhenSettingsUnchanged) {
  // Set device settings before registering observer.
  crosapi::mojom::DeviceSettingsPtr device_settings_ptr =
      crosapi::mojom::DeviceSettings::New();
  device_settings_ptr->report_device_network_status =
      crosapi::mojom::DeviceSettings::OptionalBool::kTrue;
  delegate_->UpdateDeviceSettings(device_settings_ptr.get());

  // Register observer.
  bool callback_called = false;
  base::CallbackListSubscription subscription =
      device_reporting_settings_->AddSettingsObserver(
          ::policy::key::kReportDeviceNetworkStatus,
          base::BindLambdaForTesting(
              [&callback_called]() { callback_called = true; }));

  // Verify callback isn't triggered since there is no change to the device
  // setting.
  delegate_->UpdateDeviceSettings(device_settings_ptr.get());
  EXPECT_FALSE(callback_called);
}

}  // namespace
}  // namespace reporting
