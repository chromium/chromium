// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_FILE_CHANGE_SERVICE_BRIDGE_FACTORY_H_
#define CHROME_BROWSER_LACROS_FILE_CHANGE_SERVICE_BRIDGE_FACTORY_H_

#include <memory>

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

// The factory responsible for creation of `FileChangeServiceBridge` instances.
// Instances are automatically created on creation of guest/regular profiles.
class FileChangeServiceBridgeFactory : public ProfileKeyedServiceFactory {
 public:
  FileChangeServiceBridgeFactory();
  FileChangeServiceBridgeFactory(const FileChangeServiceBridgeFactory&) =
      delete;
  FileChangeServiceBridgeFactory& operator=(
      const FileChangeServiceBridgeFactory&) = delete;
  ~FileChangeServiceBridgeFactory() override;

  // Returns the singleton factory instance. Note that the factory is created
  // lazily and will not exist until the first invocation of `GetInstance()`.
  static FileChangeServiceBridgeFactory* GetInstance();

 private:
  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_LACROS_FILE_CHANGE_SERVICE_BRIDGE_FACTORY_H_
