// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/chromeos/certificate_provider/test_certificate_provider_extension.h"
#include "chrome/browser/chromeos/dbus/cryptohome_key_delegate_service_provider.h"
#include "chrome/browser/chromeos/policy/signin_profile_extensions_policy_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/constants/cryptohome_key_delegate_constants.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/services/service_provider_test_helper.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_context.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "extensions/test/test_background_page_first_load_observer.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/client_cert_identity.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

namespace {

// Extracts the SubjectPublicKeyInfo from the given certificate.
std::string GetCertSpki(const net::X509Certificate& certificate) {
  base::StringPiece spki_bytes;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(certificate.cert_buffer()),
          &spki_bytes)) {
    return {};
  }
  return spki_bytes.as_string();
}

}  // namespace

// Tests for the CryptohomeKeyDelegateServiceProvider class.
class CryptohomeKeyDelegateServiceProviderTest
    : public policy::SigninProfileExtensionsPolicyTestBase {
 protected:
  CryptohomeKeyDelegateServiceProviderTest()
      : policy::SigninProfileExtensionsPolicyTestBase(
            version_info::Channel::UNKNOWN) {}

  ~CryptohomeKeyDelegateServiceProviderTest() override {}

  void SetUpOnMainThread() override {
    policy::SigninProfileExtensionsPolicyTestBase::SetUpOnMainThread();

    dbus_service_test_helper_ =
        std::make_unique<chromeos::ServiceProviderTestHelper>();
    dbus_service_test_helper_->SetUp(
        cryptohome::kCryptohomeKeyDelegateServiceName,
        dbus::ObjectPath(cryptohome::kCryptohomeKeyDelegateServicePath),
        cryptohome::kCryptohomeKeyDelegateInterface,
        cryptohome::
            kCryptohomeKeyDelegateChallengeKey /* exported_method_name */,
        &service_provider_);

    PrepareExtension();

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

    cert_provider_extension_.reset();

    policy::SigninProfileExtensionsPolicyTestBase::TearDownOnMainThread();
  }

  // Refreshes the browser's state from the current certificate providers.
  void RefreshCertsFromCertProviders() {
    chromeos::CertificateProviderService* cert_provider_service =
        chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
            GetInitialProfile());
    std::unique_ptr<chromeos::CertificateProvider> cert_provider =
        cert_provider_service->CreateCertificateProvider();
    base::RunLoop run_loop;
    cert_provider->GetCertificates(base::BindLambdaForTesting(
        [&](net::ClientCertIdentityList) { run_loop.Quit(); }));
    run_loop.Run();
  }

  // Calls the tested ChallengeKey D-Bus method, requesting a signature
  // challenge.
  // Returns whether the D-Bus method succeeded, and on success fills
  // |signature| with the data returned by the call.
  bool CallSignatureChallengeKey(
      cryptohome::ChallengeSignatureAlgorithm signature_algorithm,
      std::vector<uint8_t>* signature) {
    const cryptohome::AccountIdentifier account_identifier =
        cryptohome::CreateAccountIdentifierFromAccountId(
            user_manager::StubAccountId());
    cryptohome::KeyChallengeRequest request;
    request.set_challenge_type(
        cryptohome::KeyChallengeRequest::CHALLENGE_TYPE_SIGNATURE);
    request.mutable_signature_request_data()->set_data_to_sign(kDataToSign);
    request.mutable_signature_request_data()->set_public_key_spki_der(
        GetCertSpki(*cert_provider_extension_->certificate()));
    request.mutable_signature_request_data()->set_signature_algorithm(
        signature_algorithm);

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
        GetCertSpki(*cert_provider_extension_->certificate());
    crypto::SignatureVerifier verifier;
    if (!verifier.VerifyInit(algorithm, signature,
                             base::as_bytes(base::make_span(spki)))) {
      return false;
    }
    verifier.VerifyUpdate(base::as_bytes(base::make_span(kDataToSign)));
    return verifier.VerifyFinal();
  }

  TestCertificateProviderExtension* cert_provider_extension() {
    return cert_provider_extension_.get();
  }

 private:
  // Test certificate provider extension:
  const std::string kExtensionId = "ecmhnokcdiianioonpgakiooenfnonid";
  const std::string kExtensionUpdateManifestPath =
      "/extensions/test_certificate_provider/update_manifest.xml";

  // Data that is passed as an input for the signature challenge request.
  const std::string kDataToSign = "some_data";

  // Installs the certificate provider extension into the sign-in profile.
  void PrepareExtension() {
    Profile* const profile = GetInitialProfile();
    cert_provider_extension_ =
        std::make_unique<TestCertificateProviderExtension>(profile,
                                                           kExtensionId);
    extensions::TestBackgroundPageFirstLoadObserver bg_page_first_load_observer(
        profile, kExtensionId);
    AddExtensionForForceInstallation(kExtensionId,
                                     kExtensionUpdateManifestPath);
    bg_page_first_load_observer.Wait();
  }

  chromeos::CryptohomeKeyDelegateServiceProvider service_provider_;
  std::unique_ptr<chromeos::ServiceProviderTestHelper>
      dbus_service_test_helper_;
  std::unique_ptr<TestCertificateProviderExtension> cert_provider_extension_;
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
  cert_provider_extension()->set_should_fail_certificate_requests(true);
  RefreshCertsFromCertProviders();

  std::vector<uint8_t> signature;
  EXPECT_FALSE(CallSignatureChallengeKey(
      cryptohome::CHALLENGE_RSASSA_PKCS1_V1_5_SHA256, &signature));
}

// Verifies that the ChallengeKey request fails when the test provider returns
// an error to the signature request.
IN_PROC_BROWSER_TEST_F(CryptohomeKeyDelegateServiceProviderTest,
                       SignatureErrorWhileSigning) {
  cert_provider_extension()->set_should_fail_sign_digest_requests(true);

  std::vector<uint8_t> signature;
  EXPECT_FALSE(CallSignatureChallengeKey(
      cryptohome::CHALLENGE_RSASSA_PKCS1_V1_5_SHA256, &signature));
}
