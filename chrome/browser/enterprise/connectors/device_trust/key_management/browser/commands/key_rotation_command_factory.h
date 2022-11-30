// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_KEY_ROTATION_COMMAND_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_KEY_ROTATION_COMMAND_FACTORY_H_

#include <memory>

#include "base/memory/scoped_refptr.h"

class PrefService;

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
  // object. The shared `url_loader_factory` is used in both the linux and mac
  // key rotation for mojo support, and the `local_prefs` is needed in the mac
  // key rotation exclusively for updating a local preference.
  virtual std::unique_ptr<KeyRotationCommand> CreateCommand(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* local_prefs);

 protected:
  static void SetFactoryInstanceForTesting(KeyRotationCommandFactory* factory);
  static void ClearFactoryInstanceForTesting();
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_KEY_ROTATION_COMMAND_FACTORY_H_
