// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/os_crypt/app_bound_encryption_provider_win.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/prefs/mock_pref_change_callback.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::StrictMock;

namespace os_crypt {

namespace {

class MockAppBoundEncryptionOverrides
    : public AppBoundEncryptionOverridesForTesting {
 public:
  MOCK_METHOD(HRESULT,
              EncryptAppBoundString,
              (ProtectionLevel level,
               const std::string& plaintext,
               std::string& ciphertext,
               DWORD& last_error,
               elevation_service::EncryptFlags* flags),
              (override));

  MOCK_METHOD(HRESULT,
              DecryptAppBoundString,
              (const std::string& ciphertext,
               std::string& plaintext,
               ProtectionLevel protection_level,
               std::optional<std::string>& new_ciphertext,
               DWORD& last_error,
               elevation_service::EncryptFlags* flags),
              (override));

  MOCK_METHOD(SupportLevel,
              GetAppBoundEncryptionSupportLevel,
              (PrefService * local_state),
              (override));
};

HRESULT DefaultEncrypt(ProtectionLevel level,
                       const std::string& plaintext,
                       std::string& ciphertext,
                       DWORD& last_error,
                       elevation_service::EncryptFlags* flags) {
  ciphertext = base::StrCat({"SECRET", plaintext, "DATA"});
  last_error = 0;
  return S_OK;
}

HRESULT DefaultDecrypt(const std::string& ciphertext,
                       std::string& plaintext,
                       ProtectionLevel protection_level,
                       std::optional<std::string>& new_ciphertext,
                       DWORD& last_error,
                       elevation_service::EncryptFlags* flags) {
  const size_t prefix_len = sizeof("SECRET") - 1;
  const size_t suffix_len = sizeof("DATA") - 1;

  // Length of middle = total - prefix - suffix
  plaintext = ciphertext.substr(prefix_len,
                                ciphertext.length() - prefix_len - suffix_len);
  last_error = 0;
  return S_OK;
}

// Simulates a failure due to DPAPI key lost which results in a new key being
// generated.
HRESULT PermanentlyFailingDecrypt(const std::string& ciphertext,
                                  std::string& plaintext,
                                  ProtectionLevel protection_level,
                                  std::optional<std::string>& new_ciphertext,
                                  DWORD& last_error,
                                  elevation_service::EncryptFlags* flags) {
  // See `DetermineErrorType` in `app_bound_encryption_provider_win.cc`.
  last_error = NTE_BAD_KEY_STATE;
  return elevation_service::Elevator::kErrorCouldNotDecryptWithUserContext;
}

}  // namespace

TEST(AppBoundEncryptionProvider, TestEncryptDecrypt) {
  std::string ciphertext;
  {
    DWORD last_error;
    EXPECT_HRESULT_SUCCEEDED(
        DefaultEncrypt(ProtectionLevel::PROTECTION_PATH_VALIDATION, "text",
                       ciphertext, last_error, nullptr));
  }
  {
    DWORD last_error;
    std::string plaintext;
    std::optional<std::string> new_ciphertext;
    EXPECT_HRESULT_SUCCEEDED(DefaultDecrypt(
        ciphertext, plaintext, ProtectionLevel::PROTECTION_PATH_VALIDATION,
        new_ciphertext, last_error, /*flags=*/nullptr));
    EXPECT_EQ(plaintext, "text");
  }
}

TEST(AppBoundEncryptionProvider, Basic) {
  base::test::ScopedFeatureList feature(
      os_crypt_async::features::kRegenerateKeyForCatastrophicFailures);

  base::test::TaskEnvironment env;
  ::testing::StrictMock<MockAppBoundEncryptionOverrides> mock_app_bound;

  SetOverridesForTesting(&mock_app_bound);

  ON_CALL(mock_app_bound, EncryptAppBoundString)
      .WillByDefault(::testing::Invoke(DefaultEncrypt));
  ON_CALL(mock_app_bound, DecryptAppBoundString)
      .WillByDefault(::testing::Invoke(DefaultDecrypt));
  ON_CALL(mock_app_bound, GetAppBoundEncryptionSupportLevel)
      .WillByDefault(::testing::Return(SupportLevel::kSupported));

  const char* kPrefName = "os_crypt.app_bound_encrypted_key";

  TestingPrefServiceSimple prefs;
  MockPrefChangeCallback pref_observer(&prefs);
  PrefChangeRegistrar registrar;
  registrar.Init(&prefs);
  registrar.Add(kPrefName, pref_observer.GetCallback());
  // Part 1: Generate and store a new encrypted key into the prefs. The provider
  // should generate a random key, encrypt it with the app-bound mocks, then
  // persist the encrypted key to store.
  EXPECT_CALL(pref_observer, OnPreferenceChanged(_)).Times(1);

  os_crypt_async::AppBoundEncryptionProviderWin::RegisterLocalPrefs(
      prefs.registry());

  // `Key` has no public constructor and is move-only so use a std::optional as
  // a handy container.
  std::optional<os_crypt_async::Encryptor::Key> encryption_key;
  std::string encrypted_key;
  EXPECT_CALL(mock_app_bound, GetAppBoundEncryptionSupportLevel).Times(1);
  EXPECT_CALL(mock_app_bound, EncryptAppBoundString).Times(1);
  {
    os_crypt_async::AppBoundEncryptionProviderWin provider(&prefs);
    base::test::TestFuture<
        const std::string&,
        base::expected<os_crypt_async::Encryptor::Key,
                       os_crypt_async::KeyProvider::KeyError>>
        future;
    provider.GetKey(future.GetCallback());
    auto [tag, key] = future.Take();
    EXPECT_EQ(tag, "v20");
    ASSERT_TRUE(key.has_value());
    encryption_key.emplace(std::move(*key));
    encrypted_key = prefs.GetString(kPrefName);
    EXPECT_FALSE(encrypted_key.empty());
  }

  ::testing::Mock::VerifyAndClearExpectations(&mock_app_bound);
  ::testing::Mock::VerifyAndClearExpectations(&pref_observer);

  // Part 2: Retrieve an existing key from the pref store, the provider should
  // retrieve the encrypted key, decrypt using the app-bound mocks, and supply
  // the same decrypted key as before.
  EXPECT_CALL(pref_observer, OnPreferenceChanged).Times(0);
  EXPECT_CALL(mock_app_bound, GetAppBoundEncryptionSupportLevel).Times(1);
  EXPECT_CALL(mock_app_bound, DecryptAppBoundString).Times(1);
  {
    os_crypt_async::AppBoundEncryptionProviderWin provider(&prefs);
    base::test::TestFuture<
        const std::string&,
        base::expected<os_crypt_async::Encryptor::Key,
                       os_crypt_async::KeyProvider::KeyError>>
        future;
    provider.GetKey(future.GetCallback());
    const auto& [_, key] = future.Get();
    ASSERT_TRUE(key.has_value());
    // The key returned should be the same as before.
    EXPECT_EQ(*key, *encryption_key);

    EXPECT_EQ(prefs.GetString(kPrefName), encrypted_key);
  }

  ::testing::Mock::VerifyAndClearExpectations(&mock_app_bound);
  ::testing::Mock::VerifyAndClearExpectations(&pref_observer);

  // Part 3: Verify that a normal/temporary failure results in no key but it is
  // not deleted.
  EXPECT_CALL(mock_app_bound, GetAppBoundEncryptionSupportLevel).Times(1);
  // Fake a temporarily failing decrypt.
  EXPECT_CALL(mock_app_bound, DecryptAppBoundString)
      .WillOnce(::testing::Return(E_FAIL));
  EXPECT_CALL(pref_observer, OnPreferenceChanged).Times(0);

  {
    os_crypt_async::AppBoundEncryptionProviderWin provider(&prefs);
    base::test::TestFuture<
        const std::string&,
        base::expected<os_crypt_async::Encryptor::Key,
                       os_crypt_async::KeyProvider::KeyError>>
        future;
    provider.GetKey(future.GetCallback());
    const auto& [_, key] = future.Get();

    // A failure like E_FAIL is temporary, the key is not available but hasn't
    // been discarded.
    ASSERT_EQ(os_crypt_async::KeyProvider::KeyError::kTemporarilyUnavailable,
              key.error());

    // Pref is not modified or deleted.
    EXPECT_EQ(prefs.GetString(kPrefName), encrypted_key);
  }

  ::testing::Mock::VerifyAndClearExpectations(&mock_app_bound);
  ::testing::Mock::VerifyAndClearExpectations(&pref_observer);

  // Part 4: Verify that a permanent failure results in a key but it's
  // different.
  EXPECT_CALL(mock_app_bound, GetAppBoundEncryptionSupportLevel).Times(1);
  EXPECT_CALL(mock_app_bound, DecryptAppBoundString)
      .WillOnce(PermanentlyFailingDecrypt);
  // An encrypt is called at this stage, since the new random key needs to be
  // encrypted.
  EXPECT_CALL(mock_app_bound, EncryptAppBoundString).Times(1);
  EXPECT_CALL(pref_observer, OnPreferenceChanged).Times(1);

  std::optional<os_crypt_async::Encryptor::Key> new_encryption_key;
  std::string new_encrypted_key;
  {
    os_crypt_async::AppBoundEncryptionProviderWin provider(&prefs);
    base::test::TestFuture<
        const std::string&,
        base::expected<os_crypt_async::Encryptor::Key,
                       os_crypt_async::KeyProvider::KeyError>>
        future;
    provider.GetKey(future.GetCallback());
    auto [_, key] = future.Take();

    ASSERT_TRUE(key.has_value());
    // The key returned should be different as before.
    EXPECT_NE(*key, *encryption_key);
    new_encryption_key.emplace(std::move(*key));

    // Pref is present, but different from before (as key was regenerated).
    new_encrypted_key = prefs.GetString(kPrefName);
    EXPECT_FALSE(new_encrypted_key.empty());
    EXPECT_NE(new_encrypted_key, encrypted_key);
  }

  ::testing::Mock::VerifyAndClearExpectations(&mock_app_bound);
  ::testing::Mock::VerifyAndClearExpectations(&pref_observer);

  // Part 5: Verify that now the issue is hopefully fixed, the retrieval of the
  // new key works and it's the same.
  EXPECT_CALL(mock_app_bound, GetAppBoundEncryptionSupportLevel).Times(1);
  EXPECT_CALL(mock_app_bound, DecryptAppBoundString).Times(1);
  EXPECT_CALL(pref_observer, OnPreferenceChanged).Times(0);

  {
    os_crypt_async::AppBoundEncryptionProviderWin provider(&prefs);
    base::test::TestFuture<
        const std::string&,
        base::expected<os_crypt_async::Encryptor::Key,
                       os_crypt_async::KeyProvider::KeyError>>
        future;
    provider.GetKey(future.GetCallback());
    const auto& [_, key] = future.Get();

    ASSERT_TRUE(key.has_value());
    // The key returned should be same as the new one generated in part 4.
    EXPECT_EQ(*key, *new_encryption_key);
  }

  ::testing::Mock::VerifyAndClearExpectations(&mock_app_bound);
  ::testing::Mock::VerifyAndClearExpectations(&pref_observer);

  // Part 5: Verify that the kill-switch works as intended, and a failure
  // similar to part 4 does not result in a new key.
  {
    base::test::ScopedFeatureList killswitch;
    killswitch.InitAndDisableFeature(
        os_crypt_async::features::kRegenerateKeyForCatastrophicFailures);
    EXPECT_CALL(mock_app_bound, GetAppBoundEncryptionSupportLevel).Times(1);
    EXPECT_CALL(mock_app_bound, DecryptAppBoundString)
        .WillOnce(PermanentlyFailingDecrypt);
    EXPECT_CALL(pref_observer, OnPreferenceChanged).Times(0);

    {
      os_crypt_async::AppBoundEncryptionProviderWin provider(&prefs);
      base::test::TestFuture<
          const std::string&,
          base::expected<os_crypt_async::Encryptor::Key,
                         os_crypt_async::KeyProvider::KeyError>>
          future;
      provider.GetKey(future.GetCallback());
      const auto& [_, key] = future.Get();

      // The failure is now temporary, the key is not available but hasn't been
      // discarded.
      ASSERT_EQ(os_crypt_async::KeyProvider::KeyError::kTemporarilyUnavailable,
                key.error());

      // Pref is not modified or deleted.
      EXPECT_EQ(prefs.GetString(kPrefName), new_encrypted_key);
    }
  }

  ::testing::Mock::VerifyAndClearExpectations(&mock_app_bound);
  ::testing::Mock::VerifyAndClearExpectations(&pref_observer);

  // Part 6: In part 5 the pref was preserved even for a 'permanent' failure due
  // to the killswitch, so verify one last time that it's still there and the
  // key can be retrieved.
  EXPECT_CALL(mock_app_bound, GetAppBoundEncryptionSupportLevel).Times(1);
  EXPECT_CALL(mock_app_bound, DecryptAppBoundString).Times(1);
  EXPECT_CALL(pref_observer, OnPreferenceChanged).Times(0);

  {
    os_crypt_async::AppBoundEncryptionProviderWin provider(&prefs);
    base::test::TestFuture<
        const std::string&,
        base::expected<os_crypt_async::Encryptor::Key,
                       os_crypt_async::KeyProvider::KeyError>>
        future;
    provider.GetKey(future.GetCallback());
    const auto& [_, key] = future.Get();

    ASSERT_TRUE(key.has_value());
    // The key returned should be same as the new one generated in part 4.
    EXPECT_EQ(*key, *new_encryption_key);
  }

  SetOverridesForTesting(nullptr);
}

}  // namespace os_crypt
