// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tpm_firmware_update.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace tpm_firmware_update {

TEST(TPMFirmwareUpdateTest, DecodeSettingsProto) {
  enterprise_management::TPMFirmwareUpdateSettingsProto settings;
  settings.set_allow_user_initiated_powerwash(true);
  settings.set_allow_user_initiated_preserve_device_state(true);
  settings.set_auto_update_mode(
      enterprise_management::
          TPMFirmwareUpdateSettingsProto_AutoUpdateMode_USER_ACKNOWLEDGMENT);
  auto dict = DecodeSettingsProto(settings);
  ASSERT_TRUE(dict);
  bool value = false;
  EXPECT_TRUE(dict->GetBoolean("allow-user-initiated-powerwash", &value));
  EXPECT_TRUE(value);
  EXPECT_TRUE(
      dict->GetBoolean("allow-user-initiated-preserve-device-state", &value));
  EXPECT_TRUE(value);
  int update_mode_value = 0;
  EXPECT_TRUE(dict->GetInteger("auto-update-mode", &update_mode_value));
  EXPECT_EQ(2, update_mode_value);
}

class TPMFirmwareUpdateTest : public testing::Test {
 public:
  enum class Availability {
    kPending,
    kUnavailable,
    kUnavailableROCAVulnerable,
    kAvailable,
  };

  TPMFirmwareUpdateTest() {
    feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    feature_list_->InitAndEnableFeature(features::kTPMFirmwareUpdate);
    CHECK(temp_dir_.CreateUniqueTempDir());
    base::FilePath update_location_path =
        temp_dir_.GetPath().AppendASCII("tpm_firmware_update_location");
    path_override_location_ = std::make_unique<base::ScopedPathOverride>(
        chrome::FILE_CHROME_OS_TPM_FIRMWARE_UPDATE_LOCATION,
        update_location_path, update_location_path.IsAbsolute(), false);
    base::FilePath srk_vulnerable_roca_path = temp_dir_.GetPath().AppendASCII(
        "tpm_firmware_update_srk_vulnerable_roca");
    path_override_srk_vulnerable_roca_ =
        std::make_unique<base::ScopedPathOverride>(
            chrome::FILE_CHROME_OS_TPM_FIRMWARE_UPDATE_SRK_VULNERABLE_ROCA,
            srk_vulnerable_roca_path, srk_vulnerable_roca_path.IsAbsolute(),
            false);
    SetUpdateAvailability(Availability::kAvailable);
  }

  void SetUpdateAvailability(Availability availability) {
    base::FilePath srk_vulnerable_roca_path;
    ASSERT_TRUE(base::PathService::Get(
        chrome::FILE_CHROME_OS_TPM_FIRMWARE_UPDATE_SRK_VULNERABLE_ROCA,
        &srk_vulnerable_roca_path));
    switch (availability) {
      case Availability::kPending:
      case Availability::kUnavailable:
        base::DeleteFile(srk_vulnerable_roca_path, false);
        break;
      case Availability::kAvailable:
      case Availability::kUnavailableROCAVulnerable:
        ASSERT_TRUE(base::ImportantFileWriter::WriteFileAtomically(
            srk_vulnerable_roca_path, ""));
        break;
    }

    base::FilePath update_location_path;
    ASSERT_TRUE(base::PathService::Get(
        chrome::FILE_CHROME_OS_TPM_FIRMWARE_UPDATE_LOCATION,
        &update_location_path));
    switch (availability) {
      case Availability::kPending:
        base::DeleteFile(update_location_path, false);
        break;
      case Availability::kUnavailable:
      case Availability::kUnavailableROCAVulnerable:
        ASSERT_TRUE(base::ImportantFileWriter::WriteFileAtomically(
            update_location_path, ""));
        break;
      case Availability::kAvailable:
        const char kUpdatePath[] = "/lib/firmware/tpm/firmware.bin";
        ASSERT_TRUE(base::ImportantFileWriter::WriteFileAtomically(
            update_location_path, kUpdatePath));
        break;
    }
  }

  std::unique_ptr<base::test::ScopedFeatureList> feature_list_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> path_override_location_;
  std::unique_ptr<base::ScopedPathOverride> path_override_srk_vulnerable_roca_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  chromeos::system::ScopedFakeStatisticsProvider statistics_provider_;
};

class TPMFirmwareUpdateModesTest : public TPMFirmwareUpdateTest {
 public:
  TPMFirmwareUpdateModesTest() {
    callback_ = base::BindOnce(&TPMFirmwareUpdateModesTest::RecordResponse,
                               base::Unretained(this));
  }

  void RecordResponse(const std::set<Mode>& modes) {
    callback_received_ = true;
    callback_modes_ = modes;
  }

  const std::set<Mode> kAllModes{Mode::kPowerwash, Mode::kPreserveDeviceState};

  bool callback_received_ = false;
  std::set<Mode> callback_modes_;
  base::OnceCallback<void(const std::set<Mode>&)> callback_;
};

TEST_F(TPMFirmwareUpdateModesTest, FeatureDisabled) {
  feature_list_.reset();
  feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  feature_list_->InitAndDisableFeature(features::kTPMFirmwareUpdate);
  GetAvailableUpdateModes(std::move(callback_), base::TimeDelta());
  EXPECT_TRUE(callback_received_);
  EXPECT_TRUE(callback_modes_.empty());
}

TEST_F(TPMFirmwareUpdateModesTest, FRERequired) {
  statistics_provider_.SetMachineStatistic(system::kCheckEnrollmentKey, "1");
  GetAvailableUpdateModes(std::move(callback_), base::TimeDelta());
  EXPECT_TRUE(callback_received_);
  EXPECT_TRUE(callback_modes_.empty());
}

TEST_F(TPMFirmwareUpdateModesTest, Pending) {
  SetUpdateAvailability(Availability::kPending);
  GetAvailableUpdateModes(std::move(callback_), base::TimeDelta());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_TRUE(callback_modes_.empty());
}

TEST_F(TPMFirmwareUpdateModesTest, Available) {
  GetAvailableUpdateModes(std::move(callback_), base::TimeDelta());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_EQ(kAllModes, callback_modes_);
}

TEST_F(TPMFirmwareUpdateModesTest, AvailableAfterWaiting) {
  SetUpdateAvailability(Availability::kPending);
  GetAvailableUpdateModes(std::move(callback_),
                          base::TimeDelta::FromSeconds(5));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(callback_received_);

  // When testing that file appearance triggers the callback, we can't rely on
  // a single execution of TaskEnvironment::RunUntilIdle(). This is
  // because TaskEnvironment doesn't know about file system events that
  // haven't fired and propagated to a task scheduler thread yet so may return
  // early before the file system event is received. An event is expected here
  // though, so keep spinning the loop until the callback is received. This
  // isn't ideal, but better than flakiness due to file system events racing
  // with a single invocation of RunUntilIdle().
  SetUpdateAvailability(Availability::kAvailable);
  while (!callback_received_) {
    task_environment_.RunUntilIdle();
  }
  EXPECT_EQ(kAllModes, callback_modes_);

  // Trigger timeout and validate there are no further callbacks or crashes.
  callback_received_ = false;
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(5));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(callback_received_);
}

TEST_F(TPMFirmwareUpdateModesTest, NoUpdateVulnerableSRK) {
  SetUpdateAvailability(Availability::kUnavailableROCAVulnerable);
  GetAvailableUpdateModes(std::move(callback_), base::TimeDelta());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_EQ(std::set<Mode>{Mode::kCleanup}, callback_modes_);
}

TEST_F(TPMFirmwareUpdateModesTest, NoUpdateNonVulnerableSRK) {
  SetUpdateAvailability(Availability::kUnavailable);
  GetAvailableUpdateModes(std::move(callback_), base::TimeDelta());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_EQ(std::set<Mode>(), callback_modes_);
}

TEST_F(TPMFirmwareUpdateModesTest, Timeout) {
  SetUpdateAvailability(Availability::kPending);
  GetAvailableUpdateModes(std::move(callback_),
                          base::TimeDelta::FromSeconds(5));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(callback_received_);

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(5));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_TRUE(callback_modes_.empty());
}

class TPMFirmwareUpdateModesEnterpriseTest : public TPMFirmwareUpdateModesTest {
 public:
  TPMFirmwareUpdateModesEnterpriseTest() {
    cros_settings_test_helper_.ReplaceDeviceSettingsProviderWithStub();
    cros_settings_test_helper_.InstallAttributes()->SetCloudManaged(
        "example.com", "fake-device-id");
  }

  void SetPolicy(const std::set<Mode>& modes) {
    base::DictionaryValue dict;
    dict.SetKey(kSettingsKeyAllowPowerwash,
                base::Value(modes.count(Mode::kPowerwash) > 0));
    dict.SetKey(kSettingsKeyAllowPreserveDeviceState,
                base::Value(modes.count(Mode::kPreserveDeviceState) > 0));
    cros_settings_test_helper_.Set(kTPMFirmwareUpdateSettings, dict);
  }
};

TEST_F(TPMFirmwareUpdateModesEnterpriseTest, DeviceSettingPending) {
  SetPolicy(kAllModes);

  cros_settings_test_helper_.SetTrustedStatus(
      CrosSettingsProvider::TEMPORARILY_UNTRUSTED);
  GetAvailableUpdateModes(std::move(callback_), base::TimeDelta());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(callback_received_);

  cros_settings_test_helper_.SetTrustedStatus(CrosSettingsProvider::TRUSTED);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_EQ(kAllModes, callback_modes_);
}

TEST_F(TPMFirmwareUpdateModesEnterpriseTest, DeviceSettingUntrusted) {
  cros_settings_test_helper_.SetTrustedStatus(
      CrosSettingsProvider::PERMANENTLY_UNTRUSTED);
  GetAvailableUpdateModes(std::move(callback_), base::TimeDelta());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_TRUE(callback_modes_.empty());
}

TEST_F(TPMFirmwareUpdateModesEnterpriseTest, DeviceSettingNotSet) {
  GetAvailableUpdateModes(std::move(callback_), base::TimeDelta());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_TRUE(callback_modes_.empty());
}

TEST_F(TPMFirmwareUpdateModesEnterpriseTest, DeviceSettingDisallowed) {
  SetPolicy({});
  GetAvailableUpdateModes(std::move(callback_), base::TimeDelta());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_TRUE(callback_modes_.empty());
}

TEST_F(TPMFirmwareUpdateModesEnterpriseTest, DeviceSettingPowerwashAllowed) {
  SetPolicy({Mode::kPowerwash});
  GetAvailableUpdateModes(std::move(callback_), base::TimeDelta());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_EQ(std::set<Mode>({Mode::kPowerwash}), callback_modes_);
}

TEST_F(TPMFirmwareUpdateModesEnterpriseTest,
       DeviceSettingPreserveDeviceStateAllowed) {
  SetPolicy({Mode::kPreserveDeviceState});
  GetAvailableUpdateModes(std::move(callback_), base::TimeDelta());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_EQ(std::set<Mode>({Mode::kPreserveDeviceState}), callback_modes_);
}

TEST_F(TPMFirmwareUpdateModesEnterpriseTest, VulnerableSRK) {
  SetUpdateAvailability(Availability::kUnavailableROCAVulnerable);
  SetPolicy({Mode::kPreserveDeviceState});
  GetAvailableUpdateModes(std::move(callback_), base::TimeDelta());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_EQ(std::set<Mode>({Mode::kCleanup}), callback_modes_);
}

class TPMFirmwareAutoUpdateTest : public TPMFirmwareUpdateTest {
 public:
  TPMFirmwareAutoUpdateTest() {
    callback_ = base::BindOnce(&TPMFirmwareAutoUpdateTest::RecordResponse,
                               base::Unretained(this));
  }

  void RecordResponse(bool update_available) {
    callback_received_ = true;
    update_available_ = update_available;
  }

  bool callback_received_ = false;
  bool update_available_;
  base::OnceCallback<void(bool)> callback_;
};

TEST_F(TPMFirmwareAutoUpdateTest, AutoUpdateAvaiable) {
  UpdateAvailable(std::move(callback_), base::TimeDelta());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_TRUE(update_available_);
}

TEST_F(TPMFirmwareAutoUpdateTest, VulnerableSRKNoStatePreservingUpdate) {
  SetUpdateAvailability(Availability::kUnavailableROCAVulnerable);
  UpdateAvailable(std::move(callback_), base::TimeDelta());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_FALSE(update_available_);
}

TEST_F(TPMFirmwareAutoUpdateTest, NoUpdate) {
  SetUpdateAvailability(Availability::kUnavailable);
  UpdateAvailable(std::move(callback_), base::TimeDelta());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_received_);
  EXPECT_FALSE(update_available_);
}

}  // namespace tpm_firmware_update
}  // namespace chromeos
