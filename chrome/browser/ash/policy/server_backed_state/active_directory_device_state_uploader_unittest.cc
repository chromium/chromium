// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/server_backed_state/active_directory_device_state_uploader.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/enrollment_certificate_uploader_impl.h"
#include "chrome/browser/ash/policy/core/dm_token_storage.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/attestation/mock_attestation_flow.h"
#include "chromeos/ash/components/dbus/attestation/fake_attestation_client.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArgs;

namespace em = enterprise_management;

namespace policy {
namespace {

const char kClientId[] = "fake-client-id";

void CertCallbackSuccess(
    ash::attestation::AttestationFlow::CertificateCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ash::attestation::ATTESTATION_SUCCESS,
                     "fake_cert"));
}

std::string CreateDevicePolicyData(const std::string& policy_value) {
  em::PolicyData policy_data;
  policy_data.set_policy_type(dm_protocol::kChromeDevicePolicyType);
  policy_data.set_policy_value(policy_value);
  return policy_data.SerializeAsString();
}

em::DeviceManagementResponse GetGenericResponse() {
  em::DeviceManagementResponse dm_response;
  dm_response.mutable_policy_response()->add_responses()->set_policy_data(
      CreateDevicePolicyData("fake-policy-data"));
  dm_response.mutable_cert_upload_response();
  return dm_response;
}

em::DeviceManagementResponse GetPolicyResponse() {
  em::DeviceManagementResponse dm_response;
  dm_response.mutable_policy_response()->add_responses()->set_policy_data(
      CreateDevicePolicyData("fake-policy-data"));
  return dm_response;
}

class FakeDMTokenStorage : public DMTokenStorageBase {
 public:
  explicit FakeDMTokenStorage(const std::string& dm_token,
                              base::TimeDelta delay_response)
      : dm_token_(dm_token), delay_response_(delay_response) {}
  void StoreDMToken(const std::string& dm_token,
                    StoreCallback callback) override {
    dm_token_ = dm_token;
    std::move(callback).Run(true);
  }
  void RetrieveDMToken(RetrieveCallback callback) override {
    if (!delay_response_.is_zero()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, base::BindOnce(std::move(callback), dm_token_),
          delay_response_);
    } else {
      std::move(callback).Run(dm_token_);
    }
  }

 private:
  std::string dm_token_;
  base::TimeDelta delay_response_;
};

}  // namespace

using JobConfiguration = DeviceManagementService::JobConfiguration;

class ActiveDirectoryDeviceStateUploaderTest
    : public ash::DeviceSettingsTestBase {
 public:
  ActiveDirectoryDeviceStateUploaderTest()
      : ash::DeviceSettingsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        client_id_(kClientId),
        broker_(&session_manager_client_),
        local_state_(TestingBrowserProcess::GetGlobal()) {}

  ~ActiveDirectoryDeviceStateUploaderTest() override = default;

  void SetUp() override {
    ash::AttestationClient::InitializeFake();
    ash::DeviceSettingsTestBase::SetUp();

    dm_service_.ScheduleInitialization(0);
    base::RunLoop().RunUntilIdle();
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);

    // By default, expect one call of each callback (state keys and enrollment
    // ID).
    expected_run_count_ = 2;
  }

  void TearDown() override {
    uploader_.reset();
    ash::DeviceSettingsTestBase::TearDown();
    ash::AttestationClient::Shutdown();
  }

 protected:
  void ExpectStatus(bool expect_success, bool success) {
    EXPECT_EQ(expect_success, success);
    expected_run_count_--;
    if (expected_run_count_ == 0) {
      run_loop_.Quit();
    }
  }

  // Set expectations for a success of the state keys uploading task.
  void ExpectStateKeysSuccess(bool expect_success) {
    uploader_->SetStateKeysCallbackForTesting(
        base::BindOnce(&ActiveDirectoryDeviceStateUploaderTest::ExpectStatus,
                       base::Unretained(this), expect_success));
  }

  // Set expectations for a success of the enrollment ID uploading task.
  void ExpectEnrollmentIdSuccess(bool expect_success) {
    uploader_->SetEnrollmentIdCallbackForTesting(
        base::BindOnce(&ActiveDirectoryDeviceStateUploaderTest::ExpectStatus,
                       base::Unretained(this), expect_success));
  }

  void CreateUploader(
      const std::string& dm_token,
      base::TimeDelta dm_token_retrieval_delay = base::TimeDelta()) {
    uploader_ = std::make_unique<ActiveDirectoryDeviceStateUploader>(
        client_id_, &dm_service_, &broker_, shared_url_loader_factory_,
        std::make_unique<FakeDMTokenStorage>(dm_token,
                                             dm_token_retrieval_delay),
        local_state_.Get());
    uploader_->SetDeviceSettingsServiceForTesting(
        device_settings_service_.get());
  }

  void InitUploader() {
    CloudPolicyClient* cloud_policy_client =
        uploader_->CreateClientForTesting();

    std::unique_ptr<ash::attestation::EnrollmentCertificateUploaderImpl>
        certificate_uploader_impl = std::make_unique<
            ash::attestation::EnrollmentCertificateUploaderImpl>(
            cloud_policy_client);
    certificate_uploader_impl->set_attestation_flow_for_testing(
        &attestation_flow_);
    certificate_uploader_impl->set_retry_limit_for_testing(0);
    certificate_uploader_impl->set_retry_delay_for_testing(base::TimeDelta());
    uploader_->SetEnrollmentCertificateUploaderForTesting(
        std::move(certificate_uploader_impl));

    uploader_->Init();
  }

  void SetStateKeys(int count) {
    std::vector<std::string> state_keys;
    for (int i = 0; i < count; ++i)
      state_keys.push_back(std::string(1, '1' + i));
    session_manager_client_.set_server_backed_state_keys(state_keys);
  }

  void ExpectJobAndSendResponse(int times,
                                em::DeviceManagementResponse dm_response) {
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .Times(times)
        .WillRepeatedly(DoAll(dm_service_.SendJobOKAsync(dm_response)));
  }

  void ExpectAttestationFlowCall(bool expect_call) {
    if (expect_call) {
      EXPECT_CALL(
          attestation_flow_,
          GetCertificate(
              ash::attestation::PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
              /*force_new_key=*/false, _, _, _, _))
          .WillOnce(WithArgs<7>(Invoke(CertCallbackSuccess)));
    } else {
      EXPECT_CALL(attestation_flow_, GetCertificate(_, _, _, _, _, _, _, _))
          .Times(0);
    }
  }

  base::RunLoop run_loop_;
  int expected_run_count_ = 0;
  std::string client_id_;
  ServerBackedStateKeysBroker broker_;
  StrictMock<MockJobCreationHandler> job_creation_handler_;
  FakeDeviceManagementService dm_service_{&job_creation_handler_};
  ScopedTestingLocalState local_state_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  network::TestURLLoaderFactory url_loader_factory_;
  StrictMock<ash::attestation::MockAttestationFlow> attestation_flow_;
  std::unique_ptr<ActiveDirectoryDeviceStateUploader> uploader_;
};

// Expects to successfully upload state keys and enrollment ID.
TEST_F(ActiveDirectoryDeviceStateUploaderTest, UploadsSucceed) {
  ExpectAttestationFlowCall(/*expect_call=*/true);

  // Expect 2 job creations: 1 for policy fetch and 1 for cert upload.
  ExpectJobAndSendResponse(/*times=*/2, GetGenericResponse());

  SetStateKeys(/*count=*/1);
  CreateUploader(/*dm_token=*/"test-dm-token");

  // After initialization, the uploader should try to send the enrollment ID and
  // the state keys.
  ExpectStateKeysSuccess(/*expect_success=*/true);
  ExpectEnrollmentIdSuccess(/*expect_success=*/true);

  InitUploader();
  run_loop_.Run();

  EXPECT_THAT(uploader_->state_keys_to_upload_for_testing(),
              testing::ElementsAre("1"));

  EXPECT_TRUE(uploader_->HasUploadedEnrollmentId());
}

// Expects to successfully upload state keys and enrollment ID with
// unrealistically delayed DM Token retrieval, so another state keys update
// happen before DM Token is available.
TEST_F(ActiveDirectoryDeviceStateUploaderTest,
       UploadStateKeysWithDelayedDMTokenRetrieval) {
  ExpectAttestationFlowCall(/*expect_call=*/true);

  SetStateKeys(/*count=*/1);
  CreateUploader(/*dm_token=*/"test-dm-token",
                 ServerBackedStateKeysBroker::GetPollIntervalForTesting() * 2);
  // Expect successful uploads of the state keys and enrollment ID (only at the
  // end of this test).
  ExpectStateKeysSuccess(/*expect_success=*/true);
  ExpectEnrollmentIdSuccess(/*expect_success=*/true);
  InitUploader();

  task_environment_.FastForwardBy(
      ServerBackedStateKeysBroker::GetPollIntervalForTesting() / 2);

  // Expect state keys to be updated.
  EXPECT_THAT(uploader_->state_keys_to_upload_for_testing(),
              testing::ElementsAre("1"));

  // Enrollment ID should not have been uploaded yet.
  EXPECT_FALSE(uploader_->HasUploadedEnrollmentId());

  // Expect client not to be registered yet (DM Token retrieval still pending).
  EXPECT_FALSE(uploader_->IsClientRegisteredForTesting());

  // Set new state keys and fast-forward time so state keys update is triggered
  // during DM Token retrieval (which should be still pending).
  SetStateKeys(/*count=*/2);
  task_environment_.FastForwardBy(
      ServerBackedStateKeysBroker::GetPollIntervalForTesting());

  EXPECT_THAT(uploader_->state_keys_to_upload_for_testing(),
              testing::ElementsAre("1", "2"));
  EXPECT_FALSE(uploader_->HasUploadedEnrollmentId());
  EXPECT_FALSE(uploader_->IsClientRegisteredForTesting());

  // Expect 2 job creations: 1 for policy fetch and 1 for cert upload.
  ExpectJobAndSendResponse(/*times=*/2, GetGenericResponse());

  run_loop_.Run();

  EXPECT_TRUE(uploader_->HasUploadedEnrollmentId());
  EXPECT_TRUE(uploader_->IsClientRegisteredForTesting());
}

// Expects to trigger state keys uploads after state keys update multiple times.
// The enrollment ID should be uploaded only once.
TEST_F(ActiveDirectoryDeviceStateUploaderTest, UploadStateKeysMultipleTimes) {
  ExpectAttestationFlowCall(/*expect_call=*/true);

  // Expect 4 job creations: 3 for policy fetch and 1 for cert upload.
  ExpectJobAndSendResponse(/*times=*/4, GetGenericResponse());

  SetStateKeys(/*count=*/1);
  CreateUploader(/*dm_token=*/"test-dm-token");

  // After initialization, the uploader should send the enrollment ID and the
  // state keys.
  ExpectStateKeysSuccess(/*expect_success=*/true);
  ExpectEnrollmentIdSuccess(/*expect_success=*/true);

  InitUploader();
  run_loop_.Run();
  EXPECT_THAT(uploader_->state_keys_to_upload_for_testing(),
              testing::ElementsAre("1"));
  EXPECT_TRUE(uploader_->HasUploadedEnrollmentId());

  // Set new state keys and fast-forward time to trigger state keys update.
  ExpectStateKeysSuccess(/*expect_success=*/true);
  SetStateKeys(/*count=*/2);
  task_environment_.FastForwardBy(
      ServerBackedStateKeysBroker::GetPollIntervalForTesting());

  EXPECT_THAT(uploader_->state_keys_to_upload_for_testing(),
              testing::ElementsAre("1", "2"));

  // Set new state keys and fast-forward time to trigger state keys update.
  ExpectStateKeysSuccess(/*expect_success=*/true);
  SetStateKeys(/*count=*/3);
  task_environment_.FastForwardBy(
      ServerBackedStateKeysBroker::GetPollIntervalForTesting());

  EXPECT_THAT(uploader_->state_keys_to_upload_for_testing(),
              testing::ElementsAre("1", "2", "3"));
}

// Expects to fail when uploading state keys and enrollment ID with empty DM
// Token.
TEST_F(ActiveDirectoryDeviceStateUploaderTest, UploadBothEmptyTokenFailure) {
  ExpectAttestationFlowCall(/*expect_call=*/false);

  SetStateKeys(/*count=*/1);
  CreateUploader(/*dm_token=*/"");

  // After initialization, the uploader should not send anything.
  ExpectStateKeysSuccess(/*expect_success=*/false);
  ExpectEnrollmentIdSuccess(/*expect_success=*/false);

  InitUploader();
  run_loop_.Run();

  EXPECT_FALSE(uploader_->IsClientRegisteredForTesting());
  EXPECT_FALSE(uploader_->HasUploadedEnrollmentId());
}

// Expect to fail when uploading state keys without initializing them first. The
// enrollment ID should be uploaded normally.
TEST_F(ActiveDirectoryDeviceStateUploaderTest,
       UploadBothEmptyStateKeysFailure) {
  ExpectAttestationFlowCall(/*expect_call=*/true);

  // Expect 1 job creation, for cert upload.
  ExpectJobAndSendResponse(/*times=*/1, GetGenericResponse());
  CreateUploader(/*dm_token=*/"test-dm-token");

  // After initialization, the uploader should send the enrollment ID.
  ExpectStateKeysSuccess(/*expect_success=*/false);
  ExpectEnrollmentIdSuccess(/*expect_success=*/true);

  InitUploader();
  run_loop_.Run();

  EXPECT_TRUE(uploader_->IsClientRegisteredForTesting());
  EXPECT_THAT(uploader_->state_keys_to_upload_for_testing(),
              testing::IsEmpty());
  EXPECT_TRUE(uploader_->HasUploadedEnrollmentId());
}

// Expects to upload only state keys - no enrollment ID.
TEST_F(ActiveDirectoryDeviceStateUploaderTest, AlreadyUploadedEnrollmentId) {
  ExpectAttestationFlowCall(/*expect_call=*/false);

  // Expect 1 job creation for policy fetch.
  ExpectJobAndSendResponse(/*times=*/1, GetGenericResponse());

  SetStateKeys(/*count=*/1);
  CreateUploader(/*dm_token=*/"test-dm-token");

  // Simulates Enrollment ID already uploaded.
  uploader_->SetEnrollmentIdUploadedForTesting(/*value=*/true);

  // After initialization, the uploader should send the state keys.
  ExpectStateKeysSuccess(/*expect_success=*/true);
  ExpectEnrollmentIdSuccess(/*expect_success=*/false);

  InitUploader();
  run_loop_.Run();

  EXPECT_THAT(uploader_->state_keys_to_upload_for_testing(),
              testing::ElementsAre("1"));

  // This pref should still be true.
  EXPECT_TRUE(uploader_->HasUploadedEnrollmentId());
}

// Expects to upload only state keys - no enrollment ID.
TEST_F(ActiveDirectoryDeviceStateUploaderTest, EnrollmentIdUploadFails) {
  ExpectAttestationFlowCall(/*expect_call=*/true);

  // Expect 2 job creations: 1 for policy fetch and 1 for cert upload. Sends a
  // response that indicates a certificate upload failure.
  ExpectJobAndSendResponse(/*times=*/2, GetPolicyResponse());

  SetStateKeys(/*count=*/1);
  CreateUploader(/*dm_token=*/"test-dm-token");

  // After initialization, the uploader should send the state keys.
  ExpectStateKeysSuccess(/*expect_success=*/true);
  ExpectEnrollmentIdSuccess(/*expect_success=*/false);

  InitUploader();
  run_loop_.Run();

  EXPECT_THAT(uploader_->state_keys_to_upload_for_testing(),
              testing::ElementsAre("1"));

  EXPECT_FALSE(uploader_->HasUploadedEnrollmentId());
}

}  // namespace policy
