// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/os_crypt/app_bound_encryption_provider_win.h"

#include <optional>
#include <string>
#include <tuple>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
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

}  // namespace

namespace features {
BASE_FEATURE(kAppBoundUserDataDirProtection,
             "AppBoundUserDataDirProtection",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAppBoundEncryptionKeyV3,
             "AppBoundEncryptionKeyV3",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

  std::optional<std::tuple<ReadWriteKeyData, OptionalReadOnlyKeyData>>
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
      return std::nullopt;
    }

    // Copy data to a vector.
    ReadWriteKeyData data(decrypted_key_string.cbegin(),
                          decrypted_key_string.cend());
    ::SecureZeroMemory(decrypted_key_string.data(),
                       decrypted_key_string.size());

    OptionalReadOnlyKeyData maybe_new_ciphertext_data;
    if (maybe_new_ciphertext) {
      maybe_new_ciphertext_data.emplace(maybe_new_ciphertext->cbegin(),
                                        maybe_new_ciphertext->cend());
    }
    return std::make_tuple(std::move(data),
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

  if (base::FeatureList::IsEnabled(features::kAppBoundUserDataDirProtection) &&
      support_level_ == os_crypt::SupportLevel::kNotUsingDefaultUserDataDir) {
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

  // There is no key, so generate a new one, but only on a fully supported
  // system. In unsupported systems the provider will support decrypt of
  // existing data (if App-Bound validation still passes) but not encrypt of any
  // new data.
  if (support_level_ != os_crypt::SupportLevel::kSupported) {
    std::move(callback).Run(
        kAppBoundDataPrefix,
        base::unexpected(KeyError::kPermanentlyUnavailable));
    return;
  }

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
  return ReadWriteKeyData(
      encrypted_key_with_header->cbegin() + sizeof(kCryptAppBoundKeyPrefix),
      encrypted_key_with_header->cend());
}

void AppBoundEncryptionProviderWin::StoreKey(
    base::span<const uint8_t> encrypted_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ReadWriteKeyData key(sizeof(kCryptAppBoundKeyPrefix) + encrypted_key.size());
  // Add header indicating this key is encrypted with App Bound provider.
  key.insert(key.cbegin(), std::begin(kCryptAppBoundKeyPrefix),
             std::end(kCryptAppBoundKeyPrefix));
  key.insert(key.cbegin() + sizeof(kCryptAppBoundKeyPrefix),
             encrypted_key.cbegin(), encrypted_key.cend());
  std::string base64_key = base::Base64Encode(key);
  // Store key.
  local_state_->SetString(kEncryptedKeyPrefName, base64_key);
}

void AppBoundEncryptionProviderWin::HandleEncryptedKey(
    ReadWriteKeyData decrypted_key,
    KeyCallback callback,
    const OptionalReadOnlyKeyData& encrypted_key) {
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
      std::make_tuple(std::move(decrypted_key), encrypted_key));
}

void AppBoundEncryptionProviderWin::StoreAndReplyWithKey(
    KeyCallback callback,
    std::optional<std::tuple<ReadWriteKeyData, const OptionalReadOnlyKeyData&>>
        key_pair) {
  if (!key_pair) {
    // Failure here indicates a temporary decryption failure.
    // TODO(crbug.com/382059244): Consider resetting the key here, like DPAPI
    // does.
    std::move(callback).Run(
        kAppBoundDataPrefix,
        base::unexpected(KeyError::kTemporarilyUnavailable));
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
