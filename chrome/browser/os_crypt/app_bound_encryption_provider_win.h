// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OS_CRYPT_APP_BOUND_ENCRYPTION_PROVIDER_WIN_H_
#define CHROME_BROWSER_OS_CRYPT_APP_BOUND_ENCRYPTION_PROVIDER_WIN_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
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

namespace os_crypt_async {

namespace features {

// If enabled, will re-generate a new key for catastrophic failures. See
// `DetermineErrorType` in the cc file for the two current cases.
BASE_DECLARE_FEATURE(kRegenerateKeyForCatastrophicFailures);

// If enabled, then data encrypted by an isolated process will be bound to the
// isolation state and no longer be decryptable by an un-isolated process.
BASE_DECLARE_FEATURE(kEncryptWithIsolatedState);

}  // namespace features

// Pref name for the encrypted key managed by app-bound encryption.
inline constexpr char kAppBoundEncryptedKeyPrefName[] =
    "os_crypt.app_bound_encrypted_key";

// Key prefix for a key encrypted with app-bound Encryption. This is used to
// validate that the encrypted key data retrieved from the pref is valid.
inline constexpr uint8_t kCryptAppBoundKeyPrefix[] = {'A', 'P', 'P', 'B'};

// Tag for data encrypted with app-bound encryption key. This is used by
// OSCryptAsync to identify that data has been encrypted with this key.
inline constexpr char kAppBoundDataPrefix[] = "v20";

class AppBoundEncryptionProviderWin : public os_crypt_async::KeyProvider {
 public:
  // Creates the key provider backed by pref service `local_state`. If
  // `force_protection_level` is specified then after init the encrypted pref
  // will be using the specified `ProtectionLevel` - either during key retrieval
  // a re-encrypt will occur to the specified `ProtectionLevel`, or if the pref
  // is non existent then the specified `ProtectionLevel` will be used for
  // initial encrypt.
  AppBoundEncryptionProviderWin(
      PrefService* local_state,
      std::optional<ProtectionLevel> force_protection_level);
  ~AppBoundEncryptionProviderWin() override;

  // Not copyable.
  AppBoundEncryptionProviderWin(const AppBoundEncryptionProviderWin&) = delete;
  AppBoundEncryptionProviderWin& operator=(
      const AppBoundEncryptionProviderWin&) = delete;

  static void RegisterLocalPrefs(PrefRegistrySimple* registry);

  // Returns true if this provider has previously stored an encrypted key to
  // local state.
  bool IsKeyStored() const;

  // os_crypt_async::KeyProvider interface.
  void GetKey(KeyCallback callback) override;
  bool UseForEncryption() override;
  bool IsCompatibleWithOsCryptSync() override;

 private:
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
    kKeyTooShort = 4,
    kMaxValue = kKeyTooShort,
  };

  void GenerateAndPersistNewKeyInternal(KeyCallback callback);
  base::expected<std::vector<uint8_t>, KeyRetrievalStatus>
  RetrieveEncryptedKey() const;
  void HandleEncryptedKey(ReadWriteKeyData decrypted_key,
                          KeyCallback callback,
                          OptionalReadOnlyKeyData encrypted_key);
  void StoreAndReplyWithKey(
      KeyCallback callback,
      base::expected<std::pair<ReadWriteKeyData, OptionalReadOnlyKeyData>,
                     KeyProvider::KeyError> key_pair);
  void StoreKey(base::span<const uint8_t> encrypted_key);

  raw_ptr<PrefService> local_state_ GUARDED_BY_CONTEXT(sequence_checker_);

  const std::optional<ProtectionLevel> force_protection_level_;

  class COMWorker;
  base::SequenceBound<COMWorker> com_worker_;

  const os_crypt::SupportLevel support_level_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AppBoundEncryptionProviderWin> weak_factory_{this};
};

}  // namespace os_crypt_async

#endif  // CHROME_BROWSER_OS_CRYPT_APP_BOUND_ENCRYPTION_PROVIDER_WIN_H_
