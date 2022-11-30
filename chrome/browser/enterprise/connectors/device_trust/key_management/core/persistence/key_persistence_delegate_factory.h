// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_KEY_PERSISTENCE_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_KEY_PERSISTENCE_DELEGATE_FACTORY_H_

#include <memory>

namespace enterprise_connectors {

class KeyPersistenceDelegate;

class KeyPersistenceDelegateFactory {
 public:
  virtual ~KeyPersistenceDelegateFactory() = default;

  // Returns the singleton factory instance.
  static KeyPersistenceDelegateFactory* GetInstance();

  // Returns a new KeyPersistenceDelegate instance.
  virtual std::unique_ptr<KeyPersistenceDelegate>
  CreateKeyPersistenceDelegate();

 protected:
  static void SetInstanceForTesting(KeyPersistenceDelegateFactory* factory);
  static void ClearInstanceForTesting();
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_PERSISTENCE_KEY_PERSISTENCE_DELEGATE_FACTORY_H_
