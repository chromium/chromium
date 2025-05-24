// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OS_CRYPT_APP_BOUND_ENCRYPTION_PROVIDER_WIN_H_
#define CHROME_BROWSER_OS_CRYPT_APP_BOUND_ENCRYPTION_PROVIDER_WIN_H_

#include <optional>
#include <string>
#include <tuple>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "base/types/expected.h"
#include "base/win/windows_types.h"
#include "chrome/browser/os_crypt/app_bound_encryption_win.h"
#include "components/os_crypt/async/browser/key_provider.h"

class PrefService;
class PrefRegistrySimple;

namespace os_crypt {
FORWARD_DECLARE_TEST(AppBoundEncryptionWinReencryptTest, KeyProviderTest);
FORWARD_DECLARE_TEST(AppBoundEncryptionProvider, Basic);
}  // namespace os_crypt

namespace os_crypt_async {

namespace features {

// If enabled, App-Bound encryption will request that the version 3 key be used
// for data encryption by the elevated service.
BASE_DECLARE_FEATURE(kAppBoundEncryptionKeyV3);

// If enabled, will re-generate a new key for catastrophic failures. See
// `DetermineErrorType` in the cc file for the two current cases.
BASE_DECLARE_FEATURE(kRegenerateKeyForCatastrophicFailures);

}  // namespace features

class AppBoundEncryptionProviderWin : public os_crypt_async::KeyProvider {
 public:
  explicit AppBoundEncryptionProviderWin(PrefService* local_state);
  ~AppBoundEncryptionProviderWin() override;

  // Not copyable.
  AppBoundEncryptionProviderWin(const AppBoundEncryptionProviderWin&) = delete;
  AppBoundEncryptionProviderWin& operator=(
      const AppBoundEncryptionProviderWin&) = delete;

  static void RegisterLocalPrefs(PrefRegistrySimple* registry);

 private:
  FRIEND_TEST_ALL_PREFIXES(os_crypt::AppBoundEncryptionWinReencryptTest,
                           KeyProviderTest);
  FRIEND_TEST_ALL_PREFIXES(os_crypt::AppBoundEncryptionProvider, Basic);

  using ReadOnlyKeyData = const std::vector<uint8_t>;
  using ReadWriteKeyData = std::vector<uint8_t>;
  using OptionalReadWriteKeyData = std::optional<ReadWriteKeyData>;
  using OptionalReadOnlyKeyData = std::optional<ReadOnlyKeyData>;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class KeyRetrievalStatus {
    kSuccess = 0,
    kKeyNotFound = 1,
    kKeyDecodeFailure = 2,
    kInvalidKeyHeader = 3,
    kMaxValue = kInvalidKeyHeader,
  };

  // os_crypt_async::KeyProvider interface.
  void GetKey(KeyCallback callback) override;
  bool UseForEncryption() override;
  bool IsCompatibleWithOsCryptSync() override;

  void GenerateAndPersistNewKeyInternal(KeyCallback callback);
  base::expected<std::vector<uint8_t>, KeyRetrievalStatus>
  RetrieveEncryptedKey();
  void HandleEncryptedKey(ReadWriteKeyData decrypted_key,
                          KeyCallback callback,
                          const OptionalReadOnlyKeyData& encrypted_key);
  void StoreAndReplyWithKey(
      KeyCallback callback,
      base::expected<
          std::tuple<ReadWriteKeyData, const OptionalReadOnlyKeyData&>,
          KeyProvider::KeyError> key_pair);
  void StoreKey(base::span<const uint8_t> encrypted_key);

  raw_ptr<PrefService> local_state_ GUARDED_BY_CONTEXT(sequence_checker_);

  class COMWorker;
  base::SequenceBound<COMWorker> com_worker_;

  const os_crypt::SupportLevel support_level_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AppBoundEncryptionProviderWin> weak_factory_{this};
};

}  // namespace os_crypt_async

#endif  // CHROME_BROWSER_OS_CRYPT_APP_BOUND_ENCRYPTION_PROVIDER_WIN_H_
