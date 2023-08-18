// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_LOADER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_LOADER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/common/key_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {
class BrowserDMTokenStorage;
class DeviceManagementService;
}  // namespace policy

namespace enterprise_connectors {

class KeyLoader {
 public:
  struct DTCLoadKeyResult {
    DTCLoadKeyResult();
    DTCLoadKeyResult(int status_code, scoped_refptr<SigningKeyPair> key_pair);
    DTCLoadKeyResult(DTCLoadKeyResult&&);
    DTCLoadKeyResult& operator=(DTCLoadKeyResult&&);
    ~DTCLoadKeyResult();
    // HTTP response code from the key upload request.
    absl::optional<int> status_code;
    // Permanent signing key.
    scoped_refptr<SigningKeyPair> key_pair;
  };

  using LoadKeyCallback = base::OnceCallback<void(DTCLoadKeyResult)>;

  static std::unique_ptr<KeyLoader> Create(
      policy::BrowserDMTokenStorage* dm_token_storage,
      policy::DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  virtual ~KeyLoader() = default;

  // Loads the key from the permanent key storage. The result of the key
  // load/synchronization is returned via the `callback`.
  virtual void LoadKey(LoadKeyCallback callback) = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_LOADER_H_
