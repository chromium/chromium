// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_refresh_machine_certificate_job.h"

#include <memory>
#include <utility>

#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/mock_machine_certificate_uploader.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

using ::ash::attestation::MachineCertificateUploader;
using ::ash::attestation::MockMachineCertificateUploader;
using ::testing::Invoke;
using ::testing::StrictMock;

constexpr base::TimeDelta kWorldAge = base::Days(365);
constexpr base::TimeDelta kDefaultCommandAge = base::Minutes(10);
constexpr base::TimeDelta kVeryOldCommandAge = base::Days(175);
constexpr base::TimeDelta kTooOldCommandAge = base::Days(181);

std::unique_ptr<RemoteCommandJob> CreateRefreshMachineCertificateJob(
    const base::TimeDelta& age_of_command,
    const base::TimeTicks& now,
    MachineCertificateUploader* cert_uploader) {
  // Create the job proto.
  enterprise_management::RemoteCommand command_proto;
  command_proto.set_type(
      enterprise_management::
          RemoteCommand_Type_DEVICE_REFRESH_ENTERPRISE_MACHINE_CERTIFICATE);
  constexpr RemoteCommandJob::UniqueIDType kUniqueID = 123456789;
  command_proto.set_command_id(kUniqueID);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());

  // Create the job and validate.
  auto job = std::make_unique<DeviceCommandRefreshMachineCertificateJob>(
      cert_uploader);

  EXPECT_TRUE(
      job->Init(now, command_proto, enterprise_management::SignedData()));
  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job->status());

  return job;
}

}  // namespace

class DeviceCommandRefreshMachineCertificateJobTest : public testing::Test {
 public:
  DeviceCommandRefreshMachineCertificateJobTest(
      const DeviceCommandRefreshMachineCertificateJobTest&) = delete;
  DeviceCommandRefreshMachineCertificateJobTest& operator=(
      const DeviceCommandRefreshMachineCertificateJobTest&) = delete;

 protected:
  DeviceCommandRefreshMachineCertificateJobTest() {
    const base::Time initial_time = base::Time() + kWorldAge;
    const base::TimeTicks initial_time_ticks = base::TimeTicks() + kWorldAge;
    fake_clock_.SetNow(initial_time);
    fake_tick_clock_.SetNowTicks(initial_time_ticks);
  }

  ~DeviceCommandRefreshMachineCertificateJobTest() override = default;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::TestFuture<void> job_finished_future;

  base::SimpleTestClock fake_clock_;
  base::SimpleTestTickClock fake_tick_clock_;

  StrictMock<MockMachineCertificateUploader> cert_uploader_;
};

TEST_F(DeviceCommandRefreshMachineCertificateJobTest,
       ReturnsFailureWithoutCertificateUploader) {
  std::unique_ptr<RemoteCommandJob> job = CreateRefreshMachineCertificateJob(
      kDefaultCommandAge, fake_tick_clock_.NowTicks(),
      /*cert_uploader=*/nullptr);

  EXPECT_CALL(cert_uploader_, RefreshAndUploadCertificate).Times(0);

  EXPECT_TRUE(job->Run(fake_clock_.Now(), fake_tick_clock_.NowTicks(),
                       job_finished_future.GetCallback()));
  ASSERT_TRUE(job_finished_future.Wait()) << "Job did not finish.";

  EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
}

TEST_F(DeviceCommandRefreshMachineCertificateJobTest,
       ReturnsFailureWhenCertificateUploaderFails) {
  std::unique_ptr<RemoteCommandJob> job = CreateRefreshMachineCertificateJob(
      kDefaultCommandAge, fake_tick_clock_.NowTicks(), &cert_uploader_);

  EXPECT_CALL(cert_uploader_, RefreshAndUploadCertificate)
      .WillOnce(Invoke([](MachineCertificateUploader::UploadCallback callback) {
        std::move(callback).Run(/*success=*/false);
      }));

  EXPECT_TRUE(job->Run(fake_clock_.Now(), fake_tick_clock_.NowTicks(),
                       job_finished_future.GetCallback()));
  ASSERT_TRUE(job_finished_future.Wait()) << "Job did not finish.";

  EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
}

TEST_F(DeviceCommandRefreshMachineCertificateJobTest,
       ReturnsSucessWhenCertificateUploaderSucceedes) {
  std::unique_ptr<RemoteCommandJob> job = CreateRefreshMachineCertificateJob(
      kDefaultCommandAge, fake_tick_clock_.NowTicks(), &cert_uploader_);

  EXPECT_CALL(cert_uploader_, RefreshAndUploadCertificate)
      .WillOnce(Invoke([](MachineCertificateUploader::UploadCallback callback) {
        std::move(callback).Run(/*success=*/true);
      }));

  EXPECT_TRUE(job->Run(fake_clock_.Now(), fake_tick_clock_.NowTicks(),
                       job_finished_future.GetCallback()));
  ASSERT_TRUE(job_finished_future.Wait()) << "Job did not finish.";

  EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
}

TEST_F(DeviceCommandRefreshMachineCertificateJobTest,
       ReturnsSuccessForAlmostExpiredCommand) {
  std::unique_ptr<RemoteCommandJob> job = CreateRefreshMachineCertificateJob(
      kVeryOldCommandAge, fake_tick_clock_.NowTicks(), &cert_uploader_);

  EXPECT_CALL(cert_uploader_, RefreshAndUploadCertificate)
      .WillOnce(Invoke([](MachineCertificateUploader::UploadCallback callback) {
        std::move(callback).Run(/*success=*/true);
      }));

  EXPECT_TRUE(job->Run(fake_clock_.Now(), fake_tick_clock_.NowTicks(),
                       job_finished_future.GetCallback()));
  ASSERT_TRUE(job_finished_future.Wait()) << "Job did not finish.";

  EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
}

TEST_F(DeviceCommandRefreshMachineCertificateJobTest,
       ReturnsExpiredForExpiredCommand) {
  std::unique_ptr<RemoteCommandJob> job = CreateRefreshMachineCertificateJob(
      kTooOldCommandAge, fake_tick_clock_.NowTicks(), &cert_uploader_);

  EXPECT_CALL(cert_uploader_, RefreshAndUploadCertificate).Times(0);

  EXPECT_FALSE(job->Run(fake_clock_.Now(), fake_tick_clock_.NowTicks(),
                        RemoteCommandJob::FinishedCallback()));

  EXPECT_EQ(job->status(), RemoteCommandJob::EXPIRED);
}

}  // namespace policy
