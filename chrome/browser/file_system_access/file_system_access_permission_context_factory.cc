// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
ChromeFileSystemAccessPermissionContext*
FileSystemAccessPermissionContextFactory::GetForProfile(
    content::BrowserContext* profile) {
  return static_cast<ChromeFileSystemAccessPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ChromeFileSystemAccessPermissionContext*
FileSystemAccessPermissionContextFactory::GetForProfileIfExists(
    content::BrowserContext* profile) {
  return static_cast<ChromeFileSystemAccessPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
FileSystemAccessPermissionContextFactory*
FileSystemAccessPermissionContextFactory::GetInstance() {
  static base::NoDestructor<FileSystemAccessPermissionContextFactory> instance;
  return instance.get();
}

FileSystemAccessPermissionContextFactory::
    FileSystemAccessPermissionContextFactory()
    : BrowserContextKeyedServiceFactory(
          "FileSystemAccessPermissionContext",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

FileSystemAccessPermissionContextFactory::
    ~FileSystemAccessPermissionContextFactory() = default;

content::BrowserContext*
FileSystemAccessPermissionContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

KeyedService* FileSystemAccessPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new ChromeFileSystemAccessPermissionContext(profile);
}
