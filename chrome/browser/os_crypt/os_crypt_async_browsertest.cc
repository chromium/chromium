// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/os_crypt_async.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/test/browser_test.h"
#include "services/test/echo/public/mojom/echo.mojom.h"

namespace os_crypt_async {

namespace {

Encryptor GetInstanceSync(OSCryptAsync& factory,
                          Encryptor::Option option = Encryptor::Option::kNone) {
  base::RunLoop run_loop;
  std::optional<Encryptor> encryptor;
  auto sub = factory.GetInstance(
      base::BindLambdaForTesting([&](Encryptor instance, bool result) {
        EXPECT_TRUE(result);
        encryptor.emplace(std::move(instance));
        run_loop.Quit();
      }),
      option);
  run_loop.Run();
  return std::move(*encryptor);
}

}  // namespace

class OSCryptAsyncBrowserTest : public InProcessBrowserTest {
 protected:
  base::HistogramTester histogram_tester_;
};

// Test the basic browser interface to Encrypt and Decrypt data.
IN_PROC_BROWSER_TEST_F(OSCryptAsyncBrowserTest, EncryptDecrypt) {
  auto encryptor = GetInstanceSync(*g_browser_process->os_crypt_async());
  // These histograms should always have been recorded by the time the
  // GetInstance callback above has happened, since the browser registers its
  // metrics callback before anything else gets a chance to.
  histogram_tester_.ExpectTotalCount("OSCrypt.AsyncInitialization.Time", 1u);
  histogram_tester_.ExpectUniqueSample("OSCrypt.AsyncInitialization.Result",
                                       true, 1u);

  auto ciphertext = encryptor.EncryptString("plaintext");
  ASSERT_TRUE(ciphertext);

  auto decrypted = encryptor.DecryptData(*ciphertext);
  ASSERT_TRUE(decrypted);

  EXPECT_EQ(*decrypted, "plaintext");
}

// This test verifies that an Encryptor works inside a fully sandboxed process.
IN_PROC_BROWSER_TEST_F(OSCryptAsyncBrowserTest, SandboxedEncryptionTest) {
  // Use a testing instance, otherwise fallback to os_crypt sync does not work
  // on all platforms when inside a sandbox without transferring the key over
  // manually.
  auto os_crypt_async = GetTestOSCryptAsyncForTesting();
  Encryptor encryptor = GetInstanceSync(*os_crypt_async);

  constexpr char kTestData[] = "testdatatest";
  // First, encrypt the data.
  const auto encrypted_data = encryptor.EncryptString(kTestData);
  ASSERT_TRUE(encrypted_data.has_value());

  // Launch sandboxed echo service.
  auto echo_service =
      content::ServiceProcessHost::Launch<echo::mojom::EchoService>();

  base::test::TestFuture<std::optional<std::vector<uint8_t>>> future;

  // This performs a decrypt, then an encrypt.
  echo_service->DecryptEncrypt(
      std::move(encryptor), *encrypted_data,
      future.GetCallback<const std::optional<std::vector<uint8_t>>&>());
  auto& result = future.Get();
  ASSERT_TRUE(result.has_value());

  // Obtain a second encryptor, since the first one was consumed in the mojo
  // call above.
  Encryptor encryptor2 = GetInstanceSync(*os_crypt_async);
  // Finally, decrypt the data again.
  const auto plaintext = encryptor2.DecryptData(*result);
  ASSERT_TRUE(plaintext.has_value());
  EXPECT_EQ(*plaintext, kTestData);
}

// This test verifies that an Encryptor obtained with the kEncryptSyncCompat
// option encrypts data that can be decrypted by OSCrypt, as well as ensuring
// that kEncryptSyncCompat and kNone options are interoperable with each other.
IN_PROC_BROWSER_TEST_F(OSCryptAsyncBrowserTest, OSCryptBackwardsCompatTest) {
  auto encryptor = GetInstanceSync(*g_browser_process->os_crypt_async(),
                                   Encryptor::Option::kEncryptSyncCompat);
  auto ciphertext = encryptor.EncryptString("plaintext");
  ASSERT_TRUE(ciphertext);

  {
    const auto decrypted = encryptor.DecryptData(*ciphertext);
    ASSERT_TRUE(decrypted);
    EXPECT_EQ(*decrypted, "plaintext");
  }

  {
    std::string decrypted;
    ASSERT_TRUE(OSCrypt::DecryptString(
        std::string(ciphertext->begin(), ciphertext->end()), &decrypted));
    EXPECT_EQ(decrypted, "plaintext");
  }

  {
    // Verify that data encrypted from a kEncryptSyncCompat encryptor can be
    // decrypted with a kNone encryptor.
    auto full_encryptor = GetInstanceSync(*g_browser_process->os_crypt_async(),
                                          Encryptor::Option::kNone);
    const auto decrypted = full_encryptor.DecryptData(*ciphertext);
    ASSERT_TRUE(decrypted);
    EXPECT_EQ(decrypted, "plaintext");

    // Verify that data encrypted from a kNone encryptor can be decrypted with a
    // kEncryptSyncCompat encryptor.
    const auto ciphertext2 = full_encryptor.EncryptString("more_plaintext");
    ASSERT_TRUE(ciphertext2);
    const auto decrypted2 = encryptor.DecryptData(*ciphertext2);
    ASSERT_TRUE(decrypted2);
    EXPECT_EQ(*decrypted2, "more_plaintext");
  }
}

}  // namespace os_crypt_async
