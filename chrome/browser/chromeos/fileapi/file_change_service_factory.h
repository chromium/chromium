// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_FILE_CHANGE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_FILE_CHANGE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace chromeos {

class FileChangeService;

// A factory which creates the service which notifies observers of file change
// events from external file systems. There will exist at most one service
// instance per browser context.
class FileChangeServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the singleton factory instance.
  static FileChangeServiceFactory* GetInstance();

  // Returns the service instance for the specified browser `context`. Note that
  // this will create the service if an instance does not already exist.
  FileChangeService* GetService(content::BrowserContext* context);

 private:
  friend class base::NoDestructor<FileChangeServiceFactory>;

  FileChangeServiceFactory();
  FileChangeServiceFactory(const FileChangeServiceFactory&) = delete;
  FileChangeServiceFactory& operator=(const FileChangeServiceFactory&) = delete;
  ~FileChangeServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_FILE_CHANGE_SERVICE_FACTORY_H_
