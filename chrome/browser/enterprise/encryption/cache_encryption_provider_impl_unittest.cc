// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/encryption/cache_encryption_provider_impl.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "crypto/aead.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/cache_encryption_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_encryption {

namespace {

class MockOSCryptAsync : public os_crypt_async::OSCryptAsync {
 public:
  MockOSCryptAsync() : OSCryptAsync({}) {}
  ~MockOSCryptAsync() override = default;

  MOCK_METHOD(void,
              GetInstance,
              (base::OnceCallback<void(os_crypt_async::Encryptor)>,
               os_crypt_async::Encryptor::Option),
              (override));
};

// Helper for creating an Encryptor for testing.
class TestEncryptor : public os_crypt_async::Encryptor {
 public:
  TestEncryptor(
      KeyRing keys,
      const std::string& provider_for_encryption,
      const std::string& provider_for_os_crypt_sync_compatible_encryption)
      : Encryptor(std::move(keys),
                  provider_for_encryption,
                  provider_for_os_crypt_sync_compatible_encryption) {}
};

}  // namespace

class CacheEncryptionProviderImplTest : public testing::Test {
 public:
  CacheEncryptionProviderImplTest() = default;
  ~CacheEncryptionProviderImplTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  testing::StrictMock<MockOSCryptAsync> os_crypt_async_;
  CacheEncryptionProviderImpl provider_{&os_crypt_async_,
                                        {},
                                        base::DoNothing()};
};

TEST_F(CacheEncryptionProviderImplTest, GetEncryptor) {
  std::optional<os_crypt_async::Encryptor> returned_encryptor;
  base::RunLoop run_loop;
  EXPECT_CALL(os_crypt_async_, GetInstance)
      .WillOnce([&](base::OnceCallback<void(os_crypt_async::Encryptor)> cb,
                    os_crypt_async::Encryptor::Option option) {
        os_crypt_async::Encryptor::KeyRing keys;
        keys.emplace("test_provider",
                     os_crypt_async::Encryptor::Key(
                         std::vector<uint8_t>(32, 1),
                         os_crypt_async::mojom::Algorithm::kAES256GCM));
        std::move(cb).Run(
            TestEncryptor(std::move(keys), "test_provider", "test_provider"));
      });

  mojo::Remote<network::mojom::CacheEncryptionProvider> remote(
      provider_.BindNewRemote());

  remote->GetEncryptor(
      base::BindLambdaForTesting([&](os_crypt_async::Encryptor encryptor_arg) {
        returned_encryptor.emplace(std::move(encryptor_arg));
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_TRUE(returned_encryptor.has_value());
}

TEST_F(CacheEncryptionProviderImplTest,
       GetEncryptedCacheEncryptionKey_KeyExists) {
  // Create an encryptor and encrypt a dummy key with it.
  os_crypt_async::Encryptor::KeyRing encryption_keys;
  encryption_keys.emplace("test_provider",
                          os_crypt_async::Encryptor::Key(
                              std::vector<uint8_t>(32, 1),
                              os_crypt_async::mojom::Algorithm::kAES256GCM));
  TestEncryptor encryptor(std::move(encryption_keys), "test_provider",
                          "test_provider");
  std::optional<std::vector<uint8_t>> key =
      encryptor.EncryptString(std::string(32, 2));
  ASSERT_TRUE(key.has_value());

  CacheEncryptionProviderImpl provider{&os_crypt_async_, *key,
                                       base::DoNothing()};

  EXPECT_CALL(os_crypt_async_, GetInstance)
      .WillOnce([&](base::OnceCallback<void(os_crypt_async::Encryptor)> cb,
                    os_crypt_async::Encryptor::Option option) {
        // Create an encryptor that can decrypt the key.
        os_crypt_async::Encryptor::KeyRing decryption_keys;
        decryption_keys.emplace(
            "test_provider", os_crypt_async::Encryptor::Key(
                                 std::vector<uint8_t>(32, 1),
                                 os_crypt_async::mojom::Algorithm::kAES256GCM));
        std::move(cb).Run(TestEncryptor(std::move(decryption_keys),
                                        "test_provider", "test_provider"));
      });

  mojo::Remote<network::mojom::CacheEncryptionProvider> remote(
      provider.BindNewRemote());

  base::test::TestFuture<const std::vector<uint8_t>&> future;
  remote->GetEncryptedCacheEncryptionKey(future.GetCallback());
  std::vector<uint8_t> returned_key = future.Take();

  EXPECT_EQ(returned_key, *key);
}

TEST_F(CacheEncryptionProviderImplTest,
       GetEncryptedCacheEncryptionKey_KeyDecryptionFails) {
  // Create an encryptor and encrypt a dummy key with it.
  os_crypt_async::Encryptor::KeyRing encryption_keys;
  encryption_keys.emplace("test_provider",
                          os_crypt_async::Encryptor::Key(
                              std::vector<uint8_t>(32, 1),
                              os_crypt_async::mojom::Algorithm::kAES256GCM));
  TestEncryptor encryptor(std::move(encryption_keys), "test_provider",
                          "test_provider");
  std::optional<std::vector<uint8_t>> key =
      encryptor.EncryptString(std::string(32, 2));
  ASSERT_TRUE(key.has_value());

  std::vector<uint8_t> stored_key;
  base::RunLoop run_loop;
  CacheEncryptionProviderImpl provider{
      &os_crypt_async_, *key,
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& new_key) {
        stored_key = new_key;
        run_loop.Quit();
      })};

  EXPECT_CALL(os_crypt_async_, GetInstance)
      .WillOnce([&](base::OnceCallback<void(os_crypt_async::Encryptor)> cb,
                    os_crypt_async::Encryptor::Option option) {
        // Create an encryptor that CANNOT decrypt the key.
        os_crypt_async::Encryptor::KeyRing decryption_keys;
        decryption_keys.emplace(
            "test_provider", os_crypt_async::Encryptor::Key(
                                 std::vector<uint8_t>(32, 99),
                                 os_crypt_async::mojom::Algorithm::kAES256GCM));
        std::move(cb).Run(TestEncryptor(std::move(decryption_keys),
                                        "test_provider", "test_provider"));
      });

  mojo::Remote<network::mojom::CacheEncryptionProvider> remote(
      provider.BindNewRemote());

  base::test::TestFuture<const std::vector<uint8_t>&> future;
  remote->GetEncryptedCacheEncryptionKey(future.GetCallback());
  std::vector<uint8_t> returned_key = future.Take();

  run_loop.Run();

  // A new key should have been generated and returned.
  EXPECT_FALSE(returned_key.empty());
  EXPECT_NE(returned_key, *key);
  EXPECT_EQ(returned_key, stored_key);
}

TEST_F(CacheEncryptionProviderImplTest,
       GetEncryptedCacheEncryptionKey_CreateNewKey) {
  std::vector<uint8_t> stored_key;
  base::RunLoop run_loop;
  CacheEncryptionProviderImpl provider{
      &os_crypt_async_,
      {},
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& key) {
        stored_key = key;
        run_loop.Quit();
      })};

  // Mock OSCryptAsync to return a test encryptor.
  EXPECT_CALL(os_crypt_async_, GetInstance)
      .WillOnce([&](base::OnceCallback<void(os_crypt_async::Encryptor)> cb,
                    os_crypt_async::Encryptor::Option option) {
        os_crypt_async::Encryptor::KeyRing keys;
        keys.emplace("test_provider",
                     os_crypt_async::Encryptor::Key(
                         std::vector<uint8_t>(32, 1),
                         os_crypt_async::mojom::Algorithm::kAES256GCM));
        std::move(cb).Run(
            TestEncryptor(std::move(keys), "test_provider", "test_provider"));
      });

  mojo::Remote<network::mojom::CacheEncryptionProvider> remote(
      provider.BindNewRemote());

  base::test::TestFuture<const std::vector<uint8_t>&> future;
  remote->GetEncryptedCacheEncryptionKey(future.GetCallback());
  std::vector<uint8_t> returned_key = future.Take();

  EXPECT_FALSE(returned_key.empty());
  EXPECT_EQ(returned_key, stored_key);

  // Now verify that the key can be decrypted.
  os_crypt_async::Encryptor::KeyRing keys;
  keys.emplace("test_provider",
               os_crypt_async::Encryptor::Key(
                   std::vector<uint8_t>(32, 1),
                   os_crypt_async::mojom::Algorithm::kAES256GCM));
  TestEncryptor encryptor(std::move(keys), "test_provider", "test_provider");
  std::optional<std::string> decrypted = encryptor.DecryptData(returned_key);
  EXPECT_TRUE(decrypted.has_value());
  EXPECT_EQ(decrypted->size(), 32u);
}

TEST_F(CacheEncryptionProviderImplTest,
       GetEncryptedCacheEncryptionKey_Reencrypt) {
  // Create an encryptor with an "old" key and encrypt a dummy key with it.
  os_crypt_async::Encryptor::KeyRing old_keys;
  old_keys.emplace("test_provider",
                   os_crypt_async::Encryptor::Key(
                       std::vector<uint8_t>(32, 1),
                       os_crypt_async::mojom::Algorithm::kAES256GCM));
  TestEncryptor old_encryptor(std::move(old_keys), "test_provider",
                              "test_provider");
  std::optional<std::vector<uint8_t>> old_encrypted_key =
      old_encryptor.EncryptString(std::string(32, 2));
  ASSERT_TRUE(old_encrypted_key.has_value());

  std::vector<uint8_t> stored_key;
  base::RunLoop run_loop;
  CacheEncryptionProviderImpl provider{
      &os_crypt_async_, *old_encrypted_key,
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& new_key) {
        stored_key = new_key;
        run_loop.Quit();
      })};

  EXPECT_CALL(os_crypt_async_, GetInstance)
      .WillOnce([&](base::OnceCallback<void(os_crypt_async::Encryptor)> cb,
                    os_crypt_async::Encryptor::Option option) {
        // Create a "new" encryptor. This one can decrypt the old key, but
        // encryption will be done with a new key.
        os_crypt_async::Encryptor::KeyRing new_keys;
        new_keys.emplace("test_provider",
                         os_crypt_async::Encryptor::Key(
                             std::vector<uint8_t>(32, 1),
                             os_crypt_async::mojom::Algorithm::kAES256GCM));
        new_keys.emplace("new_provider",
                         os_crypt_async::Encryptor::Key(
                             std::vector<uint8_t>(32, 3),
                             os_crypt_async::mojom::Algorithm::kAES256GCM));
        std::move(cb).Run(
            TestEncryptor(std::move(new_keys), "new_provider", "new_provider"));
      });

  mojo::Remote<network::mojom::CacheEncryptionProvider> remote(
      provider.BindNewRemote());

  base::test::TestFuture<const std::vector<uint8_t>&> future;
  remote->GetEncryptedCacheEncryptionKey(future.GetCallback());
  std::vector<uint8_t> returned_key = future.Take();
  run_loop.Run();

  // The key should have been re-encrypted with the new provider and stored.
  EXPECT_NE(returned_key, *old_encrypted_key);
  EXPECT_EQ(returned_key, stored_key);

  // Verify that the new key can be decrypted with the new encryptor.
  os_crypt_async::Encryptor::KeyRing new_keys;
  new_keys.emplace("new_provider",
                   os_crypt_async::Encryptor::Key(
                       std::vector<uint8_t>(32, 3),
                       os_crypt_async::mojom::Algorithm::kAES256GCM));
  TestEncryptor new_encryptor(std::move(new_keys), "new_provider",
                              "new_provider");
  std::optional<std::string> decrypted =
      new_encryptor.DecryptData(returned_key);
  ASSERT_TRUE(decrypted.has_value());
  EXPECT_EQ(*decrypted, std::string(32, 2));
}

}  // namespace enterprise_encryption
