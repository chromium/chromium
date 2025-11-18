// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/encryption/cache_encryption_provider_impl.h"

#include "base/test/task_environment.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include <optional>

#include "base/test/bind.h"
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
  CacheEncryptionProviderImpl provider_{&os_crypt_async_};
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
        std::move(cb).Run(TestEncryptor(std::move(keys), "test_provider",
                                        "test_provider"));
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

}  // namespace enterprise_encryption
