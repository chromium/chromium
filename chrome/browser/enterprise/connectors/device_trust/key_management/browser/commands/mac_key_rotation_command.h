// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_MAC_KEY_ROTATION_COMMAND_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_MAC_KEY_ROTATION_COMMAND_H_

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace enterprise_connectors {

class MacKeyRotationCommand : public KeyRotationCommand {
 public:
  MacKeyRotationCommand(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* local_prefs);

  ~MacKeyRotationCommand() override;

  // KeyRotationCommand:
  void Trigger(const Params& params, Callback callback) override;

 private:
  friend class MacKeyRotationCommandTest;

  // Processes the `result` of the key rotation and returns it to the currently
  // pending callback.
  void OnKeyRotated(KeyRotationManager::Result result);

  // Notifies the pending callback of a timeout.
  void OnKeyRotationTimeout();

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::raw_ptr<PrefService> local_prefs_;
  std::unique_ptr<KeyRotationManager> key_rotation_manager_;
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
