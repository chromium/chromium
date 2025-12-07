// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_MAC_KEY_ROTATION_COMMAND_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_MAC_KEY_ROTATION_COMMAND_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_types.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"

namespace enterprise_attestation {
class CloudManagementDelegate;
}  // namespace enterprise_attestation

namespace enterprise_management {
class DeviceManagementRequest;
}  // namespace enterprise_management

namespace network {
class SharedURLLoaderFactory;
}  // namespace network
namespace enterprise_connectors {

class MacKeyRotationCommand : public KeyRotationCommand {
 public:
  explicit MacKeyRotationCommand(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  MacKeyRotationCommand(
      std::unique_ptr<enterprise_attestation::CloudManagementDelegate>
          cloud_management_delegate);

  ~MacKeyRotationCommand() override;

  // KeyRotationCommand:
  void Trigger(const Params& params, Callback callback) override;

 private:
  friend class MacKeyRotationCommandTest;

  // Processes the `result` of the key rotation and returns it to the currently
  // pending callback.
  void OnKeyRotated(KeyRotationResult result);

  // Notifies the pending callback of a timeout.
  void OnKeyRotationTimeout();

  void UploadPublicKeyToDmServer(
      base::expected<const enterprise_management::DeviceManagementRequest,
                     KeyRotationResult> request);

  void OnUploadingPublicKeyCompleted(policy::DMServerJobResult result);

  bool IsDmTokenValid();

  std::unique_ptr<enterprise_attestation::CloudManagementDelegate>
      cloud_management_delegate_;

  // TODO(b/351201459): When IsDTCKeyRotationUploadedBySharedAPI is fully
  // launched, remove url_loader_factory_.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  base::OneShotTimer timeout_timer_;

  // Callback for the current request.
  Callback pending_callback_;

  // Used to issue Keychain APIs.
  std::unique_ptr<SecureEnclaveClient> client_;

  base::WeakPtrFactory<MacKeyRotationCommand> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_MAC_KEY_ROTATION_COMMAND_H_
