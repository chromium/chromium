// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_MAC_KEY_ROTATION_COMMAND_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_MAC_KEY_ROTATION_COMMAND_H_

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace enterprise_connectors {

class MacKeyRotationCommand : public KeyRotationCommand {
 public:
  explicit MacKeyRotationCommand(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~MacKeyRotationCommand() override;

  // KeyRotationCommand:
  void Trigger(const Params& params, Callback callback) override;

 private:
  friend class MacKeyRotationCommandTest;

  explicit MacKeyRotationCommand(
      std::unique_ptr<KeyRotationManager> key_rotation_manager);

  std::unique_ptr<KeyRotationManager> key_rotation_manager_;

  // Used to issue Keychain APIs.
  std::unique_ptr<SecureEnclaveClient> client_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_MAC_KEY_ROTATION_COMMAND_H_
