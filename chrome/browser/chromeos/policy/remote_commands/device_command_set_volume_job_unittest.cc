// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/device_command_set_volume_job.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace em = enterprise_management;

namespace {

const RemoteCommandJob::UniqueIDType kUniqueID = 123456789;

// Name of the field in the command payload containing the volume.
const char kVolumeFieldName[] = "volume";

em::RemoteCommand GenerateSetVolumeCommandProto(base::TimeDelta age_of_command,
                                                int volume) {
  em::RemoteCommand command_proto;
  command_proto.set_type(
      enterprise_management::RemoteCommand_Type_DEVICE_SET_VOLUME);
  command_proto.set_command_id(kUniqueID);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());
  std::string payload;
  base::DictionaryValue root_dict;
  root_dict.SetInteger(kVolumeFieldName, volume);
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
  EXPECT_TRUE(
      job->Init(base::TimeTicks::Now(), set_volume_command_proto, nullptr));
  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job->status());
  return job;
}

}  // namespace

class DeviceCommandSetVolumeTest : public ChromeAshTestBase {
 protected:
  DeviceCommandSetVolumeTest();

  // testing::Test
  void SetUp() override;

  base::RunLoop run_loop_;
  base::TimeTicks test_start_time_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceCommandSetVolumeTest);
};

DeviceCommandSetVolumeTest::DeviceCommandSetVolumeTest() {}

void DeviceCommandSetVolumeTest::SetUp() {
  ChromeAshTestBase::SetUp();
  test_start_time_ = base::TimeTicks::Now();
}

void VerifyResults(base::RunLoop* run_loop,
                   RemoteCommandJob* job,
                   int expected_volume,
                   bool expected_muted) {
  EXPECT_EQ(RemoteCommandJob::SUCCEEDED, job->status());
  int volume = chromeos::CrasAudioHandler::Get()->GetOutputVolumePercent();
  bool muted = chromeos::CrasAudioHandler::Get()->IsOutputMuted();
  EXPECT_EQ(expected_volume, volume);
  EXPECT_EQ(expected_muted, muted);
  run_loop->Quit();
}

TEST_F(DeviceCommandSetVolumeTest, NonMuted) {
  const int kVolume = 45;
  auto job = CreateSetVolumeJob(test_start_time_, kVolume);
  EXPECT_TRUE(
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindOnce(&VerifyResults, base::Unretained(&run_loop_),
                              base::Unretained(job.get()), kVolume, false)));
  run_loop_.Run();
}

TEST_F(DeviceCommandSetVolumeTest, Muted) {
  const int kVolume = 0;
  auto job = CreateSetVolumeJob(test_start_time_, kVolume);
  EXPECT_TRUE(
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindOnce(&VerifyResults, base::Unretained(&run_loop_),
                              base::Unretained(job.get()), kVolume, true)));
  run_loop_.Run();
}

TEST_F(DeviceCommandSetVolumeTest, VolumeOutOfRange) {
  const int kVolume = 110;
  std::unique_ptr<RemoteCommandJob> job(new DeviceCommandSetVolumeJob());
  auto set_volume_command_proto = GenerateSetVolumeCommandProto(
      base::TimeTicks::Now() - test_start_time_, kVolume);
  EXPECT_FALSE(
      job->Init(base::TimeTicks::Now(), set_volume_command_proto, nullptr));
  EXPECT_EQ(RemoteCommandJob::INVALID, job->status());
}

TEST_F(DeviceCommandSetVolumeTest, CommandTimeout) {
  auto delta = base::TimeDelta::FromMinutes(10);
  auto job = CreateSetVolumeJob(test_start_time_ - delta, 50);
  EXPECT_FALSE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                        RemoteCommandJob::FinishedCallback()));
  EXPECT_EQ(RemoteCommandJob::EXPIRED, job->status());
}

}  // namespace policy
