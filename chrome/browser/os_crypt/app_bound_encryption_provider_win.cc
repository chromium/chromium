// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/os_crypt/app_bound_encryption_provider_win.h"

#include <optional>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "chrome/browser/os_crypt/app_bound_encryption_win.h"
#include "components/crash/core/common/crash_key.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "crypto/random.h"

namespace os_crypt_async {

namespace {

// Pref name for the encrypted key managed by app-bound encryption.
constexpr char kEncryptedKeyPrefName[] = "os_crypt.app_bound_encrypted_key";

// Key prefix for a key encrypted with app-bound Encryption. This is used to
// validate that the encrypted key data retrieved from the pref is valid.
constexpr uint8_t kCryptAppBoundKeyPrefix[] = {'A', 'P', 'P', 'B'};

// Tag for data encrypted with app-bound encryption key. This is used by
// OSCryptAsync to identify that data has been encrypted with this key.
constexpr char kAppBoundDataPrefix[] = "v20";

constexpr ProtectionLevel kCurrentProtectionLevel =
    ProtectionLevel::PROTECTION_PATH_VALIDATION;

// Determines whether or not a particular `error` and `last_error` pair is which
// type of `KeyError`. Returns std::nullopt if it has no opinion.
std::optional<KeyProvider::KeyError> DetermineErrorType(HRESULT error,
                                                        DWORD last_error) {
  if (!base::FeatureList::IsEnabled(
          features::kRegenerateKeyForCatastrophicFailures)) {
    return std::nullopt;
  }

  switch (error) {
    case elevation_service::Elevator::kErrorCouldNotDecryptWithSystemContext:
      if (last_error == ERROR_PATH_NOT_FOUND) {
        return KeyProvider::KeyError::kPermanentlyUnavailable;
      }
      break;
    case elevation_service::Elevator::kErrorCouldNotDecryptWithUserContext:
      if (last_error == static_cast<DWORD>(NTE_BAD_KEY_STATE)) {
        return KeyProvider::KeyError::kPermanentlyUnavailable;
      }
      break;
    default:
      return std::nullopt;
  }
  return std::nullopt;
}

}  // namespace

namespace features {
BASE_FEATURE(kAppBoundEncryptionKeyV3, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRegenerateKeyForCatastrophicFailures,
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace features

AppBoundEncryptionProviderWin::AppBoundEncryptionProviderWin(
    PrefService* local_state)
    : local_state_(local_state),
      com_worker_(base::ThreadPool::CreateCOMSTATaskRunner({base::MayBlock()})),
      support_level_(
          os_crypt::GetAppBoundEncryptionSupportLevel(local_state_)) {}

AppBoundEncryptionProviderWin::~AppBoundEncryptionProviderWin() = default;

class AppBoundEncryptionProviderWin::COMWorker {
 public:
  OptionalReadOnlyKeyData EncryptKey(ReadOnlyKeyData& decrypted_key) {
    std::string plaintext_string(decrypted_key.begin(), decrypted_key.end());
    std::string ciphertext;
    DWORD last_error;

    elevation_service::EncryptFlags flags{
        .use_latest_key =
            base::FeatureList::IsEnabled(features::kAppBoundEncryptionKeyV3)};
    HRESULT res = os_crypt::EncryptAppBoundString(kCurrentProtectionLevel,
                                                  plaintext_string, ciphertext,
                                                  last_error, &flags);

    base::UmaHistogramSparse("OSCrypt.AppBoundProvider.Encrypt.ResultCode",
                             res);

    if (!SUCCEEDED(res)) {
      LOG(ERROR) << "Unable to encrypt key. Result: "
                 << logging::SystemErrorCodeToString(res)
                 << " GetLastError: " << last_error;
      base::UmaHistogramSparse(
          "OSCrypt.AppBoundProvider.Encrypt.ResultLastError", last_error);
      return std::nullopt;
    }

    return ReadOnlyKeyData(ciphertext.cbegin(), ciphertext.cend());
  }

  base::expected<std::pair<ReadWriteKeyData, OptionalReadOnlyKeyData>,
                 KeyProvider::KeyError>
  DecryptKey(ReadOnlyKeyData& encrypted_key) {
    DWORD last_error;
    std::string encrypted_key_string(encrypted_key.begin(),
                                     encrypted_key.end());
    std::string decrypted_key_string;
    std::optional<std::string> maybe_new_ciphertext;
    elevation_service::EncryptFlags flags{
        .use_latest_key =
            base::FeatureList::IsEnabled(features::kAppBoundEncryptionKeyV3)};
    HRESULT res = os_crypt::DecryptAppBoundString(
        encrypted_key_string, decrypted_key_string, kCurrentProtectionLevel,
        maybe_new_ciphertext, last_error, &flags);

    base::UmaHistogramSparse("OSCrypt.AppBoundProvider.Decrypt.ResultCode",
                             res);

    if (!SUCCEEDED(res)) {
      LOG(ERROR) << "Unable to decrypt key. Result: "
                 << logging::SystemErrorCodeToString(res)
                 << " GetLastError: " << last_error;
      base::UmaHistogramSparse(
          "OSCrypt.AppBoundProvider.Decrypt.ResultLastError", last_error);
      const auto error_type = DetermineErrorType(res, last_error);
      // Try and resolve this error to see if it might be a permanent one that
      // would result in local key being deleted. If it cannot be determined,
      // assume it's temporary.
      return base::unexpected(
          error_type.value_or(KeyProvider::KeyError::kTemporarilyUnavailable));
    }

    // Copy data to a vector.
    ReadWriteKeyData data(decrypted_key_string.cbegin(),
                          decrypted_key_string.cend());
    ::SecureZeroMemory(decrypted_key_string.data(),
                       decrypted_key_string.size());

    std::optional<std::vector<uint8_t>> maybe_new_ciphertext_data;
    if (maybe_new_ciphertext) {
      maybe_new_ciphertext_data.emplace(maybe_new_ciphertext->cbegin(),
                                        maybe_new_ciphertext->cend());
    }
    return std::make_pair(std::move(data),
                          std::move(maybe_new_ciphertext_data));
  }
};

// static
void AppBoundEncryptionProviderWin::RegisterLocalPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kEncryptedKeyPrefName, std::string());
}

void AppBoundEncryptionProviderWin::GetKey(KeyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto encrypted_key_data = RetrieveEncryptedKey();

  base::UmaHistogramEnumeration(
      "OSCrypt.AppBoundProvider.KeyRetrieval.Status",
      encrypted_key_data.error_or(KeyRetrievalStatus::kSuccess));

  base::UmaHistogramEnumeration("OSCrypt.AppBoundEncryption.SupportLevel",
                                support_level_);

  if (support_level_ == os_crypt::SupportLevel::kNotSystemLevel) {
    // No service. No App-Bound APIs are available, so fail now.
    std::move(callback).Run(
        kAppBoundDataPrefix,
        base::unexpected(KeyError::kPermanentlyUnavailable));
    return;
  }

  if (support_level_ == os_crypt::SupportLevel::kNotUsingDefaultUserDataDir) {
    // Modified user data dir, signal temporarily unavailable. This means
    // decrypts will not work, but neither will new encrypts. Since the key is
    // temporarily unavailable, no data should be lost.
    std::move(callback).Run(
        kAppBoundDataPrefix,
        base::unexpected(KeyError::kTemporarilyUnavailable));
    return;
  }

  if (encrypted_key_data.has_value()) {
    // There is a key, perform the decryption on the background worker.
    com_worker_.AsyncCall(&AppBoundEncryptionProviderWin::COMWorker::DecryptKey)
        .WithArgs(std::move(encrypted_key_data.value()))
        .Then(
            base::BindOnce(&AppBoundEncryptionProviderWin::StoreAndReplyWithKey,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  // Clean up bad keys.
  switch (encrypted_key_data.error()) {
    case KeyRetrievalStatus::kSuccess:
      NOTREACHED();
    case KeyRetrievalStatus::kKeyNotFound:
      // Not found means nothing to do.
      break;
    case KeyRetrievalStatus::kKeyDecodeFailure:
    case KeyRetrievalStatus::kInvalidKeyHeader:
    case KeyRetrievalStatus::kKeyTooShort:
      local_state_->ClearPref(kEncryptedKeyPrefName);
      break;
  }

  // There is no key or the key was invalid, so generate a new one, but only on
  // a fully supported system. In unsupported systems the provider will support
  // decrypt of existing data (if App-Bound validation still passes) but not
  // encrypt of any new data.
  if (support_level_ != os_crypt::SupportLevel::kSupported) {
    std::move(callback).Run(
        kAppBoundDataPrefix,
        base::unexpected(KeyError::kPermanentlyUnavailable));
    return;
  }

  GenerateAndPersistNewKeyInternal(std::move(callback));
}

void AppBoundEncryptionProviderWin::GenerateAndPersistNewKeyInternal(
    KeyCallback callback) {
  const auto random_key = crypto::RandBytesAsVector(
      os_crypt_async::Encryptor::Key::kAES256GCMKeySize);
  // Take a copy of the key. This will be returned as the unencrypted key for
  // the provider, once the encryption operation is complete. This key is
  // securely cleared later on in `StoreAndReplyWithKey`.
  ReadWriteKeyData decrypted_key(random_key.cbegin(), random_key.cend());
  // Perform the encryption on the background worker.
  com_worker_.AsyncCall(&AppBoundEncryptionProviderWin::COMWorker::EncryptKey)
      .WithArgs(std::move(random_key))
      .Then(base::BindOnce(&AppBoundEncryptionProviderWin::HandleEncryptedKey,
                           weak_factory_.GetWeakPtr(), std::move(decrypted_key),
                           std::move(callback)));
}

bool AppBoundEncryptionProviderWin::UseForEncryption() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return support_level_ == os_crypt::SupportLevel::kSupported;
}

bool AppBoundEncryptionProviderWin::IsCompatibleWithOsCryptSync() {
  return false;
}

base::expected<AppBoundEncryptionProviderWin::ReadWriteKeyData,
               AppBoundEncryptionProviderWin::KeyRetrievalStatus>
AppBoundEncryptionProviderWin::RetrieveEncryptedKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!local_state_->HasPrefPath(kEncryptedKeyPrefName)) {
    return base::unexpected(KeyRetrievalStatus::kKeyNotFound);
  }

  const std::string base64_encrypted_key =
      local_state_->GetString(kEncryptedKeyPrefName);

  std::optional<ReadWriteKeyData> encrypted_key_with_header =
      base::Base64Decode(base64_encrypted_key);

  if (!encrypted_key_with_header) {
    return base::unexpected(KeyRetrievalStatus::kKeyDecodeFailure);
  }

  if (!std::equal(std::begin(kCryptAppBoundKeyPrefix),
                  std::end(kCryptAppBoundKeyPrefix),
                  encrypted_key_with_header->cbegin())) {
    return base::unexpected(KeyRetrievalStatus::kInvalidKeyHeader);
  }

  // Trim off the key prefix.
  const auto key = ReadWriteKeyData(
      encrypted_key_with_header->cbegin() + sizeof(kCryptAppBoundKeyPrefix),
      encrypted_key_with_header->cend());

  // This is an encrypted random key and encrypting N uniformly random bits
  // requires >= N bits of ciphertext - follows from Shannon entropy theory and
  // invertibility constraints. However the exact length is
  // determined by the elevated service and might vary.
  if (key.size() < os_crypt_async::Encryptor::Key::kAES256GCMKeySize) {
    return base::unexpected(KeyRetrievalStatus::kKeyTooShort);
  }

  return key;
}

void AppBoundEncryptionProviderWin::StoreKey(
    base::span<const uint8_t> encrypted_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto key = base::HeapArray<uint8_t>::Uninit(sizeof(kCryptAppBoundKeyPrefix) +
                                              encrypted_key.size());
  key.copy_prefix_from(base::span(kCryptAppBoundKeyPrefix));
  key.subspan(sizeof(kCryptAppBoundKeyPrefix))
      .copy_from_nonoverlapping(encrypted_key);
  std::string base64_key = base::Base64Encode(key);
  // Store key.
  local_state_->SetString(kEncryptedKeyPrefName, base64_key);
}

void AppBoundEncryptionProviderWin::HandleEncryptedKey(
    ReadWriteKeyData decrypted_key,
    KeyCallback callback,
    OptionalReadOnlyKeyData encrypted_key) {
  if (!encrypted_key) {
    ::SecureZeroMemory(decrypted_key.data(), decrypted_key.size());
    // Failure here means encryption failed, which is considered a permanent
    // error.
    std::move(callback).Run(
        kAppBoundDataPrefix,
        base::unexpected(KeyError::kPermanentlyUnavailable));
    return;
  }

  StoreAndReplyWithKey(
      std::move(callback),
      std::make_pair(std::move(decrypted_key), std::move(encrypted_key)));
}

void AppBoundEncryptionProviderWin::StoreAndReplyWithKey(
    KeyCallback callback,
    base::expected<std::pair<ReadWriteKeyData, OptionalReadOnlyKeyData>,
                   KeyProvider::KeyError> key_pair) {
  if (!key_pair.has_value()) {
    // This can only happen in the decrypt path.
    if (key_pair.error() == KeyProvider::KeyError::kPermanentlyUnavailable) {
      // A decrypt has failed permanently. A new key must be generated,
      // encrypted, and returned.
      GenerateAndPersistNewKeyInternal(std::move(callback));
      return;
    }
    std::move(callback).Run(kAppBoundDataPrefix,
                            base::unexpected(key_pair.error()));
    return;
  }

  auto& [decrypted_key, maybe_encrypted_key] = *key_pair;

  if (maybe_encrypted_key) {
    StoreKey(*maybe_encrypted_key);
  }

  // Constructor takes a copy.
  Encryptor::Key key(decrypted_key, mojom::Algorithm::kAES256GCM);
  ::SecureZeroMemory(decrypted_key.data(), decrypted_key.size());
  std::move(callback).Run(kAppBoundDataPrefix, std::move(key));
}

}  // namespace os_crypt_async
