// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/machine_certificate_uploader_impl.h"

#include <stdint.h>

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/attestation_key_payload.pb.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/ash/components/attestation/fake_certificate.h"
#include "chromeos/ash/components/attestation/mock_attestation_flow.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/attestation/fake_attestation_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArgs;

namespace ash {
namespace attestation {

namespace {

constexpr int64_t kCertValid = 90;
constexpr int64_t kCertExpiringSoon = 20;
constexpr int64_t kCertExpired = -20;
constexpr int kRetryLimit = 3;
constexpr char kFakeCertificate[] = "fake_cert";

void CertCallbackUnspecifiedFailure(
    AttestationFlow::CertificateCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ATTESTATION_UNSPECIFIED_FAILURE, ""));
}

void CertCallbackBadRequestFailure(
    AttestationFlow::CertificateCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                ATTESTATION_SERVER_BAD_REQUEST_FAILURE, ""));
}

void CertCallbackNotAvailableFailure(
    AttestationFlow::CertificateCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ATTESTATION_NOT_AVAILABLE, ""));
}

void ResultCallbackSuccess(policy::CloudPolicyClient::ResultCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), policy::CloudPolicyClient::Result(
                                              policy::DM_STATUS_SUCCESS)));
}

void ResultCallbackNotRegistered(
    policy::CloudPolicyClient::ResultCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     policy::CloudPolicyClient::Result(
                         policy::CloudPolicyClient::NotRegistered())));
}

class CallbackObserver {
 public:
  MOCK_METHOD(void, Callback, (bool state));

  auto GetCallback() {
    return base::BindOnce(&CallbackObserver::Callback, base::Unretained(this));
  }
};

class MockableFakeAttestationFlow : public MockAttestationFlow {
 public:
  MockableFakeAttestationFlow() {
    ON_CALL(*this, GetCertificate(_, _, _, _, _, _, _, _))
        .WillByDefault(
            Invoke(this, &MockableFakeAttestationFlow::GetCertificateInternal));
  }
  ~MockableFakeAttestationFlow() override = default;
  void set_status(AttestationStatus status) { status_ = status; }

 private:
  void GetCertificateInternal(
      AttestationCertificateProfile certificate_profile,
      const AccountId& account_id,
      const std::string& request_origin,
      bool force_new_key,
      ::attestation::KeyType key_crypto_type,
      const std::string& key_name,
      const std::optional<AttestationFlow::CertProfileSpecificData>&
          profile_specific_data,
      CertificateCallback callback) {
    std::string certificate;
    if (status_ == ATTESTATION_SUCCESS) {
      certificate = certificate_;
      AttestationClient::Get()
          ->GetTestInterface()
          ->GetMutableKeyInfoReply(cryptohome::Identification(account_id).id(),
                                   kEnterpriseMachineKey)
          ->set_certificate(certificate_);
    }
    std::move(callback).Run(status_, certificate);
  }
  AttestationStatus status_ = ATTESTATION_SUCCESS;
  const std::string certificate_ = kFakeCertificate;
};

}  // namespace

class MachineCertificateUploaderTestBase : public ::testing::Test {
 public:
  MachineCertificateUploaderTestBase() {
    AttestationClient::InitializeFake();
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    policy_client_.SetDMToken("fake_dm_token");
  }
  ~MachineCertificateUploaderTestBase() override {
    AttestationClient::Shutdown();
  }

 protected:
  enum MockOptions {
    MOCK_KEY_EXISTS = 1,           // Configure so a certified key exists.
    MOCK_KEY_UPLOADED = (1 << 1),  // Configure so an upload has occurred.
    MOCK_NEW_KEY = (1 << 2),       // Configure expecting new key generation.
    MOCK_UNREGISTERED_CLIENT =
        (1 << 3)  // Configure to fake an unregistered cloud policy client.
  };

  // The derived fixture has different needs to control this function's
  // behavior.
  virtual bool GetShouldRefreshCert() const = 0;

  // Configures mock expectations according to |mock_options|.  If options
  // require that a certificate exists, |certificate| will be used.
  void SetupMocks(int mock_options, const std::string& certificate) {
    bool key_exists = (mock_options & MOCK_KEY_EXISTS);
    // Setup expected key / cert queries.
    if (key_exists) {
      ::attestation::GetKeyInfoReply* key_info =
          AttestationClient::Get()->GetTestInterface()->GetMutableKeyInfoReply(
              /*username=*/"", kEnterpriseMachineKey);
      key_info->set_certificate(certificate);
    }

    // Setup expected key payload queries.
    bool key_uploaded = (mock_options & MOCK_KEY_UPLOADED);
    if (key_uploaded) {
      ::attestation::GetKeyInfoReply* key_info =
          AttestationClient::Get()->GetTestInterface()->GetMutableKeyInfoReply(
              /*username=*/"", kEnterpriseMachineKey);
      key_info->set_payload(key_uploaded ? CreatePayload() : std::string());
    }

    // Setup expected key uploads.  Use WillOnce() so StrictMock will trigger an
    // error if our expectations are not met exactly.  We want to verify that
    // during a single run through the uploader only one upload operation occurs
    // (because it is costly) and similarly, that the writing of the uploaded
    // status in the key payload matches the upload operation.
    bool new_key = GetShouldRefreshCert() || (mock_options & MOCK_NEW_KEY);
    if (new_key || !key_uploaded) {
      if (mock_options & MOCK_UNREGISTERED_CLIENT) {
        EXPECT_CALL(policy_client_,
                    UploadEnterpriseMachineCertificate(
                        new_key ? kFakeCertificate : certificate, _))
            .WillOnce(WithArgs<1>(Invoke(ResultCallbackNotRegistered)));
      } else {
        EXPECT_CALL(policy_client_,
                    UploadEnterpriseMachineCertificate(
                        new_key ? kFakeCertificate : certificate, _))
            .WillOnce(WithArgs<1>(Invoke(ResultCallbackSuccess)));
      }
    }

    // Setup expected key generations.  Again use WillOnce().  Key generation is
    // another costly operation and if it gets triggered more than once during
    // a single pass this indicates a logical problem in the observer.
    if (new_key) {
      EXPECT_CALL(attestation_flow_, GetCertificate(_, _, _, _, _, _, _, _));
    }
  }

  void RunUploader(
      base::OnceCallback<void(bool)> upload_callback = base::DoNothing()) {
    MachineCertificateUploaderImpl uploader(&policy_client_,
                                            &attestation_flow_);
    uploader.set_retry_limit_for_testing(kRetryLimit);
    uploader.set_retry_delay_for_testing(base::TimeDelta());
    if (GetShouldRefreshCert())
      uploader.RefreshAndUploadCertificate(base::DoNothing());
    else
      uploader.UploadCertificateIfNeeded(std::move(upload_callback));

    base::RunLoop().RunUntilIdle();
  }

  std::string CreatePayload() {
    AttestationKeyPayload proto;
    proto.set_is_certificate_uploaded(true);
    std::string serialized;
    proto.SerializeToString(&serialized);
    return serialized;
  }

  content::BrowserTaskEnvironment task_environment_;
  ScopedCrosSettingsTestHelper settings_helper_;
  StrictMock<MockableFakeAttestationFlow> attestation_flow_;
  StrictMock<policy::MockCloudPolicyClient> policy_client_;
};

class MachineCertificateUploaderTest
    : public MachineCertificateUploaderTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  bool GetShouldRefreshCert() const final { return GetParam(); }
};

TEST_P(MachineCertificateUploaderTest, UnregisteredPolicyClient) {
  policy_client_.SetDMToken("");
  RunUploader();
}

TEST_P(MachineCertificateUploaderTest, GetCertificateUnspecifiedFailure) {
  EXPECT_CALL(attestation_flow_, GetCertificate(_, _, _, _, _, _, _, _))
      .WillRepeatedly(WithArgs<7>(Invoke(CertCallbackUnspecifiedFailure)));
  RunUploader();
}

TEST_P(MachineCertificateUploaderTest, GetCertificateBadRequestFailure) {
  EXPECT_CALL(attestation_flow_, GetCertificate(_, _, _, _, _, _, _, _))
      .WillOnce(WithArgs<7>(Invoke(CertCallbackBadRequestFailure)));
  RunUploader();
}

TEST_P(MachineCertificateUploaderTest, GetCertificateNotAvailableFailure) {
  EXPECT_CALL(attestation_flow_, GetCertificate(_, _, _, _, _, _, _, _))
      .WillOnce(WithArgs<7>(Invoke(CertCallbackNotAvailableFailure)));
  RunUploader();
}

TEST_P(MachineCertificateUploaderTest, NewCertificate) {
  SetupMocks(MOCK_NEW_KEY, "");
  RunUploader();
  EXPECT_EQ(CreatePayload(),
            AttestationClient::Get()
                ->GetTestInterface()
                ->GetMutableKeyInfoReply(/*username=*/"", kEnterpriseMachineKey)
                ->payload());
}

TEST_P(MachineCertificateUploaderTest, WaitForUploadComplete) {
  SetupMocks(MOCK_NEW_KEY, "");

  StrictMock<CallbackObserver> waiting_callback_observer;
  MachineCertificateUploaderImpl uploader(&policy_client_, &attestation_flow_);

  uploader.WaitForUploadComplete(waiting_callback_observer.GetCallback());
  EXPECT_CALL(waiting_callback_observer, Callback(true));

  StrictMock<CallbackObserver> direct_callback_observer;
  EXPECT_CALL(direct_callback_observer, Callback(true));

  if (GetShouldRefreshCert()) {
    uploader.RefreshAndUploadCertificate(
        direct_callback_observer.GetCallback());
  } else {
    uploader.UploadCertificateIfNeeded(direct_callback_observer.GetCallback());
  }

  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&waiting_callback_observer);
  EXPECT_CALL(waiting_callback_observer, Callback(true));
  uploader.WaitForUploadComplete(waiting_callback_observer.GetCallback());

  EXPECT_EQ(CreatePayload(),
            AttestationClient::Get()
                ->GetTestInterface()
                ->GetMutableKeyInfoReply(/*username=*/"", kEnterpriseMachineKey)
                ->payload());
}

TEST_P(MachineCertificateUploaderTest, WaitForUploadFail) {
  EXPECT_CALL(attestation_flow_, GetCertificate(_, _, _, _, _, _, _, _))
      .WillOnce(WithArgs<7>(Invoke(CertCallbackBadRequestFailure)));

  StrictMock<CallbackObserver> waiting_callback_observer;
  MachineCertificateUploaderImpl uploader(&policy_client_, &attestation_flow_);

  uploader.WaitForUploadComplete(waiting_callback_observer.GetCallback());
  EXPECT_CALL(waiting_callback_observer, Callback(false));

  StrictMock<CallbackObserver> direct_callback_observer;
  EXPECT_CALL(direct_callback_observer, Callback(false));

  if (GetShouldRefreshCert()) {
    uploader.RefreshAndUploadCertificate(
        direct_callback_observer.GetCallback());
  } else {
    uploader.UploadCertificateIfNeeded(direct_callback_observer.GetCallback());
  }

  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&waiting_callback_observer);

  // Consequent calls to `WaitForUploadComplete` will return false until
  // somebody else retries to upload the cert again.
  EXPECT_CALL(waiting_callback_observer, Callback(false));
  uploader.WaitForUploadComplete(waiting_callback_observer.GetCallback());
  base::RunLoop().RunUntilIdle();
}

TEST_P(MachineCertificateUploaderTest, KeyExistsNotUploaded) {
  std::string certificate;
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(kCertValid), &certificate));
  SetupMocks(MOCK_KEY_EXISTS, certificate);
  RunUploader();
  EXPECT_EQ(CreatePayload(),
            AttestationClient::Get()
                ->GetTestInterface()
                ->GetMutableKeyInfoReply(/*username=*/"", kEnterpriseMachineKey)
                ->payload());
}

TEST_P(MachineCertificateUploaderTest, KeyExistsAlreadyUploaded) {
  std::string certificate;
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(kCertValid), &certificate));
  SetupMocks(MOCK_KEY_EXISTS | MOCK_KEY_UPLOADED, certificate);
  RunUploader();
}

TEST_P(MachineCertificateUploaderTest, KeyExistsCertExpiringSoon) {
  std::string certificate;
  ASSERT_TRUE(
      GetFakeCertificatePEM(base::Days(kCertExpiringSoon), &certificate));
  SetupMocks(MOCK_KEY_EXISTS | MOCK_KEY_UPLOADED | MOCK_NEW_KEY, certificate);
  RunUploader();
}

TEST_P(MachineCertificateUploaderTest, KeyExistsCertExpired) {
  std::string certificate;
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(kCertExpired), &certificate));
  SetupMocks(MOCK_KEY_EXISTS | MOCK_KEY_UPLOADED | MOCK_NEW_KEY, certificate);
  RunUploader();
}

TEST_P(MachineCertificateUploaderTest, IgnoreUnknownCertFormat) {
  SetupMocks(MOCK_KEY_EXISTS | MOCK_KEY_UPLOADED, "unsupported");
  RunUploader();
}

TEST_P(MachineCertificateUploaderTest,
       UnregisterPolicyClientDuringCallsReturnsUploadFailure) {
  // We might get unregistered during asynchronous calls. Fake that behaviour
  // here by letting the mock return unregistered.
  SetupMocks(MOCK_NEW_KEY | MOCK_UNREGISTERED_CLIENT, "");

  bool upload_success = false;
  RunUploader(base::BindOnce(
      [](bool* upload_success, bool success) { *upload_success = success; },
      (&upload_success)));

  EXPECT_FALSE(upload_success);
}

INSTANTIATE_TEST_SUITE_P(All,
                         MachineCertificateUploaderTest,
                         testing::Values(false, true));

class MachineCertificateUploaderTestNoRefresh
    : public MachineCertificateUploaderTestBase {
 public:
  bool GetShouldRefreshCert() const final { return false; }
};

TEST_F(MachineCertificateUploaderTestNoRefresh, DBusFailureRetry) {
  SetupMocks(MOCK_NEW_KEY, "");
  AttestationClient::Get()->GetTestInterface()->set_key_info_dbus_error_count(
      kRetryLimit - 1);
  RunUploader();
  EXPECT_EQ(CreatePayload(),
            AttestationClient::Get()
                ->GetTestInterface()
                ->GetMutableKeyInfoReply(/*username=*/"", kEnterpriseMachineKey)
                ->payload());
  EXPECT_EQ(
      AttestationClient::Get()->GetTestInterface()->key_info_dbus_error_count(),
      0);
}

}  // namespace attestation
}  // namespace ash
