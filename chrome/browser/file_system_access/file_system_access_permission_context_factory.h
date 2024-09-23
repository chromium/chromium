// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class ChromeFileSystemAccessPermissionContext;

// Factory to get or create an instance of
// ChromeFileSystemAccessPermissionContext from a Profile.
class FileSystemAccessPermissionContextFactory
    : public ProfileKeyedServiceFactory {
 public:
  static ChromeFileSystemAccessPermissionContext* GetForProfile(
      content::BrowserContext* profile);
  static ChromeFileSystemAccessPermissionContext* GetForProfileIfExists(
      content::BrowserContext* profile);
  static FileSystemAccessPermissionContextFactory* GetInstance();

  FileSystemAccessPermissionContextFactory(
      const FileSystemAccessPermissionContextFactory&) = delete;
  FileSystemAccessPermissionContextFactory& operator=(
      const FileSystemAccessPermissionContextFactory&) = delete;

 private:
  friend class base::NoDestructor<FileSystemAccessPermissionContextFactory>;

  FileSystemAccessPermissionContextFactory();
  ~FileSystemAccessPermissionContextFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_FACTORY_H_
