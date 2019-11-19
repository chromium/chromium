// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/display/display_configuration_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/common/api/system_display.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"

namespace em = enterprise_management;

namespace {

struct PolicyValue {
  base::Optional<int> external_width;
  base::Optional<int> external_height;
  base::Optional<int> external_scale_percentage;
  bool use_native = false;
  base::Optional<int> internal_scale_percentage;

  bool operator==(const PolicyValue& rhs) const {
    return external_width == rhs.external_width &&
           external_height == rhs.external_height &&
           external_scale_percentage == rhs.external_scale_percentage &&
           use_native == rhs.use_native &&
           internal_scale_percentage == rhs.internal_scale_percentage;
  }

  gfx::Size external_display_size() const {
    return gfx::Size(external_width.value_or(0), external_height.value_or(0));
  }
};

const gfx::Size kDefaultDisplayResolution(1280, 800);
const gfx::Size kDefaultExternalDisplayResolution(1920, 1080);
const int kDefaultDisplayScale = 100;

display::DisplayManager* GetDisplayManager() {
  return ash::Shell::Get()->display_manager();
}

int64_t GetInternalDisplayId() {
  const display::DisplayManager* const display_manager = GetDisplayManager();
  return display_manager->first_display_id();
}

gfx::Size GetResolution(int64_t display_id) {
  const display::DisplayManager* const display_manager = GetDisplayManager();
  display::ManagedDisplayMode display_mode;
  if (display_manager->GetSelectedModeForDisplayId(display_id, &display_mode)) {
    return display_mode.size();
  }
  const display::Display& display =
      display_manager->GetDisplayForId(display_id);
  return display.GetSizeInPixel();
}

double GetScale(int64_t display_id) {
  const display::DisplayManager* const display_manager = GetDisplayManager();
  display::ManagedDisplayMode display_mode;
  const display::Display& display =
      display_manager->GetDisplayForId(display_id);
  return display.device_scale_factor();
}

int GetScaleOfInternalDisplay() {
  // Converting scale to percents.
  return floor(GetScale(GetInternalDisplayId()) * 100.0 + 0.5);
}

gfx::Size GetResolutionOfInternalDisplay() {
  return GetResolution(GetInternalDisplayId());
}

int64_t GetExternalDisplayId() {
  const display::DisplayManager* const display_manager = GetDisplayManager();
  if (display_manager->GetNumDisplays() < 2) {
    ADD_FAILURE() << "The external display is not connected.";
    return 0;
  }
  return display_manager->GetCurrentDisplayIdList()[1];
}

int GetScaleOfExternalDisplay() {
  // Converting scale to percents.
  return floor(GetScale(GetExternalDisplayId()) * 100.0 + 0.5);
}

gfx::Size GetResolutionOfExternalDisplay() {
  return GetResolution(GetExternalDisplayId());
}

PolicyValue GetPolicySetting() {
  const base::DictionaryValue* resolution_pref = nullptr;
  chromeos::CrosSettings::Get()->GetDictionary(
      chromeos::kDeviceDisplayResolution, &resolution_pref);
  EXPECT_TRUE(resolution_pref) << "DeviceDisplayResolution setting is not set";
  const base::Value* width = resolution_pref->FindKeyOfType(
      {chromeos::kDeviceDisplayResolutionKeyExternalWidth},
      base::Value::Type::INTEGER);
  const base::Value* height = resolution_pref->FindKeyOfType(
      {chromeos::kDeviceDisplayResolutionKeyExternalHeight},
      base::Value::Type::INTEGER);
  const base::Value* external_scale = resolution_pref->FindKeyOfType(
      {chromeos::kDeviceDisplayResolutionKeyExternalScale},
      base::Value::Type::INTEGER);
  const base::Value* use_native = resolution_pref->FindKeyOfType(
      {chromeos::kDeviceDisplayResolutionKeyExternalUseNative},
      base::Value::Type::BOOLEAN);
  const base::Value* internal_scale = resolution_pref->FindKeyOfType(
      {chromeos::kDeviceDisplayResolutionKeyInternalScale},
      base::Value::Type::INTEGER);
  PolicyValue result;
  if (width)
    result.external_width = width->GetInt();
  if (height)
    result.external_height = height->GetInt();
  if (external_scale)
    result.external_scale_percentage = external_scale->GetInt();
  if (internal_scale)
    result.internal_scale_percentage = internal_scale->GetInt();
  if (use_native && use_native->GetBool())
    result.use_native = true;
  return result;
}

void AddExternalDisplay() {
  GetDisplayManager()->AddRemoveDisplay(
      {display::ManagedDisplayMode(gfx::Size(800, 600), 30.0, false, false),
       display::ManagedDisplayMode(gfx::Size(800, 600), 60.0, false, false),
       display::ManagedDisplayMode(gfx::Size(1280, 800), 60.0, false, false),
       display::ManagedDisplayMode(gfx::Size(1920, 1080), 30.0, false, false),
       display::ManagedDisplayMode(gfx::Size(1920, 1080), 60.0, false, true)});
  base::RunLoop().RunUntilIdle();
}

void SetPolicyValue(em::ChromeDeviceSettingsProto* proto,
                    PolicyValue policy,
                    bool recommended) {
  std::vector<std::string> json_entries;
  std::string json = "";
  if (policy.external_width) {
    json_entries.push_back("\"external_width\": " +
                           std::to_string(*policy.external_width));
  }
  if (policy.external_height) {
    json_entries.push_back("\"external_height\": " +
                           std::to_string(*policy.external_height));
  }
  if (policy.external_scale_percentage) {
    json_entries.push_back("\"external_scale_percentage\": " +
                           std::to_string(*policy.external_scale_percentage));
  }
  if (policy.internal_scale_percentage) {
    json_entries.push_back("\"internal_scale_percentage\": " +
                           std::to_string(*policy.internal_scale_percentage));
  }

  json_entries.push_back(std::string("\"recommended\": ") +
                         (recommended ? "true" : "false"));
  proto->mutable_device_display_resolution()->set_device_display_resolution(
      "{" + base::JoinString(json_entries, ",") + "}");
}

std::unique_ptr<extensions::api::system_display::DisplayMode>
CreateDisplayMode(int64_t display_id, int width, int height) {
  auto result =
      std::make_unique<extensions::api::system_display::DisplayMode>();
  const display::ManagedDisplayInfo& info =
      GetDisplayManager()->GetDisplayInfo(display_id);
  for (const display::ManagedDisplayMode& mode : info.display_modes()) {
    if (mode.size().width() == width && mode.size().height() == height) {
      result->width = mode.size().width();
      result->height = mode.size().height();
      result->width_in_native_pixels = mode.size().width();
      result->height_in_native_pixels = mode.size().height();
      result->refresh_rate = mode.refresh_rate();
      result->is_native = mode.native();
      result->device_scale_factor = mode.device_scale_factor();
      return result;
    }
  }
  result->width = width;
  result->height = height;
  return result;
}

}  // anonymous namespace

namespace policy {

class DeviceDisplayResolutionTestBase
    : public policy::DevicePolicyCrosBrowserTest,
      public testing::WithParamInterface<PolicyValue> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
    command_line->AppendSwitch(switches::kUseFirstDisplayAsInternal);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ash::DisplayConfigurationController::DisableAnimatorForTest();
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownOnMainThread() override {
    // If the login display is still showing, exit gracefully.
    if (chromeos::LoginDisplayHost::default_host()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&chrome::AttemptExit));
      RunUntilBrowserProcessQuits();
    }
  }

 protected:
  DeviceDisplayResolutionTestBase() {}

  void SetPolicy(PolicyValue policy, bool recommended) {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    SetPolicyValue(&proto, policy, recommended);
    RefreshPolicyAndWaitUntilDeviceSettingsUpdated();
  }

  void UnsetPolicy() {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.clear_device_display_resolution();
    RefreshPolicyAndWaitUntilDeviceSettingsUpdated();
  }

  void RefreshPolicyAndWaitUntilDeviceSettingsUpdated() {
    base::RunLoop run_loop;
    // For calls from SetPolicy().
    std::unique_ptr<chromeos::CrosSettings::ObserverSubscription> observer =
        chromeos::CrosSettings::Get()->AddSettingsObserver(
            chromeos::kDeviceDisplayResolution, run_loop.QuitClosure());
    RefreshDevicePolicy();
    run_loop.Run();
    // Allow tasks posted by CrosSettings observers to complete:
    base::RunLoop().RunUntilIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceDisplayResolutionTestBase);
};

class DeviceDisplayResolutionTest : public DeviceDisplayResolutionTestBase {
 public:
  DeviceDisplayResolutionTest() {}

 protected:
  void SetPolicy(PolicyValue value) {
    DeviceDisplayResolutionTestBase::SetPolicy(value, false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceDisplayResolutionTest);
};

// crbug.com/1000694.
IN_PROC_BROWSER_TEST_P(DeviceDisplayResolutionTest, DISABLED_Internal) {
  const PolicyValue policy_value = GetParam();

  EXPECT_EQ(kDefaultDisplayScale, GetScaleOfInternalDisplay())
      << "Initial primary display scale before policy";
  EXPECT_EQ(kDefaultDisplayResolution, GetResolutionOfInternalDisplay())
      << "Initial primary display resolution before policy";

  SetPolicy(policy_value);
  PolicyValue setting_resolution = GetPolicySetting();
  EXPECT_EQ(policy_value, setting_resolution)
      << "Value of CrosSettings after policy value changed";
  EXPECT_EQ(policy_value.internal_scale_percentage.value_or(0),
            GetScaleOfInternalDisplay())
      << "Scale of primary display after policy";
}

// crbug.com/1000694.
IN_PROC_BROWSER_TEST_P(DeviceDisplayResolutionTest,
                       DISABLED_ResizeExternalDisplay) {
  const PolicyValue policy_value = GetParam();

  AddExternalDisplay();

  EXPECT_EQ(kDefaultDisplayScale, GetScaleOfInternalDisplay())
      << "Initial primary display scale after connecting external";
  EXPECT_EQ(kDefaultDisplayResolution, GetResolutionOfInternalDisplay())
      << "Initial primary display resolution after connecting external";

  EXPECT_EQ(kDefaultDisplayScale, GetScaleOfExternalDisplay())
      << "Scale of external display before policy";
  EXPECT_EQ(kDefaultExternalDisplayResolution, GetResolutionOfExternalDisplay())
      << "Resolution of external display before policy";

  SetPolicy(policy_value);
  EXPECT_EQ(policy_value.external_scale_percentage.value_or(0),
            GetScaleOfExternalDisplay())
      << "Scale of already connected external display after policy";
  EXPECT_EQ(policy_value.external_display_size(),
            GetResolutionOfExternalDisplay())
      << "Resolution of already connected external display after policy";

  EXPECT_EQ(policy_value.internal_scale_percentage.value_or(0),
            GetScaleOfInternalDisplay())
      << "Primary display scale after resizing external";
}

// crbug.com/1000694.
IN_PROC_BROWSER_TEST_P(DeviceDisplayResolutionTest,
                       DISABLED_ConnectExternalDisplay) {
  const PolicyValue policy_value = GetParam();

  SetPolicy(policy_value);
  EXPECT_EQ(policy_value.internal_scale_percentage.value_or(0),
            GetScaleOfInternalDisplay())
      << "Initial primary display scale after applying policy";

  AddExternalDisplay();
  EXPECT_EQ(policy_value.external_scale_percentage.value_or(0),
            GetScaleOfExternalDisplay())
      << "Scale of newly connected external display after policy";
  EXPECT_EQ(policy_value.external_display_size(),
            GetResolutionOfExternalDisplay())
      << "Resolution of newly connected external display after policy";
  EXPECT_EQ(policy_value.internal_scale_percentage.value_or(0),
            GetScaleOfInternalDisplay())
      << "Primary display scale after connecting external";
}

// crbug.com/1000694.
IN_PROC_BROWSER_TEST_P(DeviceDisplayResolutionTest,
                       DISABLED_SetAndUnsetPolicy) {
  const PolicyValue policy_value = GetParam();
  AddExternalDisplay();
  SetPolicy(policy_value);
  UnsetPolicy();
  EXPECT_EQ(policy_value.external_scale_percentage.value_or(0),
            GetScaleOfExternalDisplay())
      << "Scale of the external display after policy was set and unset";
  EXPECT_EQ(policy_value.external_display_size(),
            GetResolutionOfExternalDisplay())
      << "Resolution of the external display after policy was set and unset.";
  EXPECT_EQ(policy_value.internal_scale_percentage.value_or(0),
            GetScaleOfInternalDisplay())
      << "Initial primary display scale after policy was set and unset";
}

INSTANTIATE_TEST_SUITE_P(
    PolicyDeviceDisplayResolution,
    DeviceDisplayResolutionTest,
    testing::Values(PolicyValue{1920, 1080, 200, false, 200},
                    PolicyValue{800, 600, 50, false, 100},
                    PolicyValue{1280, 800, 100, false, 150}));

// This class tests that the policy is reapplied after a reboot. To persist from
// PRE_Reboot to Reboot, the policy is inserted into a FakeSessionManagerClient.
// From there, it travels to DeviceSettingsProvider, whose UpdateFromService()
// method caches the policy (using device_settings_cache::Store()).
// In the main test, the FakeSessionManagerClient is not fully initialized.
// Thus, DeviceSettingsProvider falls back on the cached values (using
// device_settings_cache::Retrieve()).
class DisplayResolutionBootTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<PolicyValue> {
 protected:
  DisplayResolutionBootTest() {
    device_state_.set_skip_initial_policy_setup(true);
  }
  ~DisplayResolutionBootTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kUseFirstDisplayAsInternal);
  }

  void SetUpInProcessBrowserTestFixture() override {
    // Override FakeSessionManagerClient. This will be shut down by the browser.
    chromeos::SessionManagerClient::InitializeFakeInMemory();
    ash::DisplayConfigurationController::DisableAnimatorForTest();
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  policy::DevicePolicyCrosTestHelper test_helper_;

  chromeos::DeviceStateMixin device_state_{
      &mixin_host_,
      chromeos::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayResolutionBootTest);
};

IN_PROC_BROWSER_TEST_P(DisplayResolutionBootTest, PRE_Reboot) {
  const PolicyValue policy_value = GetParam();

  // Set policy.
  policy::DevicePolicyBuilder* const device_policy(
      test_helper_.device_policy());
  em::ChromeDeviceSettingsProto& proto(device_policy->payload());
  SetPolicyValue(&proto, policy_value, true);
  base::RunLoop run_loop;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription> observer =
      chromeos::CrosSettings::Get()->AddSettingsObserver(
          chromeos::kDeviceDisplayResolution, run_loop.QuitClosure());
  device_policy->SetDefaultSigningKey();
  device_policy->Build();
  chromeos::FakeSessionManagerClient::Get()->set_device_policy(
      device_policy->GetBlob());
  chromeos::FakeSessionManagerClient::Get()->OnPropertyChangeComplete(true);
  run_loop.Run();
  // Allow tasks posted by CrosSettings observers to complete:
  base::RunLoop().RunUntilIdle();
  AddExternalDisplay();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(policy_value.external_scale_percentage.value_or(0),
            GetScaleOfExternalDisplay())
      << "Scale of the external display after policy set";
  EXPECT_EQ(policy_value.external_display_size(),
            GetResolutionOfExternalDisplay())
      << "Resolution of the external display after policy set";
  EXPECT_EQ(policy_value.internal_scale_percentage.value_or(0),
            GetScaleOfInternalDisplay())
      << "Initial primary display scale after policy set";
}

// crbug.com/1000694.
IN_PROC_BROWSER_TEST_P(DisplayResolutionBootTest, DISABLED_Reboot) {
  const PolicyValue policy_value = GetParam();

  AddExternalDisplay();
  base::RunLoop().RunUntilIdle();
  // Check that the policy resolution is restored.
  EXPECT_EQ(policy_value.external_scale_percentage.value_or(0),
            GetScaleOfExternalDisplay())
      << "Scale of the external display after reboot";
  EXPECT_EQ(policy_value.external_display_size(),
            GetResolutionOfExternalDisplay())
      << "Resolution of the external display after reboot";
  EXPECT_EQ(policy_value.internal_scale_percentage.value_or(0),
            GetScaleOfInternalDisplay())
      << "Initial primary display scale after reboot";
}

INSTANTIATE_TEST_SUITE_P(PolicyDeviceDisplayResolution,
                         DisplayResolutionBootTest,
                         testing::Values(PolicyValue{1920, 1080, 200, false,
                                                     200},
                                         PolicyValue{800, 600, 50, false, 50}));

class DeviceDisplayResolutionRecommendedTest
    : public DeviceDisplayResolutionTestBase {
 public:
  DeviceDisplayResolutionRecommendedTest() {}

 protected:
  void SetPolicy(PolicyValue value) {
    DeviceDisplayResolutionTestBase::SetPolicy(value, true);
  }

  void SetUserProperties(
      int64_t display_id,
      extensions::api::system_display::DisplayProperties props) {
    base::RunLoop run_loop;
    base::OnceClosure quit_closure(run_loop.QuitClosure());
    base::Optional<std::string> operation_error;
    extensions::DisplayInfoProvider::Get()->SetDisplayProperties(
        std::to_string(display_id), std::move(props),
        base::BindOnce(
            [](base::OnceClosure quit_closure, base::Optional<std::string>) {
              std::move(quit_closure).Run();
            },
            std::move(quit_closure)));
    run_loop.Run();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceDisplayResolutionRecommendedTest);
};

// crbug.com/1000694.
IN_PROC_BROWSER_TEST_P(DeviceDisplayResolutionRecommendedTest,
                       DISABLED_Internal) {
  const PolicyValue policy_value = GetParam();
  EXPECT_EQ(kDefaultDisplayResolution, GetResolutionOfInternalDisplay())
      << "Initial primary display resolution before policy";
  EXPECT_EQ(kDefaultDisplayScale, GetScaleOfInternalDisplay())
      << "Initial primary display scale before policy";

  SetPolicy(policy_value);
  PolicyValue setting_resolution = GetPolicySetting();
  EXPECT_EQ(policy_value, setting_resolution)
      << "Value of CrosSettings after policy value changed";
  EXPECT_EQ(setting_resolution.internal_scale_percentage.value_or(0),
            GetScaleOfInternalDisplay())
      << "Scale of primary display after policy";

  extensions::api::system_display::DisplayProperties props;
  double user_scale = 50;
  props.display_zoom_factor = std::make_unique<double>(user_scale / 100.0);
  SetUserProperties(GetInternalDisplayId(), std::move(props));

  EXPECT_EQ(user_scale, GetScaleOfInternalDisplay())
      << "Scale of internal display after user operation";
}

// crbug.com/1000694.
IN_PROC_BROWSER_TEST_P(DeviceDisplayResolutionRecommendedTest,
                       DISABLED_ResizeExternalDisplay) {
  const PolicyValue policy_value = GetParam();
  AddExternalDisplay();

  EXPECT_EQ(kDefaultDisplayScale, GetScaleOfInternalDisplay())
      << "Initial primary display scale after connecting external";
  EXPECT_EQ(kDefaultDisplayResolution, GetResolutionOfInternalDisplay())
      << "Initial primary display resolution after connecting external";

  EXPECT_EQ(kDefaultDisplayScale, GetScaleOfExternalDisplay())
      << "Scale of external display before policy";
  EXPECT_EQ(kDefaultExternalDisplayResolution, GetResolutionOfExternalDisplay())
      << "Resolution of external display before policy";

  SetPolicy(policy_value);

  EXPECT_EQ(policy_value.external_scale_percentage.value_or(0),
            GetScaleOfExternalDisplay())
      << "Scale of the external display after policy";
  EXPECT_EQ(policy_value.external_display_size(),
            GetResolutionOfExternalDisplay())
      << "Resolution the external display after policy";

  EXPECT_EQ(policy_value.internal_scale_percentage.value_or(0),
            GetScaleOfInternalDisplay())
      << "Internal display scale after resizing external";

  extensions::api::system_display::DisplayProperties props;
  double user_scale = 50;
  double user_width = 1920;
  double user_height = 1080;
  props.display_zoom_factor = std::make_unique<double>(user_scale / 100.0);
  props.display_mode =
      CreateDisplayMode(GetExternalDisplayId(), user_width, user_height);
  SetUserProperties(GetExternalDisplayId(), std::move(props));

  EXPECT_EQ(user_scale, GetScaleOfExternalDisplay())
      << "Scale of the external display after user operation";
  EXPECT_EQ(gfx::Size(user_width, user_height),
            GetResolutionOfExternalDisplay())
      << "Resolution of the external display after user operation";

  EXPECT_EQ(policy_value.internal_scale_percentage.value_or(0),
            GetScaleOfInternalDisplay())
      << "Internal display scale after user operation";
}

INSTANTIATE_TEST_SUITE_P(
    PolicyDeviceDisplayResolution,
    DeviceDisplayResolutionRecommendedTest,
    testing::Values(PolicyValue{1920, 1080, 200, false, 200},
                    PolicyValue{800, 600, 50, false, 100},
                    PolicyValue{1280, 800, 100, false, 150}));

}  // namespace policy
