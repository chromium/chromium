// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/device_command_start_crd_session_job.h"

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace em = enterprise_management;

namespace {

constexpr char kResultCodeFieldName[] = "resultCode";
constexpr char kResultMessageFieldName[] = "message";
constexpr char kResultAccessCodeFieldName[] = "accessCode";
constexpr char kResultLastActivityFieldName[] = "lastActivitySec";

constexpr RemoteCommandJob::UniqueIDType kUniqueID = 123456789;

constexpr char kTestOAuthToken[] = "test-oauth-token";
constexpr char kTestAccessCode[] = "111122223333";
constexpr char kTestNoOAuthTokenReason[] = "oops-no-oauth-token";
constexpr char kTestNoICEConfigReason[] = "oops-no-ice-config";

constexpr char kIdlenessCutoffFieldName[] = "idlenessCutoffSec";
constexpr char kTerminateUponInputFieldName[] = "terminateUponInput";

em::RemoteCommand GenerateCommandProto(RemoteCommandJob::UniqueIDType unique_id,
                                       base::TimeDelta age_of_command,
                                       base::TimeDelta idleness_cutoff,
                                       bool terminate_upon_input) {
  em::RemoteCommand command_proto;
  command_proto.set_type(
      enterprise_management::RemoteCommand_Type_DEVICE_START_CRD_SESSION);
  command_proto.set_command_id(unique_id);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());

  std::string payload;
  base::Value root_dict(base::Value::Type::DICTIONARY);
  root_dict.SetKey(kIdlenessCutoffFieldName,
                   base::Value((int)idleness_cutoff.InSeconds()));
  root_dict.SetKey(kTerminateUponInputFieldName,
                   base::Value(terminate_upon_input));
  base::JSONWriter::Write(root_dict, &payload);
  command_proto.set_payload(payload);
  return command_proto;
}

class StubCRDHostDelegate : public DeviceCommandStartCRDSessionJob::Delegate {
 public:
  StubCRDHostDelegate(bool has_active_session,
                      bool are_services_ready,
                      bool is_running_kiosk,
                      base::TimeDelta idleness_period,
                      bool oauth_token_success,
                      bool ice_config_success,
                      bool access_code_success);
  ~StubCRDHostDelegate() override;

  bool HasActiveSession() const override;
  void TerminateSession(base::OnceClosure callback) override;

  bool AreServicesReady() const override;
  bool IsRunningKiosk() const override;
  base::TimeDelta GetIdlenessPeriod() const override;

  void FetchOAuthToken(
      DeviceCommandStartCRDSessionJob::OAuthTokenCallback success_callback,
      DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) override;

  void FetchICEConfig(
      const std::string& oauth_token,
      DeviceCommandStartCRDSessionJob::ICEConfigCallback success_callback,
      DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) override;

  void StartCRDHostAndGetCode(
      const std::string& oauth_token,
      base::Value ice_config,
      bool terminate_upon_input,
      DeviceCommandStartCRDSessionJob::AccessCodeCallback success_callback,
      DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) override;

 private:
  bool has_active_session_;
  bool are_services_ready_;
  bool is_running_kiosk_;
  base::TimeDelta idleness_period_;
  bool oauth_token_success_;
  bool ice_config_success_;
  bool access_code_success_;

  DISALLOW_COPY_AND_ASSIGN(StubCRDHostDelegate);
};

StubCRDHostDelegate::StubCRDHostDelegate(bool has_active_session,
                                         bool are_services_ready,
                                         bool is_running_kiosk,
                                         base::TimeDelta idleness_period,
                                         bool oauth_token_success,
                                         bool ice_config_success,
                                         bool access_code_success)
    : has_active_session_(has_active_session),
      are_services_ready_(are_services_ready),
      is_running_kiosk_(is_running_kiosk),
      idleness_period_(idleness_period),
      oauth_token_success_(oauth_token_success),
      ice_config_success_(ice_config_success),
      access_code_success_(access_code_success) {}

StubCRDHostDelegate::~StubCRDHostDelegate() {}

bool StubCRDHostDelegate::HasActiveSession() const {
  return has_active_session_;
}

void StubCRDHostDelegate::TerminateSession(base::OnceClosure callback) {
  has_active_session_ = false;
  std::move(callback).Run();
}

bool StubCRDHostDelegate::AreServicesReady() const {
  return are_services_ready_;
}

bool StubCRDHostDelegate::IsRunningKiosk() const {
  return is_running_kiosk_;
}

base::TimeDelta StubCRDHostDelegate::GetIdlenessPeriod() const {
  return idleness_period_;
}

void StubCRDHostDelegate::FetchOAuthToken(
    DeviceCommandStartCRDSessionJob::OAuthTokenCallback success_callback,
    DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) {
  if (oauth_token_success_) {
    std::move(success_callback).Run(kTestOAuthToken);
  } else {
    std::move(error_callback)
        .Run(DeviceCommandStartCRDSessionJob::FAILURE_NO_OAUTH_TOKEN,
             kTestNoOAuthTokenReason);
  }
}

void StubCRDHostDelegate::FetchICEConfig(
    const std::string& oauth_token,
    DeviceCommandStartCRDSessionJob::ICEConfigCallback success_callback,
    DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) {
  if (ice_config_success_) {
    base::Value ice_config(base::Value::Type::DICTIONARY);
    std::move(success_callback).Run(std::move(ice_config));
  } else {
    std::move(error_callback)
        .Run(DeviceCommandStartCRDSessionJob::FAILURE_NO_ICE_CONFIG,
             kTestNoICEConfigReason);
  }
}

void StubCRDHostDelegate::StartCRDHostAndGetCode(
    const std::string& oauth_token,
    base::Value ice_config,
    bool terminate_upon_input,
    DeviceCommandStartCRDSessionJob::AccessCodeCallback success_callback,
    DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) {
  if (access_code_success_) {
    std::move(success_callback).Run(kTestAccessCode);
  } else {
    std::move(error_callback)
        .Run(DeviceCommandStartCRDSessionJob::FAILURE_CRD_HOST_ERROR,
             std::string());
  }
}

}  // namespace

class DeviceCommandStartCRDSessionJobTest : public testing::Test {
 public:
  void VerifyResults(RemoteCommandJob* job,
                     RemoteCommandJob::Status expected_status,
                     std::string expected_payload);

 protected:
  DeviceCommandStartCRDSessionJobTest();

  // testing::Test:
  void SetUp() override;

  void InitializeJob(RemoteCommandJob* job,
                     RemoteCommandJob::UniqueIDType unique_id,
                     base::TimeTicks issued_time,
                     base::TimeDelta idleness_cutoff,
                     bool terminate_upon_input);

  std::string CreateSuccessPayload(const std::string& access_code);
  std::string CreateErrorPayload(
      DeviceCommandStartCRDSessionJob::ResultCode result_code,
      const std::string& error_message);
  std::string CreateNotIdlePayload(base::TimeDelta idleness);

  base::TimeTicks test_start_time_;

  content::BrowserTaskEnvironment task_environment_;
  base::RunLoop run_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceCommandStartCRDSessionJobTest);
};

DeviceCommandStartCRDSessionJobTest::DeviceCommandStartCRDSessionJobTest() {}

void DeviceCommandStartCRDSessionJobTest::SetUp() {
  test_start_time_ = base::TimeTicks::Now();
}

void DeviceCommandStartCRDSessionJobTest::InitializeJob(
    RemoteCommandJob* job,
    RemoteCommandJob::UniqueIDType unique_id,
    base::TimeTicks issued_time,
    base::TimeDelta idleness_cutoff,
    bool terminate_upon_input) {
  EXPECT_TRUE(job->Init(
      base::TimeTicks::Now(),
      GenerateCommandProto(unique_id, base::TimeTicks::Now() - issued_time,
                           idleness_cutoff, terminate_upon_input),
      nullptr));

  EXPECT_EQ(unique_id, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job->status());
}

std::string DeviceCommandStartCRDSessionJobTest::CreateSuccessPayload(
    const std::string& access_code) {
  std::string payload;
  base::Value root(base::Value::Type::DICTIONARY);
  root.SetKey(kResultCodeFieldName,
              base::Value(DeviceCommandStartCRDSessionJob::SUCCESS));
  root.SetKey(kResultAccessCodeFieldName, base::Value(access_code));
  base::JSONWriter::Write(root, &payload);
  return payload;
}

std::string DeviceCommandStartCRDSessionJobTest::CreateErrorPayload(
    DeviceCommandStartCRDSessionJob::ResultCode result_code,
    const std::string& error_message) {
  std::string payload;
  base::Value root(base::Value::Type::DICTIONARY);
  root.SetKey(kResultCodeFieldName, base::Value(result_code));
  if (!error_message.empty())
    root.SetKey(kResultMessageFieldName, base::Value(error_message));
  base::JSONWriter::Write(root, &payload);
  return payload;
}

std::string DeviceCommandStartCRDSessionJobTest::CreateNotIdlePayload(
    base::TimeDelta idleness) {
  std::string payload;
  base::Value root(base::Value::Type::DICTIONARY);
  root.SetKey(kResultCodeFieldName,
              base::Value(DeviceCommandStartCRDSessionJob::FAILURE_NOT_IDLE));
  root.SetKey(kResultLastActivityFieldName,
              base::Value(static_cast<int>(idleness.InSeconds())));
  base::JSONWriter::Write(root, &payload);
  return payload;
}

void DeviceCommandStartCRDSessionJobTest::VerifyResults(
    RemoteCommandJob* job,
    RemoteCommandJob::Status expected_status,
    std::string expected_payload) {
  EXPECT_EQ(expected_status, job->status());
  std::unique_ptr<std::string> payload = job->GetResultPayload();
  EXPECT_TRUE(payload);
  EXPECT_EQ(expected_payload, *payload);
  run_loop_.Quit();
}

TEST_F(DeviceCommandStartCRDSessionJobTest, Success) {
  StubCRDHostDelegate delegate(
      false /* has_active_session */, true /* are_services_ready */,
      true /* is_running_kiosk */,
      base::TimeDelta::FromHours(1) /* idleness_period */,
      true /* oauth_token_success */, true /* ice_config_success */,
      true /* access_code_success */);

  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandStartCRDSessionJob>(&delegate);
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                false /* terminate_upon_input */);
  bool success = job->Run(
      base::Time::Now(), base::TimeTicks::Now(),
      base::BindOnce(&DeviceCommandStartCRDSessionJobTest::VerifyResults,
                     base::Unretained(this), base::Unretained(job.get()),
                     RemoteCommandJob::SUCCEEDED,
                     CreateSuccessPayload(kTestAccessCode)));
  EXPECT_TRUE(success);
  run_loop_.Run();
}

TEST_F(DeviceCommandStartCRDSessionJobTest, SuccessOldSessionWasRunning) {
  StubCRDHostDelegate delegate(
      true /* has_active_session */, true /* are_services_ready */,
      true /* is_running_kiosk */,
      base::TimeDelta::FromHours(1) /* idleness_period */,
      true /* oauth_token_success */, true /* ice_config_success */,
      true /* access_code_success */);

  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandStartCRDSessionJob>(&delegate);
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                false /* terminate_upon_input */);
  bool success = job->Run(
      base::Time::Now(), base::TimeTicks::Now(),
      base::BindOnce(&DeviceCommandStartCRDSessionJobTest::VerifyResults,
                     base::Unretained(this), base::Unretained(job.get()),
                     RemoteCommandJob::SUCCEEDED,
                     CreateSuccessPayload(kTestAccessCode)));
  EXPECT_TRUE(success);
  run_loop_.Run();
}

TEST_F(DeviceCommandStartCRDSessionJobTest, FailureServicesAreNotReady) {
  StubCRDHostDelegate delegate(
      false /* has_active_session */, false /* are_services_ready */,
      true /* is_running_kiosk */,
      base::TimeDelta::FromHours(1) /* idleness_period */,
      true /* oauth_token_success */, true /* ice_config_success */,
      true /* access_code_success */);

  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandStartCRDSessionJob>(&delegate);
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                false /* terminate_upon_input */);
  bool success = job->Run(
      base::Time::Now(), base::TimeTicks::Now(),
      base::BindOnce(
          &DeviceCommandStartCRDSessionJobTest::VerifyResults,
          base::Unretained(this), base::Unretained(job.get()),
          RemoteCommandJob::FAILED,
          CreateErrorPayload(
              DeviceCommandStartCRDSessionJob::FAILURE_SERVICES_NOT_READY,
              std::string())));
  EXPECT_TRUE(success);
  run_loop_.Run();
}

TEST_F(DeviceCommandStartCRDSessionJobTest, FailureNotAKiosk) {
  StubCRDHostDelegate delegate(
      false /* has_active_session */, true /* are_services_ready */,
      false /* is_running_kiosk */,
      base::TimeDelta::FromHours(1) /* idleness_period */,
      true /* oauth_token_success */, true /* ice_config_success */,
      true /* access_code_success */);

  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandStartCRDSessionJob>(&delegate);
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                false /* terminate_upon_input */);
  bool success = job->Run(
      base::Time::Now(), base::TimeTicks::Now(),
      base::BindOnce(&DeviceCommandStartCRDSessionJobTest::VerifyResults,
                     base::Unretained(this), base::Unretained(job.get()),
                     RemoteCommandJob::FAILED,
                     CreateErrorPayload(
                         DeviceCommandStartCRDSessionJob::FAILURE_NOT_A_KIOSK,
                         std::string())));
  EXPECT_TRUE(success);
  run_loop_.Run();
}

TEST_F(DeviceCommandStartCRDSessionJobTest, FailureNotIdle) {
  StubCRDHostDelegate delegate(
      false /* has_active_session */, true /* are_services_ready */,
      true /* is_running_kiosk */,
      base::TimeDelta::FromSeconds(1) /* idleness_period */,
      true /* oauth_token_success */, true /* ice_config_success */,
      true /* access_code_success */);

  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandStartCRDSessionJob>(&delegate);
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                false /* terminate_upon_input */);
  bool success = job->Run(
      base::Time::Now(), base::TimeTicks::Now(),
      base::BindOnce(&DeviceCommandStartCRDSessionJobTest::VerifyResults,
                     base::Unretained(this), base::Unretained(job.get()),
                     RemoteCommandJob::FAILED,
                     CreateNotIdlePayload(base::TimeDelta::FromSeconds(1))));
  EXPECT_TRUE(success);
  run_loop_.Run();
}

TEST_F(DeviceCommandStartCRDSessionJobTest, TestNoOauthToken) {
  StubCRDHostDelegate delegate(
      false /* has_active_session */, true /* are_services_ready */,
      true /* is_running_kiosk */,
      base::TimeDelta::FromHours(1) /* idleness_period */,
      false /* oauth_token_success */, true /* ice_config_success */,
      true /* access_code_success */);

  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandStartCRDSessionJob>(&delegate);
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                false /* terminate_upon_input */);
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindOnce(
                   &DeviceCommandStartCRDSessionJobTest::VerifyResults,
                   base::Unretained(this), base::Unretained(job.get()),
                   RemoteCommandJob::FAILED,
                   CreateErrorPayload(
                       DeviceCommandStartCRDSessionJob::FAILURE_NO_OAUTH_TOKEN,
                       kTestNoOAuthTokenReason)));
  EXPECT_TRUE(success);
  run_loop_.Run();
}

TEST_F(DeviceCommandStartCRDSessionJobTest, TestErrorRunningCRDHost) {
  StubCRDHostDelegate delegate(
      false /* has_active_session */, true /* are_services_ready */,
      true /* is_running_kiosk */,
      base::TimeDelta::FromHours(1) /* idleness_period */,
      true /* oauth_token_success */, true /* ice_config_success */,
      false /* access_code_success */);

  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandStartCRDSessionJob>(&delegate);
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                false /* terminate_upon_input */);
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindOnce(
                   &DeviceCommandStartCRDSessionJobTest::VerifyResults,
                   base::Unretained(this), base::Unretained(job.get()),
                   RemoteCommandJob::FAILED,
                   CreateErrorPayload(
                       DeviceCommandStartCRDSessionJob::FAILURE_CRD_HOST_ERROR,
                       std::string())));
  EXPECT_TRUE(success);
  run_loop_.Run();
}

}  // namespace policy
