// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/cert_provisioning_ash.h"

#include "base/base64.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/cert_provisioning/mock_cert_provisioning_scheduler.h"
#include "chrome/browser/ash/cert_provisioning/mock_cert_provisioning_worker.h"
#include "chromeos/crosapi/mojom/cert_provisioning.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::TestFuture;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnPointee;
using ::testing::ReturnRef;
using ::testing::SaveArg;

namespace crosapi {
namespace {

// Fake failure message used for tests. The exact content of the message can be
// chosen arbitrarily.
const char kFakeFailureMessage[] = "Failure Message";

// Extracted from a X.509 certificate using the command:
// openssl x509 -pubkey -noout -in cert.pem
// and reformatted as a single line.
// This represents a RSA public key.
constexpr char kDerEncodedSpkiBase64[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA1na7r6WiaL5slsyHI7bEpP5ad9ffsz"
    "T0mBi8yc03hJpxaA3/2/"
    "PX7esUdTSGoZr1XVBxjjJc4AypzZKlsPqYKZ+lPHZPpXlp8JVHn8w8+"
    "zmPKl319vVYdJv5AE0HOuJZ6a19fXxItgzoB+"
    "oXgkA0mhyPygJwF3HMJfJHRrkxJ73c23R6kKvKTxqRKswvzTo5O5AzZFLdCe+"
    "GVTJuPo4VToGd+ZhS7QvsY38nAYG57fMnzzs5jjMF042AzzWiMt9gGbeuqCE6LXqFuSJYPo+"
    "TLaN7pwQx68PK5pd/lv58B7jjxCIAai0BX1rV6bl/Am3EukhTSuIcQiTr5c1G4E6bKwIDAQAB";

constexpr char kCertProfileVersion[] = "cert_profile_version_1";
constexpr base::TimeDelta kCertProfileRenewalPeriod = base::Days(30);
constexpr char kDeviceCertProfileId[] = "device_cert_profile_1";
constexpr char kDeviceCertProfileName[] = "Device Certificate Profile 1";
constexpr char kUserCertProfileId[] = "user_cert_profile_1";
constexpr char kUserCertProfileName[] = "User Certificate Profile 1";
constexpr char kFailedDeviceCertProfileId[] = "failed_device_cert_profile_1";
constexpr char kFailedDeviceCertProfileName[] =
    "Failed Device Certificate Profile 1";
constexpr char kFailedUserCertProfileId[] = "failed_user_cert_profile_1";
constexpr char kFailedUserCertProfileName[] =
    "Failed User Certificate Profile 1";

void SetupMockCertProvisioningWorker(
    ash::cert_provisioning::MockCertProvisioningWorker* worker,
    ash::cert_provisioning::CertProvisioningWorkerState state,
    const std::vector<uint8_t>* public_key,
    ash::cert_provisioning::CertProfile& cert_profile,
    base::Time last_update_time,
    std::optional<ash::cert_provisioning::BackendServerError>& backend_error) {
  EXPECT_CALL(*worker, GetState).WillRepeatedly(Return(state));
  EXPECT_CALL(*worker, GetLastUpdateTime)
      .WillRepeatedly(Return(last_update_time));
  EXPECT_CALL(*worker, GetPublicKey).WillRepeatedly(ReturnPointee(public_key));
  ON_CALL(*worker, GetCertProfile).WillByDefault(ReturnRef(cert_profile));
  ON_CALL(*worker, GetLastBackendServerError)
      .WillByDefault(ReturnRef(backend_error));
}

class MockMojoObserver : public mojom::CertProvisioningObserver {
 public:
  MOCK_METHOD(void, OnStateChanged, (), (override));

  auto GetRemote() { return receiver_.BindNewPipeAndPassRemote(); }

 private:
  mojo::Receiver<crosapi::mojom::CertProvisioningObserver> receiver_{this};
};

class CertProvisioningAshTest : public ::testing::Test {
 public:
  void SetUp() override {
    der_encoded_spki_ = base::Base64Decode(kDerEncodedSpkiBase64).value();

    ON_CALL(user_scheduler_, GetWorkers)
        .WillByDefault(ReturnRef(user_workers_));
    ON_CALL(user_scheduler_, GetFailedCertProfileIds)
        .WillByDefault(ReturnRef(user_failed_workers_));
    ON_CALL(user_scheduler_, AddObserver)
        .WillByDefault(
            Invoke(this, &CertProvisioningAshTest::SaveUserObserver));

    ON_CALL(device_scheduler_, GetWorkers)
        .WillByDefault(ReturnRef(device_workers_));
    ON_CALL(device_scheduler_, GetFailedCertProfileIds)
        .WillByDefault(ReturnRef(device_failed_workers_));
    ON_CALL(device_scheduler_, AddObserver)
        .WillByDefault(
            Invoke(this, &CertProvisioningAshTest::SaveDeviceObserver));
  }

  base::CallbackListSubscription SaveUserObserver(
      base::RepeatingClosure callback) {
    user_scheduler_observer_ = std::move(callback);
    return base::CallbackListSubscription();
  }

  base::CallbackListSubscription SaveDeviceObserver(
      base::RepeatingClosure callback) {
    device_scheduler_observer_ = std::move(callback);
    return base::CallbackListSubscription();
  }

 protected:
  // The mojo service is async, so the tests need to explicitly give it an
  // opportunity to execute its scheduled tasks within their synchronous
  // sequences of calls.
  void ExecuteAsyncTasks() { task_environment_.RunUntilIdle(); }

  std::vector<uint8_t> der_encoded_spki_;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  ash::cert_provisioning::MockCertProvisioningScheduler user_scheduler_;
  ash::cert_provisioning::WorkerMap user_workers_;
  base::flat_map<ash::cert_provisioning::CertProfileId,
                 ash::cert_provisioning::FailedWorkerInfo>
      user_failed_workers_;
  base::RepeatingClosure user_scheduler_observer_;

  ash::cert_provisioning::MockCertProvisioningScheduler device_scheduler_;
  ash::cert_provisioning::WorkerMap device_workers_;
  base::flat_map<ash::cert_provisioning::CertProfileId,
                 ash::cert_provisioning::FailedWorkerInfo>
      device_failed_workers_;
  base::RepeatingClosure device_scheduler_observer_;

  // The CertProvisioningAsh mojo service under test.
  CertProvisioningAsh service_;
};

TEST_F(CertProvisioningAshTest, ObserveSchedulersWhenNeeded) {
  service_.InjectForTesting(&user_scheduler_, &device_scheduler_);

  auto observer_1 = std::make_unique<MockMojoObserver>();
  auto observer_2 = std::make_unique<MockMojoObserver>();
  auto observer_3 = std::make_unique<MockMojoObserver>();

  {
    // AddObserver is called when the mojo service needs to start tracking
    // changes. One observer per scheduler is enough for any number of mojo
    // observers.
    EXPECT_CALL(user_scheduler_, AddObserver);
    EXPECT_CALL(device_scheduler_, AddObserver);

    service_.AddObserver(observer_1->GetRemote());
    service_.AddObserver(observer_2->GetRemote());
    service_.AddObserver(observer_3->GetRemote());

    ExecuteAsyncTasks();
    Mock::VerifyAndClearExpectations(&user_scheduler_);
    Mock::VerifyAndClearExpectations(&device_scheduler_);
  }

  {
    // The mojo service is supposed to stop observing the schedulers without its
    // own observers. It's indirectly verified by the fact that later it adds
    // observers again.
    observer_1.reset();
    observer_2.reset();
    observer_3.reset();

    ExecuteAsyncTasks();
  }

  // Check that the mojo service can start and stop observering several times
  // within its lifetime.
  {
    auto observer_4 = std::make_unique<MockMojoObserver>();

    EXPECT_CALL(user_scheduler_, AddObserver);
    EXPECT_CALL(device_scheduler_, AddObserver);

    service_.AddObserver(observer_4->GetRemote());

    ExecuteAsyncTasks();
    Mock::VerifyAndClearExpectations(&user_scheduler_);
    Mock::VerifyAndClearExpectations(&device_scheduler_);

    observer_4.reset();
    ExecuteAsyncTasks();
  }
}

TEST_F(CertProvisioningAshTest, ChangeNotificationsForwarded) {
  service_.InjectForTesting(&user_scheduler_, &device_scheduler_);
  EXPECT_FALSE(user_scheduler_observer_);
  EXPECT_FALSE(device_scheduler_observer_);

  MockMojoObserver observer_1;
  MockMojoObserver observer_2;
  service_.AddObserver(observer_1.GetRemote());
  service_.AddObserver(observer_2.GetRemote());
  ASSERT_TRUE(user_scheduler_observer_);
  ASSERT_TRUE(device_scheduler_observer_);

  {
    // User changes are propagated.
    EXPECT_CALL(observer_1, OnStateChanged);
    EXPECT_CALL(observer_2, OnStateChanged);

    user_scheduler_observer_.Run();

    ExecuteAsyncTasks();
    Mock::VerifyAndClearExpectations(&observer_1);
    Mock::VerifyAndClearExpectations(&observer_2);
  }

  {
    // Device changes are propagated.
    EXPECT_CALL(observer_1, OnStateChanged);
    EXPECT_CALL(observer_2, OnStateChanged);

    device_scheduler_observer_.Run();

    ExecuteAsyncTasks();
    Mock::VerifyAndClearExpectations(&observer_1);
    Mock::VerifyAndClearExpectations(&observer_2);
  }
}

TEST_F(CertProvisioningAshTest, GetStatusEmpty) {
  service_.InjectForTesting(&user_scheduler_, &device_scheduler_);

  TestFuture<std::vector<mojom::CertProvisioningProcessStatusPtr>> result;
  service_.GetStatus(result.GetCallback());

  EXPECT_EQ(0u, result.Get().size());
}

TEST_F(CertProvisioningAshTest, GetStatusAliveUserWorker) {
  service_.InjectForTesting(&user_scheduler_, &device_scheduler_);

  // Setup a user mock worker.
  ash::cert_provisioning::CertProfile user_cert_profile(
      kUserCertProfileId, kUserCertProfileName, kCertProfileVersion,
      ash::cert_provisioning::KeyType::kRsa,
      /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
      ash::cert_provisioning::ProtocolVersion::kStatic);
  // Any time should work. Any time in the past is a realistic value.
  base::Time last_update_time = base::Time::Now() - base::Hours(1);
  std::optional<ash::cert_provisioning::BackendServerError> backend_error =
      ash::cert_provisioning::BackendServerError(
          policy::DM_STATUS_REQUEST_INVALID, last_update_time);
  auto user_cert_worker =
      std::make_unique<ash::cert_provisioning::MockCertProvisioningWorker>();
  SetupMockCertProvisioningWorker(
      user_cert_worker.get(),
      ash::cert_provisioning::CertProvisioningWorkerState::kKeypairGenerated,
      &der_encoded_spki_, user_cert_profile, last_update_time, backend_error);
  user_workers_[kUserCertProfileId] = std::move(user_cert_worker);

  auto expected_user_status = mojom::CertProvisioningProcessStatus::New();
  expected_user_status->cert_profile_id = kUserCertProfileId;
  expected_user_status->cert_profile_name = kUserCertProfileName;
  expected_user_status->public_key = der_encoded_spki_;
  expected_user_status->last_update_time = last_update_time;
  expected_user_status->last_backend_server_error =
      crosapi::mojom::CertProvisioningBackendServerError::New(
          last_update_time, policy::DM_STATUS_REQUEST_INVALID);
  expected_user_status->state =
      mojom::CertProvisioningProcessState::kKeypairGenerated;
  expected_user_status->did_fail = false;
  expected_user_status->is_device_wide = false;
  expected_user_status->failure_message = std::nullopt;

  TestFuture<std::vector<mojom::CertProvisioningProcessStatusPtr>> result;
  service_.GetStatus(result.GetCallback());

  ASSERT_EQ(1u, result.Get().size());
  EXPECT_EQ(*result.Get()[0], *expected_user_status);
}

TEST_F(CertProvisioningAshTest, GetStatusAliveDeviceWorker) {
  service_.InjectForTesting(&user_scheduler_, &device_scheduler_);

  // Setup a device mock worker.
  ash::cert_provisioning::CertProfile device_cert_profile(
      kDeviceCertProfileId, kDeviceCertProfileName, kCertProfileVersion,
      ash::cert_provisioning::KeyType::kRsa,
      /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
      ash::cert_provisioning::ProtocolVersion::kStatic);
  base::Time last_update_time = base::Time::Now() - base::Hours(2);
  std::optional<ash::cert_provisioning::BackendServerError> backend_error =
      ash::cert_provisioning::BackendServerError(
          policy::DM_STATUS_REQUEST_FAILED, last_update_time);
  auto device_cert_worker =
      std::make_unique<ash::cert_provisioning::MockCertProvisioningWorker>();
  SetupMockCertProvisioningWorker(
      device_cert_worker.get(),
      ash::cert_provisioning::CertProvisioningWorkerState::kSignCsrFinished,
      &der_encoded_spki_, device_cert_profile, last_update_time, backend_error);
  device_workers_[kDeviceCertProfileId] = std::move(device_cert_worker);

  auto expected_device_status = mojom::CertProvisioningProcessStatus::New();
  expected_device_status->cert_profile_id = kDeviceCertProfileId;
  expected_device_status->cert_profile_name = kDeviceCertProfileName;
  expected_device_status->public_key = der_encoded_spki_;
  expected_device_status->last_update_time = last_update_time;
  expected_device_status->last_backend_server_error =
      crosapi::mojom::CertProvisioningBackendServerError::New(
          last_update_time, policy::DM_STATUS_REQUEST_FAILED);
  expected_device_status->state =
      mojom::CertProvisioningProcessState::kSignCsrFinished;
  expected_device_status->did_fail = false;
  expected_device_status->is_device_wide = true;
  expected_device_status->failure_message = std::nullopt;

  TestFuture<std::vector<mojom::CertProvisioningProcessStatusPtr>> result;
  service_.GetStatus(result.GetCallback());

  ASSERT_EQ(1u, result.Get().size());
  EXPECT_EQ(*result.Get()[0], *expected_device_status);
}

TEST_F(CertProvisioningAshTest, GetStatusFailedUserWorker) {
  service_.InjectForTesting(&user_scheduler_, &device_scheduler_);

  base::Time last_update_time = base::Time::Now() - base::Hours(3);

  ash::cert_provisioning::FailedWorkerInfo& info =
      user_failed_workers_[kFailedUserCertProfileId];
  info.state_before_failure =
      ash::cert_provisioning::CertProvisioningWorkerState::kVaChallengeFinished;
  info.public_key = der_encoded_spki_;
  info.cert_profile_name = kFailedUserCertProfileName;
  info.last_update_time = last_update_time;
  info.failure_message = kFakeFailureMessage;

  auto expected_user_status = mojom::CertProvisioningProcessStatus::New();
  expected_user_status->cert_profile_id = kFailedUserCertProfileId;
  expected_user_status->cert_profile_name = kFailedUserCertProfileName;
  expected_user_status->public_key = der_encoded_spki_;
  expected_user_status->last_update_time = last_update_time;
  expected_user_status->state =
      mojom::CertProvisioningProcessState::kVaChallengeFinished;
  expected_user_status->did_fail = true;
  expected_user_status->is_device_wide = false;
  expected_user_status->failure_message = kFakeFailureMessage;

  TestFuture<std::vector<mojom::CertProvisioningProcessStatusPtr>> result;
  service_.GetStatus(result.GetCallback());

  ASSERT_EQ(1u, result.Get().size());
  EXPECT_EQ(*result.Get()[0], *expected_user_status);
}

TEST_F(CertProvisioningAshTest, GetStatusFailedDeviceWorker) {
  service_.InjectForTesting(&user_scheduler_, &device_scheduler_);

  base::Time last_update_time = base::Time::Now() - base::Hours(4);

  ash::cert_provisioning::FailedWorkerInfo& info =
      device_failed_workers_[kFailedDeviceCertProfileId];
  info.state_before_failure = ash::cert_provisioning::
      CertProvisioningWorkerState::kFinishCsrResponseReceived;
  info.public_key = der_encoded_spki_;
  info.cert_profile_name = kFailedDeviceCertProfileName;
  info.last_update_time = last_update_time;
  info.failure_message = kFakeFailureMessage;

  auto expected_device_status = mojom::CertProvisioningProcessStatus::New();
  expected_device_status->cert_profile_id = kFailedDeviceCertProfileId;
  expected_device_status->cert_profile_name = kFailedDeviceCertProfileName;
  expected_device_status->public_key = der_encoded_spki_;
  expected_device_status->last_update_time = last_update_time;
  expected_device_status->state =
      mojom::CertProvisioningProcessState::kFinishCsrResponseReceived;
  expected_device_status->did_fail = true;
  expected_device_status->is_device_wide = true;
  expected_device_status->failure_message = kFakeFailureMessage;

  TestFuture<std::vector<mojom::CertProvisioningProcessStatusPtr>> result;
  service_.GetStatus(result.GetCallback());

  ASSERT_EQ(1u, result.Get().size());
  EXPECT_EQ(*result.Get()[0], *expected_device_status);
}

TEST_F(CertProvisioningAshTest, UpdateOneProcess) {
  service_.InjectForTesting(&user_scheduler_, &device_scheduler_);

  {
    // The service will try different schedulers until it finds the one that
    // contains the profile id.
    EXPECT_CALL(user_scheduler_, UpdateOneWorker("111")).WillOnce(Return(true));
    EXPECT_CALL(device_scheduler_, UpdateOneWorker).Times(0);

    service_.UpdateOneProcess("111");

    ExecuteAsyncTasks();
    Mock::VerifyAndClearExpectations(&user_scheduler_);
    Mock::VerifyAndClearExpectations(&device_scheduler_);
  }

  {
    // If the first one reports that it doesn't own the id, the service will try
    // another one.
    EXPECT_CALL(user_scheduler_, UpdateOneWorker("222"))
        .WillOnce(Return(false));
    EXPECT_CALL(device_scheduler_, UpdateOneWorker("222"))
        .WillOnce(Return(false));

    service_.UpdateOneProcess("222");

    ExecuteAsyncTasks();
    Mock::VerifyAndClearExpectations(&user_scheduler_);
    Mock::VerifyAndClearExpectations(&device_scheduler_);
  }
}

}  // namespace
}  // namespace crosapi
