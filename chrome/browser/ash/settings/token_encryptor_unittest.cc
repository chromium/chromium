// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/token_encryptor.h"

#include <string>

#include "base/test/scoped_running_on_chromeos.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::Eq;
using ::testing::Ne;

// Make up a system salt for testing that's obviously not random but which has a
// normal length and isn't some weird boundary case like all-zeros.
std::string SaltForTesting() {
  std::string salt;
  salt.reserve(16);
  for (size_t i = 0; i < salt.capacity(); ++i) {
    salt.push_back(7 * i + 1);
  }
  return salt;
}

TEST(TokenEncryptorTest, TestRoundTrip) {
  base::test::ScopedRunningOnChromeOS running_on_chromeos;
  CryptohomeTokenEncryptor token_encryptor(SaltForTesting());

  std::string token_a("1234567890");
  std::string token_b("abcdef");

  std::string encrypted_a = token_encryptor.EncryptWithSystemSalt(token_a);
  std::string encrypted_b = token_encryptor.EncryptWithSystemSalt(token_b);
  EXPECT_THAT(encrypted_a, Ne(token_a));
  EXPECT_THAT(encrypted_b, Ne(token_b));

  std::string decrypted_a = token_encryptor.DecryptWithSystemSalt(encrypted_a);
  std::string decrypted_b = token_encryptor.DecryptWithSystemSalt(encrypted_b);
  EXPECT_THAT(decrypted_a, Eq(token_a));
  EXPECT_THAT(decrypted_b, Eq(token_b));
}

TEST(TokenEncryptorTest, TestRoundTripForWeakEncryption) {
  base::test::ScopedRunningOnChromeOS running_on_chromeos;
  CryptohomeTokenEncryptor token_encryptor(SaltForTesting());

  std::string token_a("1234567890");
  std::string token_b("abcdef");

  std::string encrypted_a = token_encryptor.WeakEncryptWithSystemSalt(token_a);
  std::string encrypted_b = token_encryptor.WeakEncryptWithSystemSalt(token_b);
  EXPECT_THAT(encrypted_a, Ne(token_a));
  EXPECT_THAT(encrypted_b, Ne(token_b));

  std::string decrypted_a =
      token_encryptor.WeakDecryptWithSystemSalt(encrypted_a);
  std::string decrypted_b =
      token_encryptor.WeakDecryptWithSystemSalt(encrypted_b);
  EXPECT_THAT(decrypted_a, Eq(token_a));
  EXPECT_THAT(decrypted_b, Eq(token_b));
}

// The encryptor doesn't actually do any encryption when running outside of
// ChromeOS. Test that this is what actually happens.
TEST(TokenEncryptorTest, TestNoopOutsideOfChromeOs) {
  CryptohomeTokenEncryptor token_encryptor(SaltForTesting());

  std::string token_a("1234567890");
  std::string token_b("abcdef");

  std::string encrypted_a = token_encryptor.EncryptWithSystemSalt(token_a);
  std::string encrypted_b = token_encryptor.EncryptWithSystemSalt(token_b);
  EXPECT_THAT(encrypted_a, Eq(token_a));
  EXPECT_THAT(encrypted_b, Eq(token_b));

  std::string decrypted_a = token_encryptor.DecryptWithSystemSalt(encrypted_a);
  std::string decrypted_b = token_encryptor.DecryptWithSystemSalt(encrypted_b);
  EXPECT_THAT(decrypted_a, Eq(token_a));
  EXPECT_THAT(decrypted_b, Eq(token_b));
}

}  // namespace
}  // namespace ash
