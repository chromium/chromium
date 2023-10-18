// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/ranges/algorithm.h"
#include "base/test/test_future.h"
#include "chrome/browser/lacros/cert/cert_db_initializer_factory.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/cpp/keystore_service_util.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom-shared.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_builder.h"

// NOTE: Some tests in this file modify the certificate store. That is
// potentially a lasting side effect that can affect other tests.
// * To prevent interference with tests that are run in parallel, these tests
// are a part of lacros_chrome_browsertests test suite.
// * To prevent interference with following tests, they try to clean up all the
// side effects themself, e.g. if a test adds a cert, it is also responsible for
// deleting it.
// Subsequent runs of lacros browser tests share the same ash-chrome instance
// and thus also the same user certificate database. The certificate database is
// not cleaned automatically between tests because of performance concerns.

namespace {

namespace is_cert_in_nss {
namespace internal {

void IsCertInNSSDatabaseOnIOThreadWithCertList(
    const std::vector<uint8_t>& expected_cert_der,
    bool* out_cert_found,
    base::OnceClosure done_closure,
    net::ScopedCERTCertificateList certs) {
  for (const net::ScopedCERTCertificate& cert : certs) {
    auto cert_der = base::make_span(cert->derCert.data, cert->derCert.len);
    if (base::ranges::equal(cert_der, expected_cert_der)) {
      *out_cert_found = true;
      break;
    }
  }

  std::move(done_closure).Run();
}

void IsCertInNSSDatabaseOnIOThreadWithCertDb(
    const std::vector<uint8_t>& expected_cert_der,
    bool* out_cert_found,
    base::OnceClosure done_closure,
    net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  cert_db->ListCerts(base::BindOnce(&IsCertInNSSDatabaseOnIOThreadWithCertList,
                                    expected_cert_der, out_cert_found,
                                    std::move(done_closure)));
}

void IsCertInNSSDatabaseOnIOThread(
    NssCertDatabaseGetter database_getter,
    const std::vector<uint8_t>& expected_cert_der,
    bool* out_cert_found,
    base::OnceClosure done_closure) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto did_get_cert_db_split_callback = base::SplitOnceCallback(base::BindOnce(
      &IsCertInNSSDatabaseOnIOThreadWithCertDb, expected_cert_der,
      out_cert_found, std::move(done_closure)));

  net::NSSCertDatabase* cert_db =
      std::move(database_getter)
          .Run(std::move(did_get_cert_db_split_callback.first));
  if (cert_db) {
    std::move(did_get_cert_db_split_callback.second).Run(cert_db);
  }
}
}  // namespace internal

// Returns true if a certificate with subject CommonName `common_name` is
// present in the `NSSCertDatabase` for `profile`.
bool IsCertInNSSDatabase(Profile* profile,
                         const std::vector<uint8_t>& expected_cert_der) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RunLoop run_loop;
  bool cert_found = false;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(internal::IsCertInNSSDatabaseOnIOThread,
                     NssServiceFactory::GetForContext(profile)
                         ->CreateNSSCertDatabaseGetterForIOThread(),
                     expected_cert_der, &cert_found, run_loop.QuitClosure()));
  run_loop.Run();
  return cert_found;
}

}  // namespace is_cert_in_nss

// Makes a CertBuilder that would return a valid x509 client certificate for the
// `public_key_spki`.
scoped_refptr<net::X509Certificate> MakeCert(
    const std::vector<uint8_t>& public_key_spki) {
  auto issuer = std::make_unique<net::CertBuilder>(/*orig_cert=*/nullptr,
                                                   /*issuer=*/nullptr);
  issuer->GenerateRSAKey();
  auto cert_builder =
      net::CertBuilder::FromSubjectPublicKeyInfo(public_key_spki, issuer.get());
  cert_builder->SetSignatureAlgorithm(net::SignatureAlgorithm::kRsaPkcs1Sha256);
  cert_builder->SetValidity(base::Time::Now(),
                            base::Time::Now() + base::Days(30));
  return cert_builder->GetX509Certificate();
}

// Returns x509 client certificate from the `cert_builder` as a DER-encoded
// certificate.
std::vector<uint8_t> CertToDer(scoped_refptr<net::X509Certificate> cert) {
  auto cert_span = net::x509_util::CryptoBufferAsSpan(cert->cert_buffer());
  return std::vector<uint8_t>(cert_span.begin(), cert_span.end());
}

// This class provides integration testing for the keystore service crosapi.
// TODO(https://crbug.com/1134340): The logic being tested does not rely on
// //chrome or //content so it would be helpful if this lived in a lower-level
// test suite.
class KeystoreServiceLacrosBrowserTest : public InProcessBrowserTest {
 protected:
  KeystoreServiceLacrosBrowserTest() = default;

  KeystoreServiceLacrosBrowserTest(const KeystoreServiceLacrosBrowserTest&) =
      delete;
  KeystoreServiceLacrosBrowserTest& operator=(
      const KeystoreServiceLacrosBrowserTest&) = delete;

  ~KeystoreServiceLacrosBrowserTest() override = default;

  mojo::Remote<crosapi::mojom::KeystoreService>& keystore_service_remote() {
    return chromeos::LacrosService::Get()
        ->GetRemote<crosapi::mojom::KeystoreService>();
  }

  void SetUp() override {
    CertDbInitializerFactory::GetInstance()
        ->SetCreateWithBrowserContextForTesting(
            /*should_create=*/true);
    InProcessBrowserTest::SetUp();
  }
};

// Tests that providing an incorrectly formatted challenge for user's keystore
// returns error message.
IN_PROC_BROWSER_TEST_F(KeystoreServiceLacrosBrowserTest, WrongFormattingUser) {
  crosapi::mojom::ChallengeAttestationOnlyKeystoreResultPtr result;
  std::vector<uint8_t> incorrect_challenge = {10, 11, 12, 13, 14, 15};
  crosapi::mojom::KeystoreServiceAsyncWaiter async_waiter(
      keystore_service_remote().get());
  async_waiter.ChallengeAttestationOnlyKeystore(
      crosapi::mojom::KeystoreType::kUser, incorrect_challenge,
      /*migrate=*/false,
      crosapi::mojom::KeystoreSigningAlgorithmName::kRsassaPkcs115, &result);

  ASSERT_TRUE(result->is_error_message());

  // TODO(https://crbug.com/1134349): Currently this errors out because the
  // device is not enterprise enrolled. We want this to error out because of a
  // poorly formatted attestation message.
  const char expected_error_message[] =
      "Failed to get Enterprise certificate. Error code = 2";
  EXPECT_EQ(expected_error_message, result->get_error_message());
}

// Tests that get certificates will return empty list
IN_PROC_BROWSER_TEST_F(KeystoreServiceLacrosBrowserTest, GetCertificatesEmpty) {
  crosapi::mojom::GetCertificatesResultPtr result;
  crosapi::mojom::KeystoreServiceAsyncWaiter async_waiter(
      keystore_service_remote().get());
  async_waiter.GetCertificates(crosapi::mojom::KeystoreType::kUser, &result);
  ASSERT_TRUE(result->is_certificates());
  EXPECT_EQ(0u, result->get_certificates().size());
}

// Tests that generate RSA key works
IN_PROC_BROWSER_TEST_F(KeystoreServiceLacrosBrowserTest,
                       GenerateKeyPKCSSuccess) {
  crosapi::mojom::KeystoreBinaryResultPtr result;
  crosapi::mojom::KeystoreServiceAsyncWaiter async_waiter(
      keystore_service_remote().get());
  crosapi::mojom::KeystorePKCS115ParamsPtr params =
      crosapi::mojom::KeystorePKCS115Params::New();
  params->modulus_length = 1024;
  crosapi::mojom::KeystoreSigningAlgorithmPtr algo =
      crosapi::mojom::KeystoreSigningAlgorithm::NewPkcs115(std::move(params));

  async_waiter.GenerateKey(crosapi::mojom::KeystoreType::kUser, std::move(algo),
                           &result);

  ASSERT_TRUE(result->is_blob());
  // Testing that key has some length (162 comes from the test run).
  EXPECT_EQ(result->get_blob().size(), 162U);
}

IN_PROC_BROWSER_TEST_F(KeystoreServiceLacrosBrowserTest,
                       GenerateKeyECDSASuccess) {
  crosapi::mojom::KeystoreBinaryResultPtr result;
  crosapi::mojom::KeystoreServiceAsyncWaiter async_waiter(
      keystore_service_remote().get());
  crosapi::mojom::KeystoreECDSAParamsPtr params =
      crosapi::mojom::KeystoreECDSAParams::New();
  params->named_curve = "P-256";
  crosapi::mojom::KeystoreSigningAlgorithmPtr algo =
      crosapi::mojom::KeystoreSigningAlgorithm::NewEcdsa(std::move(params));

  async_waiter.GenerateKey(crosapi::mojom::KeystoreType::kUser, std::move(algo),
                           &result);

  ASSERT_TRUE(result->is_blob());
  // Testing that key has some length (91 comes from the test run).
  EXPECT_EQ(result->get_blob().size(), 91U);
}

// Tests that sign returns error because no private key.
IN_PROC_BROWSER_TEST_F(KeystoreServiceLacrosBrowserTest, SignReturnError) {
  crosapi::mojom::KeystoreBinaryResultPtr result;
  crosapi::mojom::KeystoreServiceAsyncWaiter async_waiter(
      keystore_service_remote().get());
  bool is_keystore_provided = true;

  async_waiter.Sign(
      is_keystore_provided, crosapi::mojom::KeystoreType::kUser,
      /*public_key=*/{1, 2, 3, 4, 5},
      /*scheme=*/crosapi::mojom::KeystoreSigningScheme::kRsassaPkcs1V15Sha256,
      /*data=*/{10, 11, 12, 13, 14, 15}, &result);

  // Errors out because the public key is not valid. Currently there's no way to
  // create a valid key in Ash-Chrome during browser tests.
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), crosapi::mojom::KeystoreError::kKeyNotFound);
}

// Tests that trying to add/remove an incorrectly formatted certificate should
// fail.
IN_PROC_BROWSER_TEST_F(KeystoreServiceLacrosBrowserTest, CertificateBadFormat) {
  std::vector<uint8_t> dummy_certificate;
  dummy_certificate.push_back(15);
  crosapi::mojom::KeystoreServiceAsyncWaiter async_waiter(
      keystore_service_remote().get());
  bool is_result_error = false;
  crosapi::mojom::KeystoreError result_error_code;
  async_waiter.AddCertificate(crosapi::mojom::KeystoreType::kUser,
                              std::move(dummy_certificate), &is_result_error,
                              &result_error_code);
  ASSERT_TRUE(is_result_error) << "Error: " << result_error_code;
  EXPECT_EQ(result_error_code,
            crosapi::mojom::KeystoreError::kCertificateInvalid);

  bool is_remove_result_error = false;
  crosapi::mojom::KeystoreError remove_result_error_code;
  async_waiter.RemoveCertificate(
      crosapi::mojom::KeystoreType::kUser, std::move(dummy_certificate),
      &is_remove_result_error, &remove_result_error_code);

  ASSERT_TRUE(is_remove_result_error) << "Error: " << remove_result_error_code;
  EXPECT_EQ(remove_result_error_code,
            crosapi::mojom::KeystoreError::kCertificateInvalid);
}

// Tests that importing a correct certificate works and that it becomes visible
// in both Ash and Lacros. Tests that removing a certificate works.
IN_PROC_BROWSER_TEST_F(KeystoreServiceLacrosBrowserTest, AddRemoveCertificate) {
  crosapi::mojom::KeystoreServiceAsyncWaiter async_waiter(
      keystore_service_remote().get());

  // Creating a valid cert using this API requires generating a key for it
  // first.
  crosapi::mojom::KeystoreBinaryResultPtr generate_key_result;
  async_waiter.GenerateKey(
      crosapi::mojom::KeystoreType::kUser,
      crosapi::keystore_service_util::MakeRsaKeystoreSigningAlgorithm(
          /*modulus_length=*/2048, /*sw_backed=*/false),
      &generate_key_result);
  ASSERT_FALSE(generate_key_result->is_error());

  // Generate a client certificate for the generated key.
  scoped_refptr<net::X509Certificate> cert =
      MakeCert(generate_key_result->get_blob());
  std::vector<uint8_t> cert_der = CertToDer(cert);

  // Make Ash import the certificate.
  bool result_is_error = false;
  crosapi::mojom::KeystoreError result_error;
  async_waiter.AddCertificate(crosapi::mojom::KeystoreType::kUser, cert_der,
                              &result_is_error, &result_error);
  ASSERT_FALSE(result_is_error) << "Error: " << result_error;

  // Test that Lacros can see the certificate that was imported by Ash.
  EXPECT_TRUE(
      is_cert_in_nss::IsCertInNSSDatabase(browser()->profile(), cert_der));

  // Test that Ash also returns the imported certificate.
  crosapi::mojom::GetCertificatesResultPtr get_certs_result;
  async_waiter.GetCertificates(crosapi::mojom::KeystoreType::kUser,
                               &get_certs_result);
  ASSERT_FALSE(get_certs_result->is_error());
  EXPECT_TRUE(base::Contains(get_certs_result->get_certificates(), cert_der));

  // Make Ash remove the certificate.
  async_waiter.RemoveCertificate(crosapi::mojom::KeystoreType::kUser, cert_der,
                                 &result_is_error, &result_error);
  ASSERT_FALSE(result_is_error) << "Error: " << result_error;

  // Test that Lacros cannot see the certificate anymore.
  EXPECT_FALSE(
      is_cert_in_nss::IsCertInNSSDatabase(browser()->profile(), cert_der));

  // Test that Ash doesn't return the imported certificate anymore.
  async_waiter.GetCertificates(crosapi::mojom::KeystoreType::kUser,
                               &get_certs_result);
  ASSERT_FALSE(get_certs_result->is_error());
  EXPECT_FALSE(base::Contains(get_certs_result->get_certificates(), cert_der));
}

}  // namespace
