// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/tpm_enrollment_key_signing_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/attestation/attestation.pb.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kFakeChallenge[] = "fake challenge";

}  // namespace

namespace policy {

class TpmEnrollmentKeySigningServiceTest : public testing::Test {
 public:
  TpmEnrollmentKeySigningServiceTest() {
    ash::AttestationClient::InitializeFake();
  }
  ~TpmEnrollmentKeySigningServiceTest() override {
    ash::AttestationClient::Shutdown();
  }

  static void SaveChallengeResponseAndRunCallback(
      base::OnceClosure callback,
      bool* out_success,
      enterprise_management::SignedData* out_signed_data,
      bool in_success,
      enterprise_management::SignedData in_signed_data) {
    *out_success = in_success;
    *out_signed_data = in_signed_data;
    std::move(callback).Run();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(TpmEnrollmentKeySigningServiceTest, SigningSuccess) {
  ash::AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistSignSimpleChallengeKey(
          /*username=*/"", ash::attestation::kEnterpriseEnrollmentKey);

  base::RunLoop run_loop;
  bool returned_success = false;
  enterprise_management::SignedData returned_signed_data;
  TpmEnrollmentKeySigningService signing_service;
  signing_service.SignData(
      kFakeChallenge, base::BindOnce(TpmEnrollmentKeySigningServiceTest::
                                         SaveChallengeResponseAndRunCallback,
                                     run_loop.QuitClosure(), &returned_success,
                                     &returned_signed_data));
  run_loop.Run();
  EXPECT_TRUE(returned_success);
  ::attestation::SignedData result_challenge_response;
  result_challenge_response.set_data(returned_signed_data.data());
  result_challenge_response.set_signature(returned_signed_data.signature());
  EXPECT_TRUE(ash::AttestationClient::Get()
                  ->GetTestInterface()
                  ->VerifySimpleChallengeResponse(kFakeChallenge,
                                                  result_challenge_response));
}

TEST_F(TpmEnrollmentKeySigningServiceTest, SigningFailure) {
  // This is expected to be failed because we don't allowslit any key in
  // `FakeAttestationClient`.
  base::RunLoop run_loop;
  bool returned_success = true;
  enterprise_management::SignedData returned_signed_data;
  TpmEnrollmentKeySigningService signing_service;
  signing_service.SignData(kFakeChallenge,
                           BindOnce(TpmEnrollmentKeySigningServiceTest::
                                        SaveChallengeResponseAndRunCallback,
                                    run_loop.QuitClosure(), &returned_success,
                                    &returned_signed_data));
  run_loop.Run();
  EXPECT_FALSE(returned_success);
}

}  // namespace policy
