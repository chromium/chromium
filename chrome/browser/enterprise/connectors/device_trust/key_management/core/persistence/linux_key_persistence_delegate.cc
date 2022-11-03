// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/linux_key_persistence_delegate.h"

#include <fcntl.h>
#include <grp.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "base/syslog_logging.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/ec_signing_key.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/unexportable_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;
using BPKUP = enterprise_management::BrowserPublicKeyUploadResponse;

namespace enterprise_connectors {

namespace {

// The mode the signing key file should have.
constexpr int kFileMode = 0664;

constexpr int kMaxBufferSize = 2048;
constexpr char kSigningKeyName[] = "signingKey";
constexpr char kSigningKeyTrustLevel[] = "trustLevel";

// The path to the policy directory should be the same as
// `chrome::DIR_POLICY_FILES`. This file duplicates those constants, as it runs
// in the chrome-management-service binary and thus cannot directly use
// `chrome::DIR_POLICY_FILES`.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
base::FilePath::CharType kDirPolicyPath[] =
    FILE_PATH_LITERAL("/etc/opt/chrome/policies");
#else
base::FilePath::CharType kDirPolicyPath[] =
    FILE_PATH_LITERAL("/etc/chromium/policies");
#endif

absl::optional<base::FilePath>& GetTestFilePathStorage() {
  static base::NoDestructor<absl::optional<base::FilePath>> storage;
  return *storage;
}

base::FilePath GetSigningKeyFilePath() {
  auto& storage = GetTestFilePathStorage();
  if (storage) {
    return storage.value();
  }
  base::FilePath path(kDirPolicyPath);
  return path.Append(constants::kSigningKeyFilePath);
}

base::File OpenSigningKeyFile(uint32_t flags) {
  return base::File(GetSigningKeyFilePath(), flags);
}

bool RecordFailure(KeyPersistenceOperation operation,
                   KeyPersistenceError error,
                   const std::string& log_message) {
  RecordError(operation, error);
  SYSLOG(ERROR) << log_message;
  return false;
}

}  // namespace

LinuxKeyPersistenceDelegate::LinuxKeyPersistenceDelegate() = default;
LinuxKeyPersistenceDelegate::~LinuxKeyPersistenceDelegate() = default;

bool LinuxKeyPersistenceDelegate::CheckRotationPermissions() {
  base::FilePath signing_key_path = GetSigningKeyFilePath();
  locked_file_ = base::File(signing_key_path, base::File::FLAG_OPEN |
                                                  base::File::FLAG_READ |
                                                  base::File::FLAG_WRITE);
  if (!locked_file_ || !locked_file_->IsValid() ||
      HANDLE_EINTR(flock(locked_file_->GetPlatformFile(), LOCK_EX | LOCK_NB)) ==
          -1) {
    return RecordFailure(
        KeyPersistenceOperation::kCheckPermissions,
        KeyPersistenceError::kLockPersistenceStorageFailed,
        "Device trust key rotation failed. Could not acquire lock on the "
        "signing key storage.");
  }

  int mode;
  if (!base::GetPosixFilePermissions(signing_key_path, &mode)) {
    return RecordFailure(
        KeyPersistenceOperation::kCheckPermissions,
        KeyPersistenceError::kRetrievePersistenceStoragePermissionsFailed,
        "Device trust key rotation failed. Could not get permissions "
        "for the signing key storage.");
  }

  struct stat st;
  stat(signing_key_path.value().c_str(), &st);
  gid_t signing_key_file_gid = st.st_gid;
  struct group* chrome_mgmt_group = getgrnam(constants::kGroupName);

  if (!chrome_mgmt_group || signing_key_file_gid != chrome_mgmt_group->gr_gid ||
      mode != kFileMode) {
    RecordFailure(KeyPersistenceOperation::kCheckPermissions,
                  KeyPersistenceError::kInvalidPermissionsForPersistenceStorage,
                  "Device trust key rotation failed. Incorrect permissions "
                  "for the signing key storage.");
  }
  return true;
}

bool LinuxKeyPersistenceDelegate::StoreKeyPair(
    KeyPersistenceDelegate::KeyTrustLevel trust_level,
    std::vector<uint8_t> wrapped) {
  base::File file = OpenSigningKeyFile(base::File::FLAG_OPEN_TRUNCATED |
                                       base::File::FLAG_WRITE);
  if (trust_level == BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED) {
    DCHECK_EQ(wrapped.size(), 0u);
    if (file.error_details() == base::File::FILE_OK) {
      return true;
    };

    return RecordFailure(KeyPersistenceOperation::kStoreKeyPair,
                           KeyPersistenceError::kDeleteKeyPairFailed,
                           "Device trust key rotation failed. Failed to delete "
                           "the signing key pair.");
  }

  if (!file.IsValid()) {
    return RecordFailure(
        KeyPersistenceOperation::kStoreKeyPair,
        KeyPersistenceError::kOpenPersistenceStorageFailed,
        "Device trust key rotation failed. Could not open the signing key file "
        "for writing.");
  }

  // Storing key and trust level information.
  base::Value keyinfo(base::Value::Type::DICTIONARY);
  const std::string encoded_key = base::Base64Encode(wrapped);
  keyinfo.SetKey(kSigningKeyName, base::Value(encoded_key));
  keyinfo.SetKey(kSigningKeyTrustLevel, base::Value(trust_level));
  std::string keyinfo_str;
  if (!base::JSONWriter::Write(keyinfo, &keyinfo_str)) {
    return RecordFailure(
        KeyPersistenceOperation::kStoreKeyPair,
        KeyPersistenceError::kJsonFormatSigningKeyPairFailed,
        "Device trust key rotation failed. Could not format signing key "
        "information for storage.");
  }

  if (file.WriteAtCurrentPos(keyinfo_str.c_str(), keyinfo_str.length()) > 0) {
    return true;
  }

  return RecordFailure(KeyPersistenceOperation::kStoreKeyPair,
                       KeyPersistenceError::kWritePersistenceStorageFailed,
                       "Device trust key rotation failed. Could not write to "
                       "the signing key storage.");
}

std::unique_ptr<SigningKeyPair> LinuxKeyPersistenceDelegate::LoadKeyPair() {
  std::string file_content;
  if (!base::ReadFileToStringWithMaxSize(GetSigningKeyFilePath(), &file_content,
                                         kMaxBufferSize)) {
    RecordFailure(
        KeyPersistenceOperation::kLoadKeyPair,
        KeyPersistenceError::kReadPersistenceStorageFailed,
        "Device trust key rotation failed. Failed to read from the signing key "
        "storage.");
    return nullptr;
  }

  // Get dictionary key info.
  auto keyinfo = base::JSONReader::Read(file_content);
  if (!keyinfo || !keyinfo->is_dict()) {
    RecordFailure(
        KeyPersistenceOperation::kLoadKeyPair,
        KeyPersistenceError::kInvalidSigningKeyPairFormat,
        "Device trust key rotation failed. Invalid signing key format found in "
        "signing key storage.");
    return nullptr;
  }

  // Get the trust level.
  auto stored_trust_level = keyinfo->FindIntKey(kSigningKeyTrustLevel);
  if (!stored_trust_level.has_value()) {
    RecordFailure(KeyPersistenceOperation::kLoadKeyPair,
                  KeyPersistenceError::kKeyPairMissingTrustLevel,
                  "Device trust key rotation failed. Signing key pair missing "
                  "trust level details.");
    return nullptr;
  }

  if (stored_trust_level != BPKUR::CHROME_BROWSER_OS_KEY) {
    RecordFailure(KeyPersistenceOperation::kLoadKeyPair,
                  KeyPersistenceError::kInvalidTrustLevel,
                  "Device trust key rotation failed. Invalid trust level for "
                  "the signing key.");
    return nullptr;
  }

  // Get the key.
  std::string* encoded_key = keyinfo->FindStringKey(kSigningKeyName);
  std::string decoded_key;
  if (!encoded_key) {
    RecordFailure(
        KeyPersistenceOperation::kLoadKeyPair,
        KeyPersistenceError::kKeyPairMissingSigningKey,
        "Device trust key rotation failed. Signing key pair missing signing "
        "key details.");
    return nullptr;
  }

  if (!base::Base64Decode(*encoded_key, &decoded_key)) {
    RecordFailure(
        KeyPersistenceOperation::kLoadKeyPair,
        KeyPersistenceError::kFailureDecodingSigningKey,
        "Device trust key rotation failed. Failure decoding the signing key.");
    return nullptr;
  }
  std::vector<uint8_t> wrapped =
      std::vector<uint8_t>(decoded_key.begin(), decoded_key.end());

  auto provider = std::make_unique<ECSigningKeyProvider>();
  auto signing_key = provider->FromWrappedSigningKeySlowly(wrapped);
  if (!signing_key) {
    RecordFailure(
        KeyPersistenceOperation::kLoadKeyPair,
        KeyPersistenceError::kCreateSigningKeyFromWrappedFailed,
        "Device trust key rotation failed. Failure creating a signing key "
        "object from the signing key details.");
    return nullptr;
  }

  return std::make_unique<SigningKeyPair>(std::move(signing_key),
                                          BPKUR::CHROME_BROWSER_OS_KEY);
}

std::unique_ptr<SigningKeyPair> LinuxKeyPersistenceDelegate::CreateKeyPair() {
  // TODO (http://b/210343211): TPM support for linux.
  auto provider = std::make_unique<ECSigningKeyProvider>();
  auto algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};
  auto signing_key = provider->GenerateSigningKeySlowly(algorithm);

  if (!signing_key) {
    RecordFailure(
        KeyPersistenceOperation::kCreateKeyPair,
        KeyPersistenceError::kGenerateOSSigningKeyFailed,
        "Device trust key rotation failed. Failure generating a new OS signing "
        "key");
    return nullptr;
  }

  return std::make_unique<SigningKeyPair>(std::move(signing_key),
                                          BPKUR::CHROME_BROWSER_OS_KEY);
}

// static
void LinuxKeyPersistenceDelegate::SetFilePathForTesting(
    const base::FilePath& file_path) {
  auto& storage = GetTestFilePathStorage();
  storage.emplace(file_path);
}

}  // namespace enterprise_connectors
