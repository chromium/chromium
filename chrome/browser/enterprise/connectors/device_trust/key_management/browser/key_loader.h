// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_LOADER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_LOADER_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {
class DeviceManagementService;
}  // namespace policy

namespace enterprise_connectors {

class KeyLoader {
 public:
  struct DTCLoadKeyResult {
    // When a key was not loaded properly, `result` represents the loading
    // error.
    explicit DTCLoadKeyResult(LoadPersistedKeyResult result);

    // When a key was loaded properly, `key_pair` is the loaded key.
    explicit DTCLoadKeyResult(scoped_refptr<SigningKeyPair> key_pair);

    // When a key was loaded properly and a sync attempt was made, `key_pair` is
    // the loaded key and `status_code` is the HTTP result.
    DTCLoadKeyResult(int status_code, scoped_refptr<SigningKeyPair> key_pair);

    ~DTCLoadKeyResult();

    DTCLoadKeyResult(const DTCLoadKeyResult& other);
    DTCLoadKeyResult(DTCLoadKeyResult&&);

    DTCLoadKeyResult& operator=(const DTCLoadKeyResult& other);
    DTCLoadKeyResult& operator=(DTCLoadKeyResult&&);

    // HTTP response code from the key upload request.
    std::optional<int> status_code = std::nullopt;

    // Loaded signing key pair.
    scoped_refptr<SigningKeyPair> key_pair = nullptr;

    // Result of the local key loading operation.
    LoadPersistedKeyResult result = LoadPersistedKeyResult::kUnknown;
  };

  using LoadKeyCallback = base::OnceCallback<void(DTCLoadKeyResult)>;

  static std::unique_ptr<KeyLoader> Create(
      policy::DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  virtual ~KeyLoader() = default;

  // Loads the key from the permanent key storage. The result of the key
  // load/synchronization is returned via the `callback`.
  virtual void LoadKey(LoadKeyCallback callback) = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_LOADER_H_
