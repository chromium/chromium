// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class ChromeFileSystemAccessPermissionContext;

// Factory to get or create an instance of
// ChromeFileSystemAccessPermissionContext from a Profile.
class FileSystemAccessPermissionContextFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ChromeFileSystemAccessPermissionContext* GetForProfile(
      content::BrowserContext* profile);
  static ChromeFileSystemAccessPermissionContext* GetForProfileIfExists(
      content::BrowserContext* profile);
  static FileSystemAccessPermissionContextFactory* GetInstance();

 private:
  friend class base::NoDestructor<FileSystemAccessPermissionContextFactory>;

  FileSystemAccessPermissionContextFactory();
  ~FileSystemAccessPermissionContextFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(FileSystemAccessPermissionContextFactory);
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_FACTORY_H_
