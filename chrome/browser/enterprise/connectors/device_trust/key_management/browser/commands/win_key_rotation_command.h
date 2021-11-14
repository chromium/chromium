// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_WIN_KEY_ROTATION_COMMAND_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_WIN_KEY_ROTATION_COMMAND_H_

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"

namespace enterprise_connectors {

class WinKeyRotationCommand : public KeyRotationCommand {
 public:
  WinKeyRotationCommand();
  ~WinKeyRotationCommand() override;

  // KeyRotationCommand:
  bool Trigger(const Params& params) override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_WIN_KEY_ROTATION_COMMAND_H_
