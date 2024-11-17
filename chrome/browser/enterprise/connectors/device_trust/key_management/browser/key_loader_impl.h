// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_LOADER_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_LOADER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_loader.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_util.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"

namespace enterprise_attestation {
class CloudManagementDelegate;
}  // namespace enterprise_attestation

namespace enterprise_connectors {

class KeyLoaderImpl : public KeyLoader {
 public:
  explicit KeyLoaderImpl(
      std::unique_ptr<enterprise_attestation::CloudManagementDelegate>
          management_delegate);

  ~KeyLoaderImpl() override;

  // KeyLoader:
  void LoadKey(LoadKeyCallback callback) override;

 private:
  // Performs the key synchronization on the `persisted_key`.
  void SynchronizePublicKey(LoadKeyCallback callback, LoadedKey persisted_key);

  // Uses the `upload_request` to upload the `key_pair` to the DM Server.
  void OnUploadPublicKeyRequestCreated(
      scoped_refptr<SigningKeyPair> key_pair,
      LoadKeyCallback callback,
      std::optional<const enterprise_management::DeviceManagementRequest>
          upload_request);

  // Builds the load key result using the DMServerJobResult `result` and
  // `key_pair`, and returns the result to the `callback`.
  void OnUploadPublicKeyCompleted(scoped_refptr<SigningKeyPair> key_pair,
                                  LoadKeyCallback callback,
                                  const policy::DMServerJobResult result);

  std::unique_ptr<enterprise_attestation::CloudManagementDelegate>
      cloud_management_delegate_;

  // Checker used to validate that non-background tasks should be
  // running on the original sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<KeyLoaderImpl> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_LOADER_IMPL_H_
