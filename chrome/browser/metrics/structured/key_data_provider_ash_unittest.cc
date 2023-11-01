// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/key_data_provider_ash.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
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

class KeyDataProviderAshTest : public testing::Test {
 protected:
  void SetUp() override {
    CHECK(temp_dir_.CreateUniqueTempDir());

    base::RunLoop run_loop;
    base::OnceClosure callback =
        base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); });
    key_data_provider_ = std::make_unique<KeyDataProviderAsh>(
        DeviceKeyFilePath(), /*write_delay=*/base::Milliseconds(0));
    key_data_provider_->InitializeDeviceKey(std::move(callback));
    Wait();
    run_loop.Run();
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  void SetUpProfileKeys() {
    base::RunLoop run_loop;
    base::OnceClosure callback =
        base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); });
    key_data_provider_->InitializeProfileKey(ProfileKeyFilePath(),
                                             std::move(callback));
    Wait();
    run_loop.Run();
  }

  KeyData* GetCurrentKeyData(const std::string& project_name) {
    return key_data_provider_->GetKeyData(project_name);
  }

  KeyData* GetDeviceKeyData() { return key_data_provider_->GetDeviceKeyData(); }

  KeyData* GetProfileKeyData() {
    return key_data_provider_->GetProfileKeyData();
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

 private:
  std::unique_ptr<KeyDataProviderAsh> key_data_provider_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
};

}  // namespace

TEST_F(KeyDataProviderAshTest, UseDeviceKeyForDeviceProject) {
  auto* key_data = GetCurrentKeyData(kDeviceProjectName);
  EXPECT_NE(key_data, nullptr);
  EXPECT_EQ(key_data, GetDeviceKeyData());
}

TEST_F(KeyDataProviderAshTest, UseProfileKeyForProfileProject) {
  SetUpProfileKeys();
  auto* key_data = GetCurrentKeyData(kProfileProjectName);
  EXPECT_NE(key_data, nullptr);
  EXPECT_EQ(key_data, GetProfileKeyData());
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

}  // namespace metrics::structured
