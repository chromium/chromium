// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_set_volume_job.h"

#include <memory>

#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

namespace em = ::enterprise_management;

const RemoteCommandJob::UniqueIDType kUniqueID = 123456789;

// Name of the field in the command payload containing the volume.
const char kVolumeFieldName[] = "volume";

em::RemoteCommand GenerateSetVolumeCommandProto(base::TimeDelta age_of_command,
                                                int volume) {
  em::RemoteCommand command_proto;
  command_proto.set_type(em::RemoteCommand_Type_DEVICE_SET_VOLUME);
  command_proto.set_command_id(kUniqueID);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());
  std::string payload;
  base::Value::Dict root_dict;
  root_dict.Set(kVolumeFieldName, volume);
  base::JSONWriter::Write(root_dict, &payload);
  command_proto.set_payload(payload);
  return command_proto;
}

std::unique_ptr<RemoteCommandJob> CreateSetVolumeJob(
    base::TimeTicks issued_time,
    int volume) {
  auto job =
      base::WrapUnique<RemoteCommandJob>(new DeviceCommandSetVolumeJob());
  auto set_volume_command_proto = GenerateSetVolumeCommandProto(
      base::TimeTicks::Now() - issued_time, volume);
  EXPECT_TRUE(job->Init(base::TimeTicks::Now(), set_volume_command_proto,
                        em::SignedData()));
  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job->status());
  return job;
}

}  // namespace

class DeviceCommandSetVolumeTest : public ChromeAshTestBase {
 public:
  DeviceCommandSetVolumeTest(const DeviceCommandSetVolumeTest&) = delete;
  DeviceCommandSetVolumeTest& operator=(const DeviceCommandSetVolumeTest&) =
      delete;

 protected:
  DeviceCommandSetVolumeTest() = default;

  // testing::Test
  void SetUp() override;

  base::TimeTicks test_start_time_;
};

void DeviceCommandSetVolumeTest::SetUp() {
  ChromeAshTestBase::SetUp();
  test_start_time_ = base::TimeTicks::Now();
}

void VerifyResults(const RemoteCommandJob& job,
                   int expected_volume,
                   bool expected_muted) {
  EXPECT_EQ(RemoteCommandJob::SUCCEEDED, job.status());
  int volume = ash::CrasAudioHandler::Get()->GetOutputVolumePercent();
  bool muted = ash::CrasAudioHandler::Get()->IsOutputMuted();
  EXPECT_EQ(expected_volume, volume);
  EXPECT_EQ(expected_muted, muted);
}

TEST_F(DeviceCommandSetVolumeTest, NonMuted) {
  const int kVolume = 45;
  auto job = CreateSetVolumeJob(test_start_time_, kVolume);
  base::test::TestFuture<void> job_finished_future;
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       job_finished_future.GetCallback()));
  ASSERT_TRUE(job_finished_future.Wait()) << "Job did not finish.";
  VerifyResults(*job, kVolume, false);
}

TEST_F(DeviceCommandSetVolumeTest, Muted) {
  const int kVolume = 0;
  auto job = CreateSetVolumeJob(test_start_time_, kVolume);
  base::test::TestFuture<void> job_finished_future;
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       job_finished_future.GetCallback()));
  ASSERT_TRUE(job_finished_future.Wait()) << "Job did not finish.";
  VerifyResults(*job, kVolume, true);
}

TEST_F(DeviceCommandSetVolumeTest, VolumeOutOfRange) {
  const int kVolume = 110;
  std::unique_ptr<RemoteCommandJob> job(new DeviceCommandSetVolumeJob());
  auto set_volume_command_proto = GenerateSetVolumeCommandProto(
      base::TimeTicks::Now() - test_start_time_, kVolume);
  EXPECT_FALSE(job->Init(base::TimeTicks::Now(), set_volume_command_proto,
                         em::SignedData()));
  EXPECT_EQ(RemoteCommandJob::INVALID, job->status());
}

TEST_F(DeviceCommandSetVolumeTest, CommandTimeout) {
  auto delta = base::Minutes(10);
  auto job = CreateSetVolumeJob(test_start_time_ - delta, 50);
  EXPECT_FALSE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                        RemoteCommandJob::FinishedCallback()));
  EXPECT_EQ(RemoteCommandJob::EXPIRED, job->status());
}

}  // namespace policy
