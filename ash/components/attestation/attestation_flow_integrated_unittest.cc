// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/attestation/attestation_flow_integrated.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/components/attestation/attestation_flow_factory.h"
#include "ash/components/attestation/attestation_flow_utils.h"
#include "ash/components/attestation/mock_attestation_flow.h"
#include "ash/components/cryptohome/cryptohome_parameters.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/attestation/attestation_client.h"
#include "chromeos/dbus/attestation/interface.pb.h"
#include "chromeos/dbus/constants/attestation_constants.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/account_id/account_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace attestation {

namespace {

using testing::_;
using testing::SaveArg;
using testing::StrictMock;

}  // namespace

class AttestationFlowIntegratedTest : public testing::Test {
 public:
  AttestationFlowIntegratedTest() {
    chromeos::AttestationClient::InitializeFake();
  }
  ~AttestationFlowIntegratedTest() override {
    chromeos::AttestationClient::Shutdown();
  }
  void QuitRunLoopCertificateCallback(
      AttestationFlowIntegrated::CertificateCallback callback,
      AttestationStatus status,
      const std::string& cert) {
    run_loop_->Quit();
    if (callback)
      std::move(callback).Run(status, cert);
  }

 protected:
  void AllowlistCertificateRequest(
      ::attestation::ACAType aca_type,
      ::attestation::GetCertificateRequest request) {
    request.set_aca_type(aca_type);
    if (request.key_label().empty()) {
      request.set_key_label(
          GetKeyNameForProfile(static_cast<AttestationCertificateProfile>(
                                   request.certificate_profile()),
                               request.request_origin()));
    }
    chromeos::AttestationClient::Get()
        ->GetTestInterface()
        ->AllowlistCertificateRequest(request);
  }
  void Run() {
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop_->Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop* run_loop_;
  base::HistogramTester histogram_tester_;
};

TEST_F(AttestationFlowIntegratedTest, GetCertificate) {
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparations(true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback1,
      callback2, callback3;
  std::string certificate1, certificate2, certificate3;
  EXPECT_CALL(callback1, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate1));
  EXPECT_CALL(callback2, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate2));
  EXPECT_CALL(callback3, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate3));

  AttestationFlowIntegrated flow;
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(), callback1.Get());
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(), callback2.Get());
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/false, request.key_label(),
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback3.Get()));
  Run();
  EXPECT_FALSE(certificate1.empty());
  EXPECT_FALSE(certificate2.empty());
  EXPECT_NE(certificate1, certificate2);
  EXPECT_EQ(certificate2, certificate3);
  histogram_tester_.ExpectUniqueSample(
      "ChromeOS.Attestation.GetCertificateStatus",
      ::attestation::STATUS_SUCCESS, 3);
}

// This is pretty much identical to `GetCertificate` while the flow under test
// is created by the factory function to make sure that the factory function
// instantiates an object of the intended type.
TEST_F(AttestationFlowIntegratedTest, GetCertificateCreatedByFactory) {
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparations(true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback1,
      callback2, callback3;
  std::string certificate1, certificate2, certificate3;
  EXPECT_CALL(callback1, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate1));
  EXPECT_CALL(callback2, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate2));
  EXPECT_CALL(callback3, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate3));

  AttestationFlowFactory attestation_flow_factory;
  // `AttestationFlowIntegrated` doesn't use `ServerProxy`. Create the factory
  // with a strict mock of `ServerProxy` so we can catch unexpected calls.
  attestation_flow_factory.Initialize(
      std::make_unique<StrictMock<MockServerProxy>>());
  AttestationFlow* flow = attestation_flow_factory.GetDefault();
  flow->GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(), callback1.Get());
  flow->GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(), callback2.Get());
  flow->GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/false, request.key_label(),
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback3.Get()));
  Run();
  EXPECT_FALSE(certificate1.empty());
  EXPECT_FALSE(certificate2.empty());
  EXPECT_NE(certificate1, certificate2);
  EXPECT_EQ(certificate2, certificate3);
  histogram_tester_.ExpectUniqueSample(
      "ChromeOS.Attestation.GetCertificateStatus",
      ::attestation::STATUS_SUCCESS, 3);
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateFailed) {
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparations(true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  AttestationStatus status = AttestationStatus::ATTESTATION_SUCCESS;
  EXPECT_CALL(callback, Run(_, _)).WillOnce(SaveArg<0>(&status));

  AttestationFlowIntegrated flow;
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(),
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_NE(status, AttestationStatus::ATTESTATION_SUCCESS);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.Attestation.GetCertificateStatus",
      ::attestation::STATUS_SUCCESS, 0);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Attestation.GetCertificateStatus", 1);
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateFailedInvalidProfile) {
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparations(true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::CAST_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  AttestationStatus status = AttestationStatus::ATTESTATION_SUCCESS;
  EXPECT_CALL(callback, Run(_, _)).WillOnce(SaveArg<0>(&status));

  AttestationFlowIntegrated flow;
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(),
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_NE(status, AttestationStatus::ATTESTATION_SUCCESS);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Attestation.GetCertificateStatus", 0);
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateAttestationNotPrepared) {
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparationsSequence({false, true});

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  std::string certificate;
  EXPECT_CALL(callback, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate));

  AttestationFlowIntegrated flow;
  flow.set_retry_delay_for_testing(base::Milliseconds(10));
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(),
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_FALSE(certificate.empty());
  histogram_tester_.ExpectUniqueSample(
      "ChromeOS.Attestation.GetCertificateStatus",
      ::attestation::STATUS_SUCCESS, 1);
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateAttestationNeverPrepared) {
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparations(false);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  AttestationStatus status = AttestationStatus::ATTESTATION_SUCCESS;
  EXPECT_CALL(callback, Run(_, _)).WillOnce(SaveArg<0>(&status));

  AttestationFlowIntegrated flow;
  flow.set_ready_timeout_for_testing(base::Milliseconds(10));
  flow.set_retry_delay_for_testing(base::Milliseconds(3));
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(),
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_NE(status, AttestationStatus::ATTESTATION_SUCCESS);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Attestation.GetCertificateStatus", 0);
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateAttestationTestAca) {
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparations(true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");

  AllowlistCertificateRequest(::attestation::ACAType::TEST_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  std::string certificate;
  EXPECT_CALL(callback, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate));

  AttestationFlowIntegrated flow(::attestation::ACAType::TEST_ACA);
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(),
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_FALSE(certificate.empty());
  histogram_tester_.ExpectUniqueSample(
      "ChromeOS.Attestation.GetCertificateStatus",
      ::attestation::STATUS_SUCCESS, 1);
}

TEST_F(AttestationFlowIntegratedTest, GetCertificateAcaTypeFromCommandline) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(chromeos::switches::kAttestationServer,
                                  "test");
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparations(true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_USER_CERTIFICATE);
  request.set_username("username@email.com");
  request.set_key_label("label");
  request.set_request_origin("origin");

  AllowlistCertificateRequest(::attestation::ACAType::TEST_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  std::string certificate;
  EXPECT_CALL(callback, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate));

  AttestationFlowIntegrated flow;
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail(request.username()), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(),
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_FALSE(certificate.empty());
  histogram_tester_.ExpectUniqueSample(
      "ChromeOS.Attestation.GetCertificateStatus",
      ::attestation::STATUS_SUCCESS, 1);
}

TEST_F(AttestationFlowIntegratedTest, GetMachineCertificate) {
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparations(true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_MACHINE_CERTIFICATE);
  request.set_key_label("label");
  request.set_request_origin("origin");

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  std::string certificate;
  EXPECT_CALL(callback, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate));

  AttestationFlowIntegrated flow;
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      EmptyAccountId(), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(),
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_FALSE(certificate.empty());
  histogram_tester_.ExpectUniqueSample(
      "ChromeOS.Attestation.GetCertificateStatus",
      ::attestation::STATUS_SUCCESS, 1);
}

// There used to be an incidence that a non-empty username are sent when
// requesting a device key certificate, and we remove the username in the
// attestation flow process though it is not considered a valid input.
// TODO(b/179364923): Develop a better API design along with strict assertion
// instead of silently removing the username.
TEST_F(AttestationFlowIntegratedTest, GetMachineCertificateWithAccountId) {
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparations(true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_MACHINE_CERTIFICATE);
  request.set_key_label("label");
  request.set_request_origin("origin");

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  std::string certificate;
  EXPECT_CALL(callback, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate));

  AttestationFlowIntegrated flow;
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      AccountId::FromUserEmail("username@gmail.com"), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(),
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_FALSE(certificate.empty());
  histogram_tester_.ExpectUniqueSample(
      "ChromeOS.Attestation.GetCertificateStatus",
      ::attestation::STATUS_SUCCESS, 1);
}

TEST_F(AttestationFlowIntegratedTest,
       GetCertificateAttestationKeyNameFromProfile) {
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparations(true);

  ::attestation::GetCertificateRequest request;
  request.set_certificate_profile(
      ::attestation::CertificateProfile::ENTERPRISE_ENROLLMENT_CERTIFICATE);
  // Note: no key label is set.
  request.set_request_origin("origin");

  AllowlistCertificateRequest(::attestation::ACAType::DEFAULT_ACA, request);

  base::MockCallback<AttestationFlowIntegrated::CertificateCallback> callback;
  std::string certificate;
  EXPECT_CALL(callback, Run(AttestationStatus::ATTESTATION_SUCCESS, _))
      .WillOnce(SaveArg<1>(&certificate));

  AttestationFlowIntegrated flow;
  flow.GetCertificate(
      static_cast<AttestationCertificateProfile>(request.certificate_profile()),
      EmptyAccountId(), request.request_origin(),
      /*generate_new_key=*/true, request.key_label(),
      base::BindOnce(
          &AttestationFlowIntegratedTest::QuitRunLoopCertificateCallback,
          base::Unretained(this), callback.Get()));
  Run();
  EXPECT_FALSE(certificate.empty());
  histogram_tester_.ExpectUniqueSample(
      "ChromeOS.Attestation.GetCertificateStatus",
      ::attestation::STATUS_SUCCESS, 1);
}

}  // namespace attestation
}  // namespace ash
