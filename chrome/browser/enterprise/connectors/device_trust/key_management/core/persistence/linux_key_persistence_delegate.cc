// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/linux_key_persistence_delegate.h"

#include <grp.h>
#include <sys/stat.h>

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/syslog_logging.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;
using BPKUP = enterprise_management::BrowserPublicKeyUploadResponse;

namespace enterprise_connectors {

namespace {

// Mode the signing key file should have.
constexpr int kFileMode = 0664;

// Group name the signing key file should have.
constexpr char kGroupName[] = "chromemgmt";

// Path to the signing key file differs based on chrome/chromium build.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
base::FilePath::CharType kDirPolicyPath[] =
    FILE_PATH_LITERAL("/etc/opt/chrome/policies");
#else
base::FilePath::CharType kDirPolicyPath[] =
    FILE_PATH_LITERAL("/etc/chromium/policies");
#endif

base::FilePath GetSigningKeyFilePath() {
  base::FilePath path(kDirPolicyPath);
  return path.Append(constants::kSigningKeyFilePath);
}

base::File OpenSigningKeyFile(uint32_t flags) {
  return base::File(GetSigningKeyFilePath(), flags);
}

}  // namespace

bool LinuxKeyPersistenceDelegate::CheckRotationPermissions() {
  auto signing_key_path = GetSigningKeyFilePath();
  auto file = base::File(signing_key_path,
                         base::File::FLAG_OPEN | base::File::FLAG_WRITE);

  if (!file.IsValid() ||
      (file.Lock(base::File::LockMode::kExclusive) != base::File::FILE_OK)) {
    SYSLOG(ERROR) << "Device trust key rotation failed. Could not acquire a "
                     "lock on the signing key storage.";
    return false;
  }

  int mode;
  if (!base::GetPosixFilePermissions(signing_key_path, &mode)) {
    SYSLOG(ERROR)
        << "Device trust key rotation failed. Could not get permissions "
           "for the signing key storage.";
    return false;
  }

  struct stat st;
  stat(signing_key_path.value().c_str(), &st);
  gid_t signing_key_file_gid = st.st_gid;
  struct group* chrome_mgmt_group = getgrnam(kGroupName);

  if (!chrome_mgmt_group || signing_key_file_gid != chrome_mgmt_group->gr_gid ||
      mode != kFileMode) {
    SYSLOG(ERROR) << "Device trust key rotation failed. Incorrect permissions "
                     "for signing key storage.";
    return false;
  }
  return true;
}

LinuxKeyPersistenceDelegate::~LinuxKeyPersistenceDelegate() = default;
const int kMaxBufferSize = 2048;
const char kSigningKeyName[] = "signingKey";
const char kSigningKeyTrustLevel[] = "trustLevel";

bool LinuxKeyPersistenceDelegate::StoreKeyPair(
    KeyPersistenceDelegate::KeyTrustLevel trust_level,
    std::vector<uint8_t> wrapped) {
  if (trust_level == BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED) {
    DCHECK_EQ(wrapped.size(), 0u);
    base::File file = OpenSigningKeyFile(base::File::FLAG_OPEN_TRUNCATED |
                                         base::File::FLAG_WRITE);
    return file.error_details() == base::File::FILE_OK;
  }

  base::File file =
      OpenSigningKeyFile(base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  if (!file.IsValid())
    return false;

  // Storing key and trust level information.
  base::Value keyinfo(base::Value::Type::DICTIONARY);
  const std::string encoded_key = base::Base64Encode(wrapped);
  keyinfo.SetKey(kSigningKeyName, base::Value(encoded_key));
  keyinfo.SetKey(kSigningKeyTrustLevel, base::Value(trust_level));
  std::string keyinfo_str;
  if (!base::JSONWriter::Write(keyinfo, &keyinfo_str)) {
    return false;
  }
  int bytes_written =
      file.WriteAtCurrentPos(keyinfo_str.c_str(), keyinfo_str.length());
  return bytes_written > 0;
}

KeyPersistenceDelegate::KeyInfo LinuxKeyPersistenceDelegate::LoadKeyPair() {
  base::File file =
      OpenSigningKeyFile(base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return invalid_key_info();
  }

  // Read key info.
  char keyinfo_str[kMaxBufferSize];
  int bytes_read = file.ReadAtCurrentPos(keyinfo_str, kMaxBufferSize);
  if (bytes_read <= 0) {
    return invalid_key_info();
  }

  // Get dictionary key info.
  auto keyinfo = base::JSONReader::Read(keyinfo_str);
  if (!keyinfo || !keyinfo->is_dict()) {
    return invalid_key_info();
  }

  // Get the trust level.
  auto stored_trust_level = keyinfo->FindIntKey(kSigningKeyTrustLevel);
  KeyTrustLevel trust_level = BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED;
  if (stored_trust_level == BPKUR::CHROME_BROWSER_TPM_KEY) {
    trust_level = BPKUR::CHROME_BROWSER_TPM_KEY;
  } else if (stored_trust_level == BPKUR::CHROME_BROWSER_OS_KEY) {
    trust_level = BPKUR::CHROME_BROWSER_OS_KEY;
  } else {
    return invalid_key_info();
  }

  // Get the key.
  std::string* encoded_key = keyinfo->FindStringKey(kSigningKeyName);
  std::string decoded_key;

  if (!encoded_key) {
    return invalid_key_info();
  }

  if (!base::Base64Decode(*encoded_key, &decoded_key)) {
    return invalid_key_info();
  }

  std::vector<uint8_t> key(decoded_key.begin(), decoded_key.end());
  return std::make_pair(trust_level, key);
}

std::unique_ptr<crypto::UnexportableKeyProvider>
LinuxKeyPersistenceDelegate::GetTpmBackedKeyProvider() {
  NOTIMPLEMENTED();  // TODO (http://b/210343211)
  return nullptr;
}

}  // namespace enterprise_connectors
