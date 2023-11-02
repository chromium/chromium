// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/key_data_provider_ash.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/metrics/structured/key_data_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics::structured {

namespace {

using testing::Eq;
using testing::IsNull;
using testing::NotNull;

constexpr char kProfileProjectName[] = "TestProjectOne";
constexpr char kDeviceProjectName[] = "TestProjectFour";
constexpr char kCrOSEventsProjectName[] = "CrOSEvents";

class KeyDataProviderAshTest : public testing::Test, KeyDataProvider::Observer {
 protected:
  void SetUp() override {
    CHECK(temp_dir_.CreateUniqueTempDir());

    run_loop_ = std::make_unique<base::RunLoop>();
    key_data_provider_ = std::make_unique<KeyDataProviderAsh>(
        DeviceKeyFilePath(), /*write_delay=*/base::Milliseconds(0));
    key_data_provider_->AddObserver(this);
    Wait();
    run_loop_->Run();
  }

  void TearDown() override { key_data_provider_->RemoveObserver(this); }

  void OnKeyReady() override { run_loop_->Quit(); }

  void Wait() { task_environment_.RunUntilIdle(); }

  void SetUpProfileKeys() {
    run_loop_ = std::make_unique<base::RunLoop>();
    key_data_provider_->OnProfileAdded(ProfileKeyFilePath());
    Wait();
    run_loop_->Run();
  }

  KeyData* GetCurrentKeyData(const std::string& project_name) {
    return key_data_provider_->GetKeyData(project_name);
  }

  KeyData* GetDeviceKeyData() {
    return key_data_provider_->GetKeyData(kDeviceProjectName);
  }

  KeyData* GetProfileKeyData() {
    return key_data_provider_->GetKeyData(kProfileProjectName);
  }

  base::FilePath DeviceKeyFilePath() {
    return temp_dir_.GetPath()
        .Append("structured_metrics")
        .Append("device_keys");
  }

  base::FilePath ProfileKeyFilePath() {
    return temp_dir_.GetPath()
        .Append("structured_metrics")
        .Append("profile_keys");
  }

  std::unique_ptr<KeyDataProviderAsh> key_data_provider_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;

  // Used to wait for keys.
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

TEST_F(KeyDataProviderAshTest, UseDeviceKeyForDeviceProject) {
  auto* key_data = GetCurrentKeyData(kDeviceProjectName);
  EXPECT_NE(key_data, nullptr);

  // Ensure that the pointers are not the same.
  EXPECT_NE(key_data, GetProfileKeyData());
  EXPECT_EQ(key_data, GetDeviceKeyData());
}

TEST_F(KeyDataProviderAshTest, UseProfileKeyForProfileProject) {
  SetUpProfileKeys();
  auto* key_data = GetCurrentKeyData(kProfileProjectName);
  EXPECT_NE(key_data, nullptr);

  // Ensure that the pointers are not the same.
  EXPECT_EQ(key_data, GetProfileKeyData());
  EXPECT_NE(key_data, GetDeviceKeyData());
}

TEST_F(KeyDataProviderAshTest, ReturnNullIfProfileProjectBeforeProfileKey) {
  auto* key_data = GetCurrentKeyData(kProfileProjectName);
  EXPECT_EQ(key_data, nullptr);
}

TEST_F(KeyDataProviderAshTest, ReturnProfileKeyForCrOSEvent) {
  SetUpProfileKeys();
  auto* key_data = GetCurrentKeyData(kCrOSEventsProjectName);
  EXPECT_NE(key_data, nullptr);
  EXPECT_EQ(key_data, GetProfileKeyData());
}

TEST_F(KeyDataProviderAshTest, ReturnsAppropriateSequenceIds) {
  SetUpProfileKeys();
  auto* key_data = GetCurrentKeyData(kCrOSEventsProjectName);
  EXPECT_NE(key_data, nullptr);
  EXPECT_EQ(key_data, GetProfileKeyData());
}

TEST_F(KeyDataProviderAshTest, SequenceEvents_ReturnsDifferentSequenceIds) {
  SetUpProfileKeys();

  absl::optional<uint64_t> device_id =
      key_data_provider_->GetSecondaryId(kCrOSEventsProjectName);
  absl::optional<uint64_t> profile_id =
      key_data_provider_->GetId(kCrOSEventsProjectName);

  EXPECT_TRUE(device_id.has_value());
  EXPECT_TRUE(profile_id.has_value());

  // Ids generated should be different.
  EXPECT_NE(device_id.value(), profile_id.value());
}

TEST_F(KeyDataProviderAshTest, SequenceEvents_PrimaryIdEmptyOnNoProfileSetup) {
  absl::optional<uint64_t> device_id =
      key_data_provider_->GetSecondaryId(kCrOSEventsProjectName);
  absl::optional<uint64_t> profile_id =
      key_data_provider_->GetId(kCrOSEventsProjectName);

  EXPECT_TRUE(device_id.has_value());
  EXPECT_FALSE(profile_id.has_value());
}

}  // namespace metrics::structured
