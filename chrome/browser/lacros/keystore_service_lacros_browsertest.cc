// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "content/public/test/browser_test.h"

// During GenerateKey* call this error means that the key was created, but
// Ash-Chrome failed to tag it properly. It happens because Ash-Chrome is trying
// to work with the real NSS, but in browser tests (i.e. on Linux) it doesn't
// work the same way as on ChromeOS.
const char kFailedToSetAttribute[] = "Setting key attribute value failed.";

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
    return chromeos::LacrosChromeServiceImpl::Get()->keystore_service_remote();
  }
};

// Tests that providing an incorrectly formatted user keystore challenge returns
// failure.
IN_PROC_BROWSER_TEST_F(KeystoreServiceLacrosBrowserTest, WrongFormattingUser) {
  crosapi::mojom::KeystoreStringResultPtr result;
  std::string challenge = "asdf";
  crosapi::mojom::KeystoreServiceAsyncWaiter async_waiter(
      keystore_service_remote().get());
  async_waiter.ChallengeAttestationOnlyKeystore(
      challenge, crosapi::mojom::KeystoreType::kUser, /*migrate=*/false,
      &result);
  ASSERT_TRUE(result->is_error_message());

  // TODO(https://crbug.com/1134349): Currently this errors out because remote
  // attestation is disabled. We want this to error out because of a poorly
  // formatted attestation message.
}

// Tests that get certificates works.
IN_PROC_BROWSER_TEST_F(KeystoreServiceLacrosBrowserTest, GetCertificatesEmpty) {
  crosapi::mojom::GetCertificatesResultPtr result;
  crosapi::mojom::KeystoreServiceAsyncWaiter async_waiter(
      keystore_service_remote().get());
  async_waiter.GetCertificates(crosapi::mojom::KeystoreType::kUser, &result);
  ASSERT_TRUE(result->is_certificates());
  EXPECT_EQ(0u, result->get_certificates().size());
}

// Tests that generate key works.
IN_PROC_BROWSER_TEST_F(KeystoreServiceLacrosBrowserTest, GenerateKeyPKCS) {
  crosapi::mojom::KeystoreBinaryResultPtr result;
  crosapi::mojom::KeystoreServiceAsyncWaiter async_waiter(
      keystore_service_remote().get());
  crosapi::mojom::KeystoreSigningAlgorithmPtr algo =
      crosapi::mojom::KeystoreSigningAlgorithm::New();
  crosapi::mojom::KeystorePKCS115ParamsPtr params =
      crosapi::mojom::KeystorePKCS115Params::New();
  params->modulus_length = 1024;
  algo->set_pkcs115(std::move(params));
  async_waiter.GenerateKey(crosapi::mojom::KeystoreType::kUser, std::move(algo),
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
                       DISABLED_GenerateKeyECDSA) {
  crosapi::mojom::KeystoreBinaryResultPtr result;
  crosapi::mojom::KeystoreServiceAsyncWaiter async_waiter(
      keystore_service_remote().get());
  crosapi::mojom::KeystoreSigningAlgorithmPtr algo =
      crosapi::mojom::KeystoreSigningAlgorithm::New();
  crosapi::mojom::KeystoreECDSAParamsPtr params =
      crosapi::mojom::KeystoreECDSAParams::New();
  params->named_curve = "P-256";
  algo->set_ecdsa(std::move(params));
  async_waiter.GenerateKey(crosapi::mojom::KeystoreType::kUser, std::move(algo),
                           /*extension_id=*/"123", &result);
  // Errors out because Ash-Chrome is not running on ChromeOS.
  ASSERT_TRUE(result->is_error_message());
  EXPECT_EQ(result->get_error_message(), kFailedToSetAttribute);
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
  async_waiter.AddCertificate(crosapi::mojom::KeystoreType::kUser,
                              std::move(dummy_certificate), &result);
  EXPECT_EQ(result, expected_error);

  result = "";
  async_waiter.RemoveCertificate(crosapi::mojom::KeystoreType::kUser,
                                 std::move(dummy_certificate), &result);
  EXPECT_EQ(result, expected_error);
}
