// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_LINUX_KEY_PERSISTENCE_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_LINUX_KEY_PERSISTENCE_DELEGATE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/files/file.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"

namespace base {
class FilePath;
}  // namespace base

namespace enterprise_connectors {

class SigningKeyPair;

// Linux implementation of the KeyPersistenceDelegate interface.
class LinuxKeyPersistenceDelegate : public KeyPersistenceDelegate {
 public:
  LinuxKeyPersistenceDelegate();
  ~LinuxKeyPersistenceDelegate() override;

  // KeyPersistenceDelegate:
  bool CheckRotationPermissions() override;
  bool StoreKeyPair(KeyPersistenceDelegate::KeyTrustLevel trust_level,
                    std::vector<uint8_t> wrapped) override;
  scoped_refptr<SigningKeyPair> LoadKeyPair(
      KeyStorageType type,
      LoadPersistedKeyResult* result) override;
  scoped_refptr<SigningKeyPair> CreateKeyPair() override;
  bool PromoteTemporaryKeyPair() override;
  bool DeleteKeyPair(KeyStorageType type) override;

 private:
  friend class LinuxKeyPersistenceDelegateTest;

  // Statically sets a file path to be used by LinuxKeyPersistenceDelegate
  // instances when retrieve the key file path.
  static void SetFilePathForTesting(const base::FilePath& file_path);

  // Signing key file instance used for handling concurrency during the
  // key rotation process.
  std::optional<base::File> locked_file_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_LINUX_KEY_PERSISTENCE_DELEGATE_H_
