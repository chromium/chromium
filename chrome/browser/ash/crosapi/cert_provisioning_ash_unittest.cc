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
