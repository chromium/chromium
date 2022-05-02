// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

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
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_test.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_builder.h"

// NOTE: Some tests in this file modify the certificate store. That is
// potentially a lasting side effect that can affect other tests.
// * To prevent interference with tests that are run in parallel, these tests
// are a part of lacros_chrome_browsertests_run_in_series test suite.
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
    if (std::equal(cert_der.begin(), cert_der.end(),
                   expected_cert_der.begin())) {
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
  auto cert_builder =
      net::CertBuilder::FromSubjectPublicKeyInfo(public_key_spki, issuer.get());
  cert_builder->SetSignatureAlgorithmRsaPkca1(net::DigestAlgorithm::Sha256);
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

// During ExtensionGenerateKey* call this error means that the key was created,
// but Ash-Chrome failed to tag it properly. It happens because Ash-Chrome is
// trying to work with the real NSS, but in browser tests (i.e. on Linux) it
// doesn't work the same way as on ChromeOS.
const char kFailedToSetAttribute[] = "Setting key attribute value failed.";
// During ExtensionSign call this error means that Ash-Chrome wasn't able to
// find the key in a list of allowed keys.
const char kErrorKeyNotAllowedForSigning[] =
    "This key is not allowed for signing. Either it was used for "
    "signing before or it was not correctly generated.";

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

// Tests that providing an incorrectly formatted user keystore challenge returns
// failure.
IN_PROC_BROWSER_TEST_F(KeystoreServiceLacrosBrowserTest, WrongFormattingUser) {
  crosapi::mojom::DEPRECATED_KeystoreStringResultPtr result;
  std::string challenge = "asdf";
  crosapi::mojom::KeystoreServiceAsyncWaiter async_waiter(
      keystore_service_remote().get());
  async_waiter.DEPRECATED_ChallengeAttestationOnlyKeystore(
      challenge, crosapi::mojom::KeystoreType::kUser, /*migrate=*/false,
      &result);
  ASSERT_TRUE(result->is_error_message());

  // TODO(https://crbug.com/1134349): Currently this errors out because remote
  // attestation is disabled. We want this to error out because of a poorly
  // formatted attestation message.
}

// Tests that get certificates works.
IN_PROC_BROWSER_TEST_F(KeystoreServiceLacrosBrowserTest, GetCertificatesEmpty) {
  crosapi::mojom::DEPRECATED_GetCertificatesResultPtr result;
  crosapi::mojom::KeystoreServiceAsyncWaiter async_waiter(
      keystore_service_remote().get());
  async_waiter.DEPRECATED_GetCertificates(crosapi::mojom::KeystoreType::kUser,
                                          &result);
  ASSERT_TRUE(result->is_certificates());
  EXPECT_EQ(0u, result->get_certificates().size());
}

// Tests that extension generate key works.
IN_PROC_BROWSER_TEST_F(KeystoreServiceLacrosBrowserTest,
                       ExtensionGenerateKeyPKCS) {
  crosapi::mojom::DEPRECATED_ExtensionKeystoreBinaryResultPtr result;
  crosapi::mojom::KeystoreServiceAsyncWaiter async_waiter(
      keystore_service_remote().get());
  crosapi::mojom::KeystorePKCS115ParamsPtr params =
      crosapi::mojom::KeystorePKCS115Params::New();
  params->modulus_length = 1024;
  crosapi::mojom::KeystoreSigningAlgorithmPtr algo =
      crosapi::mojom::KeystoreSigningAlgorithm::NewPkcs115(std::move(params));
  async_waiter.DEPRECATED_ExtensionGenerateKey(
      crosapi::mojom::KeystoreType::kUser, std::move(algo),
      /*extension_id=*/"123", &result);
  // Errors out because Ash-Chrome is not running on ChromeOS.
  ASSERT_TRUE(result->is_error_message());
  EXPECT_EQ(result->get_error_message(), kFailedToSetAttribute);
}

// TODO(https://crbug.com/1134349): After the switch from PlatformKeysService to
// ExtensionPlatformKeysService the test started to crash on cloud builders. The
// current theory is that it is because of the added `AddKeyAttribute` call to
// NSS. In the long term it is not clear if the test should actually try to
// generate/modify keys in non-test NSS database on builders. But there's no
// simple way to prevent this at the moment.
IN_PROC_BROWSER_TEST_F(KeystoreServiceLacrosBrowserTest,
                       DISABLED_ExtensionGenerateKeyECDSA) {
  crosapi::mojom::DEPRECATED_ExtensionKeystoreBinaryResultPtr result;
  crosapi::mojom::KeystoreServiceAsyncWaiter async_waiter(
      keystore_service_remote().get());
  crosapi::mojom::KeystoreECDSAParamsPtr params =
      crosapi::mojom::KeystoreECDSAParams::New();
  params->named_curve = "P-256";
  crosapi::mojom::KeystoreSigningAlgorithmPtr algo =
      crosapi::mojom::KeystoreSigningAlgorithm::NewEcdsa(std::move(params));
  async_waiter.DEPRECATED_ExtensionGenerateKey(
      crosapi::mojom::KeystoreType::kUser, std::move(algo),
      /*extension_id=*/"123", &result);
  // Errors out because Ash-Chrome is not running on ChromeOS.
  ASSERT_TRUE(result->is_error_message());
  EXPECT_EQ(result->get_error_message(), kFailedToSetAttribute);
}

// Tests that extension sign works.
IN_PROC_BROWSER_TEST_F(KeystoreServiceLacrosBrowserTest, ExtensionSign) {
  crosapi::mojom::DEPRECATED_ExtensionKeystoreBinaryResultPtr result;
  crosapi::mojom::KeystoreServiceAsyncWaiter async_waiter(
      keystore_service_remote().get());
  async_waiter.DEPRECATED_ExtensionSign(
      crosapi::mojom::KeystoreType::kUser,
      /*public_key=*/{1, 2, 3, 4, 5},
      /*scheme=*/crosapi::mojom::KeystoreSigningScheme::kRsassaPkcs1V15Sha256,
      /*data=*/{10, 11, 12, 13, 14, 15},
      /*extension_id=*/"123", &result);
  // Errors out because the public key is not valid. Currently there's no way to
  // create a valid key in Ash-Chrome during browser tests.
  ASSERT_TRUE(result->is_error_message());
  EXPECT_EQ(result->get_error_message(), kErrorKeyNotAllowedForSigning);
}

// Tests that trying to add/remove an incorrectly formatted certificate results
// in failure.
IN_PROC_BROWSER_TEST_F(KeystoreServiceLacrosBrowserTest, CertificateBadFormat) {
  const char expected_error[] = "Certificate is not a valid X.509 certificate.";
  std::string result;
  std::vector<uint8_t> dummy_certificate;
  dummy_certificate.push_back(15);
  crosapi::mojom::KeystoreServiceAsyncWaiter async_waiter(
      keystore_service_remote().get());
  async_waiter.DEPRECATED_AddCertificate(crosapi::mojom::KeystoreType::kUser,
                                         std::move(dummy_certificate), &result);
  EXPECT_EQ(result, expected_error);

  result = "";
  async_waiter.DEPRECATED_RemoveCertificate(crosapi::mojom::KeystoreType::kUser,
                                            std::move(dummy_certificate),
                                            &result);
  EXPECT_EQ(result, expected_error);
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
