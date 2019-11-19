// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/attestation/enrollment_policy_observer.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;
using testing::WithArgs;

namespace chromeos {
namespace attestation {

namespace {

void StatusCallbackSuccess(
    const policy::CloudPolicyClient::StatusCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::BindOnce(callback, true));
}

// A FakeCryptohomeClient that can hold call until told to flush them all.
class CallsHoldingFakeCryptohomeClient : public FakeCryptohomeClient {
 public:
  void set_hold_calls(bool hold_calls) { hold_calls_ = hold_calls; }

  void TpmAttestationGetEnrollmentId(
      bool ignore_cache,
      DBusMethodCallback<TpmAttestationDataResult> callback) override {
    if (hold_calls_) {
      held_calls_.push(base::BindOnce(
          &CallsHoldingFakeCryptohomeClient::DoTpmAttestationGetEnrollmentId,
          base::Unretained(this), ignore_cache, std::move(callback)));
    } else {
      DoTpmAttestationGetEnrollmentId(ignore_cache, std::move(callback));
    }
  }

  void FlushCalls() {
    while (!held_calls_.empty()) {
      std::move(held_calls_.front()).Run();
      held_calls_.pop();
    }
  }

 private:
  void DoTpmAttestationGetEnrollmentId(
      bool ignore_cache,
      DBusMethodCallback<TpmAttestationDataResult> callback) {
    FakeCryptohomeClient::TpmAttestationGetEnrollmentId(ignore_cache,
                                                        std::move(callback));
  }

  bool hold_calls_ = false;
  std::queue<base::OnceClosure> held_calls_;
};

}  // namespace

class EnrollmentPolicyObserverTest : public DeviceSettingsTestBase {
 public:
  EnrollmentPolicyObserverTest() = default;
  ~EnrollmentPolicyObserverTest() override = default;

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();

    policy_client_.SetDMToken("fake_dm_token");

    EXPECT_TRUE(base::HexStringToString(kEnrollmentId, &enrollment_id_));

    // Destroy the DeviceSettingsTestBase fake client and replace it.
    CryptohomeClient::Shutdown();
    // This will be destroyed in DeviceSettingsTestBase::TearDown().
    cryptohome_client_ = new CallsHoldingFakeCryptohomeClient();
    cryptohome_client_->set_tpm_attestation_enrollment_id(
        true /* ignore_cache */, enrollment_id_);
  }

  void TearDown() override {
    observer_.reset();
    DeviceSettingsTestBase::TearDown();
  }

 protected:
  static constexpr char kEnrollmentId[] =
      "6fcc0ebddec3db9500cf82476d594f4d60db934c5b47fa6085c707b2a93e205b";

  void SetUpObserver() {
    observer_ = std::make_unique<EnrollmentPolicyObserver>(
        &policy_client_, device_settings_service_.get(), cryptohome_client_);
    observer_->set_retry_limit(3);
    observer_->set_retry_delay(0);
  }

  void ExpectUploadEnterpriseEnrollmentId(int times) {
    EXPECT_CALL(policy_client_, UploadEnterpriseEnrollmentId(enrollment_id_, _))
        .Times(times)
        .WillRepeatedly(WithArgs<1>(Invoke(StatusCallbackSuccess)));
  }

  void SetUpDevicePolicy(bool enrollment_id_needed) {
    device_policy_->policy_data().set_enrollment_id_needed(
        enrollment_id_needed);
  }

  void PropagateDevicePolicy() {
    ReloadDevicePolicy();
    ReloadDeviceSettings();
  }

  void Run() {
    base::RunLoop().RunUntilIdle();
  }

  // Owned by the global instance, shut down in DeviceSettingsTestBase.
  CallsHoldingFakeCryptohomeClient* cryptohome_client_ = nullptr;

  StrictMock<policy::MockCloudPolicyClient> policy_client_;
  std::unique_ptr<EnrollmentPolicyObserver> observer_;
  std::string enrollment_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EnrollmentPolicyObserverTest);
};

constexpr char EnrollmentPolicyObserverTest::kEnrollmentId[];

TEST_F(EnrollmentPolicyObserverTest, UploadEnterpriseEnrollmentId) {
  SetUpDevicePolicy(true);
  ExpectUploadEnterpriseEnrollmentId(1);
  SetUpObserver();
  PropagateDevicePolicy();
  Run();
}

TEST_F(EnrollmentPolicyObserverTest,
       UploadEnterpriseEnrollmentIdFromExistingPolicy) {
  // This test will trigger the observer work twice in a row: when the
  // observer is created, and when it gets notified later on.
  SetUpDevicePolicy(true);
  PropagateDevicePolicy();
  ExpectUploadEnterpriseEnrollmentId(2);
  SetUpObserver();
  PropagateDevicePolicy();
  Run();
}

TEST_F(EnrollmentPolicyObserverTest,
       UploadEnterpriseEnrollmentIdWithDelayedCallbacks) {
  // We hold calls to cryptohome so that one is still pending by the time the
  // observer gets notified. We expect only one upload despite the concurrent
  // calls.
  cryptohome_client_->set_hold_calls(true);
  SetUpDevicePolicy(true);
  ExpectUploadEnterpriseEnrollmentId(1);
  PropagateDevicePolicy();
  SetUpObserver();
  cryptohome_client_->FlushCalls();
  Run();
}

TEST_F(EnrollmentPolicyObserverTest, FeatureDisabled) {
  SetUpDevicePolicy(false);
  SetUpObserver();
  PropagateDevicePolicy();
  Run();
}

TEST_F(EnrollmentPolicyObserverTest, UnregisteredPolicyClient) {
  policy_client_.SetDMToken("");
  SetUpDevicePolicy(true);
  SetUpObserver();
  PropagateDevicePolicy();
  Run();
}

TEST_F(EnrollmentPolicyObserverTest, DBusFailureRetry) {
  // Simulate a DBus failure.
  cryptohome_client_->SetServiceIsAvailable(false);

  ExpectUploadEnterpriseEnrollmentId(1);

  SetUpDevicePolicy(true);
  PropagateDevicePolicy();
  SetUpObserver();

  // Emulate delayed service initialization.
  // The observer we create synchronously calls TpmAttestationGetEnrollmentId()
  // and fails. During this call, we make the service available in the next
  // run, so on retry, it will successfully return the result.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](FakeCryptohomeClient* cryptohome_client) {
                       cryptohome_client->SetServiceIsAvailable(true);
                     },
                     base::Unretained(cryptohome_client_)));

  Run();
}

}  // namespace attestation
}  // namespace chromeos
