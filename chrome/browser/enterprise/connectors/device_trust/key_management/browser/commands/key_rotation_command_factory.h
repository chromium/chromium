// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_KEY_ROTATION_COMMAND_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_KEY_ROTATION_COMMAND_FACTORY_H_

#include <memory>

#include "base/memory/scoped_refptr.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace enterprise_connectors {

class KeyRotationCommand;

class KeyRotationCommandFactory {
 public:
  virtual ~KeyRotationCommandFactory();

  static KeyRotationCommandFactory* GetInstance();

  // Creates a platform-specific key rotation command
  // object. This object takes in a shared url loader factory as
  // a parameter, which is used for mojo support in the linux key rotation.
  virtual std::unique_ptr<KeyRotationCommand> CreateCommand(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

 protected:
  static void SetFactoryInstanceForTesting(KeyRotationCommandFactory* factory);
  static void ClearFactoryInstanceForTesting();
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_KEY_ROTATION_COMMAND_FACTORY_H_
