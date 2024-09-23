// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_provider/certificate_provider_service.h"

#include <stdint.h>

#include <set>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/certificate_provider/certificate_provider.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace chromeos {

namespace {

const char kExtension1[] = "extension1";
const char kExtension2[] = "extension2";

void ExpectEmptySignatureAndStoreError(net::Error* out_error,
                                       net::Error error,
                                       const std::vector<uint8_t>& signature) {
  EXPECT_TRUE(signature.empty());
  *out_error = error;
}

void ExpectOKAndStoreSignature(std::vector<uint8_t>* out_signature,
                               net::Error error,
                               const std::vector<uint8_t>& signature) {
  EXPECT_EQ(net::OK, error);
  *out_signature = signature;
}

void StoreCertificates(net::ClientCertIdentityList* out_certs,
                       net::ClientCertIdentityList certs) {
  if (out_certs)
    *out_certs = std::move(certs);
}

void StorePrivateKey(scoped_refptr<net::SSLPrivateKey>* out_key,
                     scoped_refptr<net::SSLPrivateKey> in_key) {
  *out_key = std::move(in_key);
}

certificate_provider::CertificateInfo CreateCertInfo(
    const std::string& cert_filename) {
  certificate_provider::CertificateInfo cert_info;
  cert_info.certificate =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), cert_filename);
  EXPECT_TRUE(cert_info.certificate) << "Could not load " << cert_filename;
  cert_info.supported_algorithms.push_back(SSL_SIGN_RSA_PKCS1_SHA256);

  return cert_info;
}

bool IsKeyEqualToCertInfo(const certificate_provider::CertificateInfo& info,
                          net::SSLPrivateKey* key) {
  return info.supported_algorithms == key->GetAlgorithmPreferences();
}

bool ClientCertIdentityAlphabeticSorter(
    const std::unique_ptr<net::ClientCertIdentity>& a_identity,
    const std::unique_ptr<net::ClientCertIdentity>& b_identity) {
  return a_identity->certificate()->subject().GetDisplayName() <
         b_identity->certificate()->subject().GetDisplayName();
}

class TestDelegate : public CertificateProviderService::Delegate {
 public:
  enum class RequestType { NONE, SIGN, GET_CERTIFICATES };

  TestDelegate() {}
  TestDelegate(const TestDelegate&) = delete;
  TestDelegate& operator=(const TestDelegate&) = delete;

  std::vector<std::string> CertificateProviderExtensions() override {
    return std::vector<std::string>(provider_extensions_.begin(),
                                    provider_extensions_.end());
  }

  void BroadcastCertificateRequest(int cert_request_id) override {
    EXPECT_EQ(expected_request_type_, RequestType::GET_CERTIFICATES);
    last_cert_request_id_ = cert_request_id;
    expected_request_type_ = RequestType::NONE;
  }

  bool DispatchSignRequestToExtension(
      const std::string& extension_id,
      int sign_request_id,
      uint16_t algorithm,
      const scoped_refptr<net::X509Certificate>& certificate,
      base::span<const uint8_t> input) override {
    EXPECT_EQ(expected_request_type_, RequestType::SIGN);
    last_sign_request_id_ = sign_request_id;
    last_extension_id_ = extension_id;
    last_certificate_ = certificate;
    expected_request_type_ = RequestType::NONE;
    return true;
  }

  // Prepares this delegate for the dispatch of a request of type
  // |expected_request_type|. The first request of the right type will cause
  // |expected_request_type_| to be reset to NONE. The request's arguments will
  // be stored in |last_*_request_id_| and |last_extension_id_|. Any additional
  // request and any request of the wrong type will fail the test.
  void ClearAndExpectRequest(RequestType expected_request_type) {
    last_extension_id_.clear();
    last_sign_request_id_ = -1;
    last_cert_request_id_ = -1;
    last_certificate_ = nullptr;
    expected_request_type_ = expected_request_type;
  }

  int last_sign_request_id_ = -1;
  int last_cert_request_id_ = -1;
  scoped_refptr<net::X509Certificate> last_certificate_;
  std::string last_extension_id_;
  std::set<std::string> provider_extensions_;
  RequestType expected_request_type_ = RequestType::NONE;
};

class MockObserver : public CertificateProviderService::Observer {
 public:
  MOCK_METHOD2(
      OnCertificatesUpdated,
      void(const std::string& extension_id,
           const certificate_provider::CertificateInfoList& certificate_infos));
  MOCK_METHOD2(OnSignCompleted,
               void(const scoped_refptr<net::X509Certificate>& certificate,
                    const std::string& extension_id));
};

}  // namespace

class CertificateProviderServiceTest : public testing::Test {
 public:
  CertificateProviderServiceTest()
      : task_runner_(new base::TestMockTimeTaskRunner()),
        task_runner_current_default_handle_(task_runner_),
        service_(new CertificateProviderService()),
        cert_info1_(CreateCertInfo("client_1.pem")),
        cert_info2_(CreateCertInfo("client_2.pem")) {
    std::unique_ptr<TestDelegate> test_delegate(new TestDelegate);
    test_delegate_ = test_delegate.get();
    service_->SetDelegate(std::move(test_delegate));

    service_->AddObserver(&observer_);

    certificate_provider_ = service_->CreateCertificateProvider();
    EXPECT_TRUE(certificate_provider_);

    test_delegate_->provider_extensions_.insert(kExtension1);
  }

  CertificateProviderServiceTest(const CertificateProviderServiceTest&) =
      delete;
  CertificateProviderServiceTest& operator=(
      const CertificateProviderServiceTest&) = delete;

  // Triggers a GetCertificates request and returns the request id. Assumes that
  // at least one extension is registered as a certificate provider.
  int RequestCertificatesFromExtensions(net::ClientCertIdentityList* certs) {
    test_delegate_->ClearAndExpectRequest(
        TestDelegate::RequestType::GET_CERTIFICATES);

    certificate_provider_->GetCertificates(
        base::BindOnce(&StoreCertificates, certs));

    task_runner_->RunUntilIdle();
    EXPECT_EQ(TestDelegate::RequestType::NONE,
              test_delegate_->expected_request_type_);
    return test_delegate_->last_cert_request_id_;
  }

  scoped_refptr<net::SSLPrivateKey> FetchIdentityPrivateKey(
      net::ClientCertIdentity* identity) {
    scoped_refptr<net::SSLPrivateKey> ssl_private_key;
    identity->AcquirePrivateKey(
        base::BindOnce(StorePrivateKey, &ssl_private_key));
    task_runner_->RunUntilIdle();
    return ssl_private_key;
  }

  // Provides |cert_info1_| through kExtension1.
  std::unique_ptr<net::ClientCertIdentity> ProvideDefaultCert() {
    net::ClientCertIdentityList certs;
    const int cert_request_id = RequestCertificatesFromExtensions(&certs);
    SetCertificateProvidedByExtension(kExtension1, cert_request_id,
                                      cert_info1_);
    task_runner_->RunUntilIdle();
    if (certs.empty())
      return nullptr;
    return std::move(certs[0]);
  }

  // Like service_->SetCertificatesProvidedByExtension but taking a single
  // CertificateInfo instead of a list.
  void SetCertificateProvidedByExtension(
      const std::string& extension_id,
      int cert_request_id,
      const certificate_provider::CertificateInfo& cert_info) {
    certificate_provider::CertificateInfoList infos;
    infos.push_back(cert_info);
    EXPECT_CALL(observer_, OnCertificatesUpdated(extension_id, infos));
    service_->SetCertificatesProvidedByExtension(extension_id, infos);
    service_->SetExtensionCertificateReplyReceived(extension_id,
                                                   cert_request_id);
  }

  bool CheckLookUpCertificate(
      const certificate_provider::CertificateInfo& cert_info,
      bool expected_is_certificate_known,
      bool expected_is_currently_provided,
      const std::string& expected_extension_id) {
    bool is_currently_provided = !expected_is_currently_provided;
    std::string extension_id;
    if (expected_is_certificate_known !=
        service_->LookUpCertificate(*cert_info.certificate,
                                    &is_currently_provided, &extension_id)) {
      LOG(ERROR) << "Wrong return value.";
      return false;
    }
    if (expected_is_currently_provided != is_currently_provided) {
      LOG(ERROR) << "Wrong |is_currently_provided|.";
      return false;
    }
    if (expected_extension_id != extension_id) {
      LOG(ERROR) << "Wrong extension id. Got " << extension_id;
      return false;
    }
    return true;
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      task_runner_current_default_handle_;
  raw_ptr<TestDelegate, DanglingUntriaged> test_delegate_ = nullptr;
  testing::StrictMock<MockObserver> observer_;
  std::unique_ptr<CertificateProvider> certificate_provider_;
  std::unique_ptr<CertificateProviderService> service_;
  const certificate_provider::CertificateInfo cert_info1_;
  const certificate_provider::CertificateInfo cert_info2_;
};

TEST_F(CertificateProviderServiceTest, GetCertificates) {
  test_delegate_->provider_extensions_.insert(kExtension2);

  net::ClientCertIdentityList certs;
  const int cert_request_id = RequestCertificatesFromExtensions(&certs);

  task_runner_->RunUntilIdle();
  // No certificates set until all registered extensions replied.
  EXPECT_TRUE(certs.empty());

  SetCertificateProvidedByExtension(kExtension1, cert_request_id, cert_info1_);

  task_runner_->RunUntilIdle();
  // No certificates set until all registered extensions replied.
  EXPECT_TRUE(certs.empty());

  SetCertificateProvidedByExtension(kExtension2, cert_request_id, cert_info2_);

  task_runner_->RunUntilIdle();
  ASSERT_EQ(2u, certs.size());

  // Verify that the ClientCertIdentity returns key handles for the provided
  // certs.
  EXPECT_TRUE(FetchIdentityPrivateKey(certs[0].get()));
  EXPECT_TRUE(FetchIdentityPrivateKey(certs[1].get()));

  // Deregister the extensions as certificate providers. The next
  // GetCertificates call must report an empty list of certs.
  test_delegate_->provider_extensions_.clear();
  service_->OnExtensionUnloaded(kExtension1);
  service_->OnExtensionUnloaded(kExtension2);

  // No request expected.
  test_delegate_->ClearAndExpectRequest(TestDelegate::RequestType::NONE);

  certificate_provider_->GetCertificates(
      base::BindOnce(&StoreCertificates, &certs));

  task_runner_->RunUntilIdle();
  // As |certs| was not empty before, this ensures that StoreCertificates() was
  // called.
  EXPECT_TRUE(certs.empty());
}

TEST_F(CertificateProviderServiceTest, LookUpCertificate) {
  // Provide only |cert_info1_|.
  {
    const int cert_request_id = RequestCertificatesFromExtensions(nullptr);
    SetCertificateProvidedByExtension(kExtension1, cert_request_id,
                                      cert_info1_);
    task_runner_->RunUntilIdle();
  }

  EXPECT_TRUE(CheckLookUpCertificate(cert_info1_, true /* is known */,
                                     true /* is currently provided */,
                                     kExtension1));

  EXPECT_TRUE(CheckLookUpCertificate(cert_info2_, false /* is not known */,
                                     false /* is currently not provided */,
                                     std::string()));

  // Provide only |cert_info2_| from |kExtension2|.
  test_delegate_->provider_extensions_.insert(kExtension2);
  {
    const int cert_request_id = RequestCertificatesFromExtensions(nullptr);
    EXPECT_CALL(observer_,
                OnCertificatesUpdated(
                    kExtension1, certificate_provider::CertificateInfoList()));
    service_->SetCertificatesProvidedByExtension(
        kExtension1, certificate_provider::CertificateInfoList());
    service_->SetExtensionCertificateReplyReceived(kExtension1,
                                                   cert_request_id);
    SetCertificateProvidedByExtension(kExtension2, cert_request_id,
                                      cert_info2_);
    task_runner_->RunUntilIdle();
  }

  EXPECT_TRUE(CheckLookUpCertificate(cert_info1_, true /* is known */,
                                     false /* is currently not provided */,
                                     std::string()));

  EXPECT_TRUE(CheckLookUpCertificate(cert_info2_, true /* is known */,
                                     true /* is currently provided */,
                                     kExtension2));

  // Deregister |kExtension2| as certificate provider and provide |cert_info1_|
  // from |kExtension1|.
  test_delegate_->provider_extensions_.erase(kExtension2);
  service_->OnExtensionUnloaded(kExtension2);

  {
    const int cert_request_id = RequestCertificatesFromExtensions(nullptr);
    SetCertificateProvidedByExtension(kExtension1, cert_request_id,
                                      cert_info1_);
    task_runner_->RunUntilIdle();
  }

  EXPECT_TRUE(CheckLookUpCertificate(cert_info1_, true /* is known */,
                                     true /* is currently provided */,
                                     kExtension1));

  EXPECT_TRUE(CheckLookUpCertificate(cert_info2_, true /* is known */,
                                     false /* is currently not provided */,
                                     std::string()));

  // Provide |cert_info2_| from |kExtension1|.
  {
    const int cert_request_id = RequestCertificatesFromExtensions(nullptr);
    SetCertificateProvidedByExtension(kExtension1, cert_request_id,
                                      cert_info2_);
    task_runner_->RunUntilIdle();
  }

  {
    bool is_currently_provided = true;
    std::string extension_id;
    // |cert_info1_.certificate| was provided before, so this must return true.
    EXPECT_TRUE(service_->LookUpCertificate(
        *cert_info1_.certificate, &is_currently_provided, &extension_id));
    EXPECT_FALSE(is_currently_provided);
    EXPECT_TRUE(extension_id.empty());
  }

  {
    bool is_currently_provided = false;
    std::string extension_id;
    EXPECT_TRUE(service_->LookUpCertificate(
        *cert_info2_.certificate, &is_currently_provided, &extension_id));
    EXPECT_TRUE(is_currently_provided);
    EXPECT_EQ(kExtension1, extension_id);
  }

  EXPECT_TRUE(CheckLookUpCertificate(cert_info1_, true /* is known */,
                                     false /* is currently not provided */,
                                     std::string()));

  EXPECT_TRUE(CheckLookUpCertificate(cert_info2_, true /* is known */,
                                     true /* is currently provided */,
                                     kExtension1));
}

TEST_F(CertificateProviderServiceTest, GetCertificatesTimeout) {
  test_delegate_->provider_extensions_.insert(kExtension2);

  net::ClientCertIdentityList certs;
  const int cert_request_id = RequestCertificatesFromExtensions(&certs);

  certificate_provider::CertificateInfoList infos;
  SetCertificateProvidedByExtension(kExtension1, cert_request_id, cert_info1_);

  task_runner_->RunUntilIdle();
  // No certificates set until all registered extensions replied or a timeout
  // occurred.
  EXPECT_TRUE(certs.empty());

  task_runner_->FastForwardUntilNoTasksRemain();
  // After the timeout, only extension1_'s certificates are returned.
  // This verifies that the timeout delay is > 0 but not how long the delay is.
  ASSERT_EQ(1u, certs.size());

  EXPECT_TRUE(FetchIdentityPrivateKey(certs[0].get()));
}

TEST_F(CertificateProviderServiceTest, UnloadExtensionAfterGetCertificates) {
  test_delegate_->provider_extensions_.insert(kExtension2);

  net::ClientCertIdentityList certs;
  const int cert_request_id = RequestCertificatesFromExtensions(&certs);

  SetCertificateProvidedByExtension(kExtension1, cert_request_id, cert_info1_);
  SetCertificateProvidedByExtension(kExtension2, cert_request_id, cert_info2_);
  task_runner_->RunUntilIdle();

  ASSERT_EQ(2u, certs.size());

  // Sort the returned certs to ensure that the test results are stable.
  std::sort(certs.begin(), certs.end(), ClientCertIdentityAlphabeticSorter);

  // Private key handles for both certificates must be available now.
  EXPECT_TRUE(FetchIdentityPrivateKey(certs[0].get()));
  EXPECT_TRUE(FetchIdentityPrivateKey(certs[1].get()));

  // Unload one of the extensions.
  service_->OnExtensionUnloaded(kExtension2);

  // extension1 isn't affected by the uninstall.
  EXPECT_TRUE(FetchIdentityPrivateKey(certs[0].get()));
  // No key handles that were backed by the uninstalled extension must be
  // returned.
  EXPECT_FALSE(FetchIdentityPrivateKey(certs[1].get()));
}

TEST_F(CertificateProviderServiceTest, DestroyServiceAfterGetCertificates) {
  test_delegate_->provider_extensions_.insert(kExtension2);

  net::ClientCertIdentityList certs;
  const int cert_request_id = RequestCertificatesFromExtensions(&certs);

  SetCertificateProvidedByExtension(kExtension1, cert_request_id, cert_info1_);
  SetCertificateProvidedByExtension(kExtension2, cert_request_id, cert_info2_);
  task_runner_->RunUntilIdle();

  ASSERT_EQ(2u, certs.size());

  // Destroy the service.
  service_.reset();

  // Private key handles for both certificates should return nullptr now.
  EXPECT_FALSE(FetchIdentityPrivateKey(certs[0].get()));
  EXPECT_FALSE(FetchIdentityPrivateKey(certs[1].get()));
}

TEST_F(CertificateProviderServiceTest, UnloadExtensionDuringGetCertificates) {
  test_delegate_->provider_extensions_.insert(kExtension2);

  net::ClientCertIdentityList certs;
  const int cert_request_id = RequestCertificatesFromExtensions(&certs);

  SetCertificateProvidedByExtension(kExtension1, cert_request_id, cert_info1_);

  // The pending certificate request is only waiting for kExtension2. Unloading
  // that extension must cause the request to be finished.
  service_->OnExtensionUnloaded(kExtension2);

  task_runner_->RunUntilIdle();
  EXPECT_EQ(1u, certs.size());
}

// Trying to sign data using the exposed SSLPrivateKey must cause a sign
// request. The reply must be correctly routed back to the private key.
TEST_F(CertificateProviderServiceTest, SignRequest) {
  std::unique_ptr<net::ClientCertIdentity> cert(ProvideDefaultCert());
  ASSERT_TRUE(cert);

  scoped_refptr<net::SSLPrivateKey> private_key(
      FetchIdentityPrivateKey(cert.get()));

  ASSERT_TRUE(private_key);
  EXPECT_TRUE(IsKeyEqualToCertInfo(cert_info1_, private_key.get()));
  EXPECT_NE(std::string::npos,
            private_key->GetProviderName().find(kExtension1));

  test_delegate_->ClearAndExpectRequest(TestDelegate::RequestType::SIGN);

  std::string input = "any input data";
  std::vector<uint8_t> received_signature;
  private_key->Sign(
      SSL_SIGN_RSA_PKCS1_SHA256,
      std::vector<uint8_t>(input.begin(), input.end()),
      base::BindOnce(&ExpectOKAndStoreSignature, &received_signature));

  task_runner_->RunUntilIdle();

  const int sign_request_id = test_delegate_->last_sign_request_id_;
  EXPECT_EQ(TestDelegate::RequestType::NONE,
            test_delegate_->expected_request_type_);
  EXPECT_TRUE(cert_info1_.certificate->EqualsExcludingChain(
      test_delegate_->last_certificate_.get()));

  // No signature received until the extension replied to the service.
  EXPECT_TRUE(received_signature.empty());

  EXPECT_CALL(observer_, OnSignCompleted(cert_info1_.certificate, kExtension1));

  std::vector<uint8_t> signature_reply;
  signature_reply.push_back(5);
  signature_reply.push_back(7);
  signature_reply.push_back(8);
  service_->ReplyToSignRequest(kExtension1, sign_request_id, signature_reply);

  task_runner_->RunUntilIdle();
  EXPECT_EQ(signature_reply, received_signature);
}

TEST_F(CertificateProviderServiceTest, UnloadExtensionDuringSign) {
  std::unique_ptr<net::ClientCertIdentity> cert(ProvideDefaultCert());
  ASSERT_TRUE(cert);

  scoped_refptr<net::SSLPrivateKey> private_key(
      FetchIdentityPrivateKey(cert.get()));
  ASSERT_TRUE(private_key);

  test_delegate_->ClearAndExpectRequest(TestDelegate::RequestType::SIGN);

  std::string input = "any input data";
  net::Error error = net::OK;
  private_key->Sign(SSL_SIGN_RSA_PKCS1_SHA256,
                    std::vector<uint8_t>(input.begin(), input.end()),
                    base::BindOnce(&ExpectEmptySignatureAndStoreError, &error));

  task_runner_->RunUntilIdle();

  // No signature received until the extension replied to the service or is
  // unloaded.
  EXPECT_EQ(net::OK, error);

  // Unload the extension.
  service_->OnExtensionUnloaded(kExtension1);

  task_runner_->RunUntilIdle();
  EXPECT_EQ(net::ERR_FAILED, error);
}

// Try to sign data using key; using the Subject Public Key Info (SPKI) to
// identify the key.
TEST_F(CertificateProviderServiceTest, SignUsingSpkiAsIdentification) {
  std::string_view client1_spki_piece;
  ASSERT_TRUE(net::asn1::ExtractSPKIFromDERCert(
      net::x509_util::CryptoBufferAsStringPiece(
          cert_info1_.certificate->cert_buffer()),
      &client1_spki_piece));
  std::string client1_spki(client1_spki_piece);

  std::unique_ptr<net::ClientCertIdentity> cert(ProvideDefaultCert());
  ASSERT_TRUE(cert);

  std::vector<uint16_t> supported_algorithms;
  std::string extension_id;
  // If this fails, try to regenerate kClient1SpkiBase64 using the command shown
  // above.
  EXPECT_TRUE(
      service_->LookUpSpki(client1_spki, &supported_algorithms, &extension_id));
  EXPECT_EQ(extension_id, kExtension1);
  EXPECT_THAT(supported_algorithms,
              testing::UnorderedElementsAre(SSL_SIGN_RSA_PKCS1_SHA256));

  test_delegate_->ClearAndExpectRequest(TestDelegate::RequestType::SIGN);
  std::vector<uint8_t> input{'d', 'a', 't', 'a'};
  std::vector<uint8_t> received_signature;
  service_->RequestSignatureBySpki(
      client1_spki, SSL_SIGN_RSA_PKCS1_SHA256, input,
      /*authenticating_user_account_id=*/{},
      base::BindOnce(&ExpectOKAndStoreSignature, &received_signature));

  task_runner_->RunUntilIdle();

  const int sign_request_id = test_delegate_->last_sign_request_id_;
  EXPECT_EQ(TestDelegate::RequestType::NONE,
            test_delegate_->expected_request_type_);
  EXPECT_TRUE(cert_info1_.certificate->EqualsExcludingChain(
      test_delegate_->last_certificate_.get()));

  // No signature received until the extension replied to the service.
  EXPECT_TRUE(received_signature.empty());

  EXPECT_CALL(observer_, OnSignCompleted(cert_info1_.certificate, kExtension1));

  std::vector<uint8_t> signature_reply;
  signature_reply.push_back(5);
  signature_reply.push_back(7);
  signature_reply.push_back(8);
  service_->ReplyToSignRequest(kExtension1, sign_request_id, signature_reply);

  task_runner_->RunUntilIdle();
  EXPECT_EQ(signature_reply, received_signature);
}

}  // namespace chromeos
