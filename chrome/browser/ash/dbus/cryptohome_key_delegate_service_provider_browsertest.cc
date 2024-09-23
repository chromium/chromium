// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/cryptohome_key_delegate_service_provider.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/certificate_provider/certificate_provider.h"
#include "chrome/browser/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/certificate_provider/test_certificate_provider_extension.h"
#include "chrome/browser/certificate_provider/test_certificate_provider_extension_mixin.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/constants/cryptohome_key_delegate_constants.h"
#include "chromeos/ash/components/dbus/cryptohome/key.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/services/service_provider_test_helper.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "crypto/signature_verifier.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "extensions/common/features/simple_feature.h"
#include "net/ssl/client_cert_identity.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

namespace ash {

namespace {

// Returns the profile into which login-screen extensions are force-installed.
Profile* GetOriginalSigninProfile() {
  return ProfileHelper::GetSigninProfile()->GetOriginalProfile();
}

}  // namespace

// Tests for the CryptohomeKeyDelegateServiceProvider class.
class CryptohomeKeyDelegateServiceProviderTest
    : public MixinBasedInProcessBrowserTest {
 protected:
  CryptohomeKeyDelegateServiceProviderTest() = default;

  CryptohomeKeyDelegateServiceProviderTest(
      const CryptohomeKeyDelegateServiceProviderTest&) = delete;
  CryptohomeKeyDelegateServiceProviderTest& operator=(
      const CryptohomeKeyDelegateServiceProviderTest&) = delete;

  ~CryptohomeKeyDelegateServiceProviderTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    dbus_service_test_helper_ = std::make_unique<ServiceProviderTestHelper>();
    dbus_service_test_helper_->SetUp(
        cryptohome::kCryptohomeKeyDelegateServiceName,
        dbus::ObjectPath(cryptohome::kCryptohomeKeyDelegateServicePath),
        cryptohome::kCryptohomeKeyDelegateInterface,
        cryptohome::
            kCryptohomeKeyDelegateChallengeKey /* exported_method_name */,
        &service_provider_);

    force_install_mixin_.InitWithDeviceStateMixin(GetOriginalSigninProfile(),
                                                  &device_state_mixin_);
    ASSERT_NO_FATAL_FAILURE(
        test_certificate_provider_extension_mixin_.ForceInstall(
            GetOriginalSigninProfile()));
    // Populate the browser's state with the mapping between the test
    // certificate provider extension and the certs that it provides, so that
    // the tested implementation knows where it should send challenges to. In
    // the real-world usage, this step is done by the Login/Lock Screens while
    // preparing the parameters that are later used by the cryptohomed daemon
    // for calling our D-Bus service.
    RefreshCertsFromCertProviders();
  }

  void TearDownOnMainThread() override {
    dbus_service_test_helper_->TearDown();
    dbus_service_test_helper_.reset();

    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  ServiceProviderTestHelper* dbus_service_test_helper() {
    return dbus_service_test_helper_.get();
  }

  // Refreshes the browser's state from the current certificate providers.
  void RefreshCertsFromCertProviders() {
    chromeos::CertificateProviderService* cert_provider_service =
        chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
            GetOriginalSigninProfile());
    std::unique_ptr<chromeos::CertificateProvider> cert_provider =
        cert_provider_service->CreateCertificateProvider();
    base::RunLoop run_loop;
    cert_provider->GetCertificates(base::BindLambdaForTesting(
        [&](net::ClientCertIdentityList) { run_loop.Quit(); }));
    run_loop.Run();
  }

  cryptohome::KeyChallengeRequest CreateKeyChallengeRequest(
      cryptohome::ChallengeSignatureAlgorithm signature_algorithm) const {
    cryptohome::KeyChallengeRequest request;
    request.set_challenge_type(
        cryptohome::KeyChallengeRequest::CHALLENGE_TYPE_SIGNATURE);
    request.mutable_signature_request_data()->set_data_to_sign(kDataToSign);
    request.mutable_signature_request_data()->set_public_key_spki_der(
        certificate_provider_extension()->GetCertificateSpki());
    request.mutable_signature_request_data()->set_signature_algorithm(
        signature_algorithm);
    return request;
  }

  // Calls the tested ChallengeKey D-Bus method, requesting a signature
  // challenge.
  // Returns whether the D-Bus method succeeded, and on success fills
  // |signature| with the data returned by the call.
  bool CallSignatureChallengeKey(
      cryptohome::ChallengeSignatureAlgorithm signature_algorithm,
      std::vector<uint8_t>* signature) {
    return CallSignatureChallengeKeyWithProto(
        cryptohome::CreateAccountIdentifierFromAccountId(
            user_manager::StubAccountId()),
        CreateKeyChallengeRequest(signature_algorithm), signature);
  }

  // Same as CallSignatureChallengeKey(), but takes parameters in the protobuf
  // format.
  bool CallSignatureChallengeKeyWithProto(
      const cryptohome::AccountIdentifier& account_identifier,
      const cryptohome::KeyChallengeRequest& request,
      std::vector<uint8_t>* signature) {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeKeyDelegateInterface,
        cryptohome::kCryptohomeKeyDelegateChallengeKey);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(account_identifier);
    writer.AppendProtoAsArrayOfBytes(request);
    std::unique_ptr<dbus::Response> dbus_response =
        dbus_service_test_helper_->CallMethod(&method_call);
    if (dbus_response->GetMessageType() == dbus::Message::MESSAGE_ERROR)
      return false;
    dbus::MessageReader reader(dbus_response.get());
    cryptohome::KeyChallengeResponse response;
    EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&response));
    EXPECT_TRUE(response.has_signature_response_data());
    EXPECT_TRUE(response.signature_response_data().has_signature());
    signature->assign(response.signature_response_data().signature().begin(),
                      response.signature_response_data().signature().end());
    return true;
  }

  // Returns whether the given |signature| is a valid signature of the original
  // data.
  bool IsSignatureValid(crypto::SignatureVerifier::SignatureAlgorithm algorithm,
                        const std::vector<uint8_t>& signature) const {
    const std::string spki =
        certificate_provider_extension()->GetCertificateSpki();
    crypto::SignatureVerifier verifier;
    if (!verifier.VerifyInit(algorithm, signature, base::as_byte_span(spki))) {
      return false;
    }
    verifier.VerifyUpdate(base::as_byte_span(kDataToSign));
    return verifier.VerifyFinal();
  }

  TestCertificateProviderExtension* certificate_provider_extension() {
    return test_certificate_provider_extension_mixin_.extension();
  }

  const TestCertificateProviderExtension* certificate_provider_extension()
      const {
    return test_certificate_provider_extension_mixin_.extension();
  }

 private:
  // Data that is passed as an input for the signature challenge request.
  const std::string kDataToSign = "some_data";

  // Bypass "signin_screen" feature only enabled for allowlisted extensions.
  extensions::SimpleFeature::ScopedThreadUnsafeAllowlistForTest
      feature_allowlist_{TestCertificateProviderExtension::extension_id()};

  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  ExtensionForceInstallMixin force_install_mixin_{&mixin_host_};

  CryptohomeKeyDelegateServiceProvider service_provider_;
  std::unique_ptr<ServiceProviderTestHelper> dbus_service_test_helper_;

  TestCertificateProviderExtensionMixin
      test_certificate_provider_extension_mixin_{&mixin_host_,
                                                 &force_install_mixin_};
};

// Verifies that the ChallengeKey request with the PKCS #1 v1.5 SHA-256
// algorithm is handled successfully using the test provider.
IN_PROC_BROWSER_TEST_F(CryptohomeKeyDelegateServiceProviderTest,
                       SignatureSuccessSha256) {
  std::vector<uint8_t> signature;
  EXPECT_TRUE(CallSignatureChallengeKey(
      cryptohome::CHALLENGE_RSASSA_PKCS1_V1_5_SHA256, &signature));
  EXPECT_TRUE(
      IsSignatureValid(crypto::SignatureVerifier::RSA_PKCS1_SHA256, signature));
}

// Verifies that the ChallengeKey request with the PKCS #1 v1.5 SHA-1 algorithm
// is handled successfully using the test provider.
IN_PROC_BROWSER_TEST_F(CryptohomeKeyDelegateServiceProviderTest,
                       SignatureSuccessSha1) {
  std::vector<uint8_t> signature;
  EXPECT_TRUE(CallSignatureChallengeKey(
      cryptohome::CHALLENGE_RSASSA_PKCS1_V1_5_SHA1, &signature));
  EXPECT_TRUE(
      IsSignatureValid(crypto::SignatureVerifier::RSA_PKCS1_SHA1, signature));
}

// Verifies that the ChallengeKey request fails when the requested algorithm
// isn't supported by the test provider.
IN_PROC_BROWSER_TEST_F(CryptohomeKeyDelegateServiceProviderTest,
                       SignatureErrorUnsupportedAlgorithm) {
  std::vector<uint8_t> signature;
  EXPECT_FALSE(CallSignatureChallengeKey(
      cryptohome::CHALLENGE_RSASSA_PKCS1_V1_5_SHA384, &signature));
}

// Verifies that the ChallengeKey request fails when the used key isn't reported
// by the test provider anymore.
IN_PROC_BROWSER_TEST_F(CryptohomeKeyDelegateServiceProviderTest,
                       SignatureErrorKeyRemoved) {
  certificate_provider_extension()->set_should_provide_certificates(false);
  RefreshCertsFromCertProviders();

  std::vector<uint8_t> signature;
  EXPECT_FALSE(CallSignatureChallengeKey(
      cryptohome::CHALLENGE_RSASSA_PKCS1_V1_5_SHA256, &signature));
}

// Verifies that the ChallengeKey request fails when the test provider returns
// an error to the signature request.
IN_PROC_BROWSER_TEST_F(CryptohomeKeyDelegateServiceProviderTest,
                       SignatureErrorWhileSigning) {
  certificate_provider_extension()->set_should_fail_sign_digest_requests(true);

  std::vector<uint8_t> signature;
  EXPECT_FALSE(CallSignatureChallengeKey(
      cryptohome::CHALLENGE_RSASSA_PKCS1_V1_5_SHA256, &signature));
}

// Verifies that the ChallengeKey request fails when no arguments are supplied.
IN_PROC_BROWSER_TEST_F(CryptohomeKeyDelegateServiceProviderTest,
                       SignatureErrorNoArgs) {
  dbus::MethodCall method_call(cryptohome::kCryptohomeKeyDelegateInterface,
                               cryptohome::kCryptohomeKeyDelegateChallengeKey);
  std::unique_ptr<dbus::Response> dbus_response =
      dbus_service_test_helper()->CallMethod(&method_call);
  EXPECT_EQ(dbus_response->GetMessageType(), dbus::Message::MESSAGE_ERROR);
}

// Verifies that the ChallengeKey request fails when non-protobuf data is passed
// for the "account_id" argument.
IN_PROC_BROWSER_TEST_F(CryptohomeKeyDelegateServiceProviderTest,
                       SignatureErrorMistypedAccountIdArg) {
  dbus::MethodCall method_call(cryptohome::kCryptohomeKeyDelegateInterface,
                               cryptohome::kCryptohomeKeyDelegateChallengeKey);
  dbus::MessageWriter writer(&method_call);
  writer.AppendByte(123);
  writer.AppendProtoAsArrayOfBytes(CreateKeyChallengeRequest(
      cryptohome::CHALLENGE_RSASSA_PKCS1_V1_5_SHA256));

  std::unique_ptr<dbus::Response> dbus_response =
      dbus_service_test_helper()->CallMethod(&method_call);
  EXPECT_EQ(dbus_response->GetMessageType(), dbus::Message::MESSAGE_ERROR);
}

// Verifies that the ChallengeKey request fails when the "account_id" argument
// has bad value.
IN_PROC_BROWSER_TEST_F(CryptohomeKeyDelegateServiceProviderTest,
                       SignatureErrorBadAccountIdArg) {
  std::vector<uint8_t> signature;
  EXPECT_FALSE(CallSignatureChallengeKeyWithProto(
      cryptohome::CreateAccountIdentifierFromAccountId(AccountId()),
      CreateKeyChallengeRequest(cryptohome::CHALLENGE_RSASSA_PKCS1_V1_5_SHA256),
      &signature));
}

// Verifies that the ChallengeKey request fails when the "challenge_request"
// argument is missing.
IN_PROC_BROWSER_TEST_F(CryptohomeKeyDelegateServiceProviderTest,
                       SignatureErrorNoRequestArg) {
  dbus::MethodCall method_call(cryptohome::kCryptohomeKeyDelegateInterface,
                               cryptohome::kCryptohomeKeyDelegateChallengeKey);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(
      cryptohome::CreateAccountIdentifierFromAccountId(
          user_manager::StubAccountId()));

  std::unique_ptr<dbus::Response> dbus_response =
      dbus_service_test_helper()->CallMethod(&method_call);
  EXPECT_EQ(dbus_response->GetMessageType(), dbus::Message::MESSAGE_ERROR);
}

// Verifies that the ChallengeKey request fails when non-protobuf data is passed
// for the "challenge_request" argument.
IN_PROC_BROWSER_TEST_F(CryptohomeKeyDelegateServiceProviderTest,
                       SignatureErrorMistypedRequestArg) {
  dbus::MethodCall method_call(cryptohome::kCryptohomeKeyDelegateInterface,
                               cryptohome::kCryptohomeKeyDelegateChallengeKey);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(
      cryptohome::CreateAccountIdentifierFromAccountId(
          user_manager::StubAccountId()));
  writer.AppendByte(123);

  std::unique_ptr<dbus::Response> dbus_response =
      dbus_service_test_helper()->CallMethod(&method_call);
  EXPECT_EQ(dbus_response->GetMessageType(), dbus::Message::MESSAGE_ERROR);
}

// Verifies that the ChallengeKey request fails when the "challenge_type" field
// is unset.
IN_PROC_BROWSER_TEST_F(CryptohomeKeyDelegateServiceProviderTest,
                       SignatureErrorNoChallengeType) {
  cryptohome::KeyChallengeRequest request =
      CreateKeyChallengeRequest(cryptohome::CHALLENGE_RSASSA_PKCS1_V1_5_SHA256);
  request.clear_challenge_type();

  std::vector<uint8_t> signature;
  EXPECT_FALSE(CallSignatureChallengeKeyWithProto(
      cryptohome::CreateAccountIdentifierFromAccountId(
          user_manager::StubAccountId()),
      request, &signature));
}

// Verifies that the ChallengeKey request fails when the
// "signature_request_data" field is unset.
IN_PROC_BROWSER_TEST_F(CryptohomeKeyDelegateServiceProviderTest,
                       SignatureErrorNoRequestData) {
  cryptohome::KeyChallengeRequest request =
      CreateKeyChallengeRequest(cryptohome::CHALLENGE_RSASSA_PKCS1_V1_5_SHA256);
  request.clear_signature_request_data();

  std::vector<uint8_t> signature;
  EXPECT_FALSE(CallSignatureChallengeKeyWithProto(
      cryptohome::CreateAccountIdentifierFromAccountId(
          user_manager::StubAccountId()),
      request, &signature));
}

// Verifies that the ChallengeKey request fails when the "data_to_sign" field is
// unset.
IN_PROC_BROWSER_TEST_F(CryptohomeKeyDelegateServiceProviderTest,
                       SignatureErrorNoDataToSign) {
  cryptohome::KeyChallengeRequest request =
      CreateKeyChallengeRequest(cryptohome::CHALLENGE_RSASSA_PKCS1_V1_5_SHA256);
  request.mutable_signature_request_data()->clear_data_to_sign();

  std::vector<uint8_t> signature;
  EXPECT_FALSE(CallSignatureChallengeKeyWithProto(
      cryptohome::CreateAccountIdentifierFromAccountId(
          user_manager::StubAccountId()),
      request, &signature));
}

// Verifies that the ChallengeKey request fails when the "public_key_spki_der"
// field is unset.
IN_PROC_BROWSER_TEST_F(CryptohomeKeyDelegateServiceProviderTest,
                       SignatureErrorNoPublicKey) {
  cryptohome::KeyChallengeRequest request =
      CreateKeyChallengeRequest(cryptohome::CHALLENGE_RSASSA_PKCS1_V1_5_SHA256);
  request.mutable_signature_request_data()->clear_public_key_spki_der();

  std::vector<uint8_t> signature;
  EXPECT_FALSE(CallSignatureChallengeKeyWithProto(
      cryptohome::CreateAccountIdentifierFromAccountId(
          user_manager::StubAccountId()),
      request, &signature));
}

// Verifies that the ChallengeKey request fails when the "signature_algorithm"
// field is unset.
IN_PROC_BROWSER_TEST_F(CryptohomeKeyDelegateServiceProviderTest,
                       SignatureErrorNoAlgorithm) {
  cryptohome::KeyChallengeRequest request =
      CreateKeyChallengeRequest(cryptohome::CHALLENGE_RSASSA_PKCS1_V1_5_SHA256);
  request.mutable_signature_request_data()->clear_signature_algorithm();

  std::vector<uint8_t> signature;
  EXPECT_FALSE(CallSignatureChallengeKeyWithProto(
      cryptohome::CreateAccountIdentifierFromAccountId(
          user_manager::StubAccountId()),
      request, &signature));
}

}  // namespace ash
