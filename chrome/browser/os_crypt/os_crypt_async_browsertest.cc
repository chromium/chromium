// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "content/public/test/browser_test.h"

using OSCryptAsyncBrowserTest = InProcessBrowserTest;

// Test the basic interface to Encrypt and Decrypt data.
IN_PROC_BROWSER_TEST_F(OSCryptAsyncBrowserTest, EncryptDecrypt) {
  std::unique_ptr<os_crypt_async::Encryptor> encryptor;
  auto sub = g_browser_process->os_crypt_async()->GetInstance(
      base::BindLambdaForTesting(
          [&encryptor](os_crypt_async::Encryptor instance, bool result) {
            EXPECT_TRUE(result);
            encryptor = std::make_unique<os_crypt_async::Encryptor>(
                std::move(instance));
          }));
  ASSERT_TRUE(encryptor);

  auto ciphertext = encryptor->EncryptString("plaintext");
  ASSERT_TRUE(ciphertext);

  auto decrypted = encryptor->DecryptData(*ciphertext);
  ASSERT_TRUE(decrypted);

  EXPECT_EQ(*decrypted, "plaintext");
}
