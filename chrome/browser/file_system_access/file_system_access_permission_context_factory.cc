// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/profiles/profile.h"

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
    : ProfileKeyedServiceFactory(
          "FileSystemAccessPermissionContext",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

FileSystemAccessPermissionContextFactory::
    ~FileSystemAccessPermissionContextFactory() = default;

std::unique_ptr<KeyedService>
FileSystemAccessPermissionContextFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  return std::make_unique<ChromeFileSystemAccessPermissionContext>(profile);
}

void FileSystemAccessPermissionContextFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  auto* permission_context =
      GetForProfileIfExists(Profile::FromBrowserContext(context));
  if (permission_context)
    permission_context->FlushScheduledSaveSettingsCalls();
}
