// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/native_file_system/native_file_system_permission_context_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/native_file_system/chrome_native_file_system_permission_context.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
ChromeNativeFileSystemPermissionContext*
NativeFileSystemPermissionContextFactory::GetForProfile(
    content::BrowserContext* profile) {
  return static_cast<ChromeNativeFileSystemPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ChromeNativeFileSystemPermissionContext*
NativeFileSystemPermissionContextFactory::GetForProfileIfExists(
    content::BrowserContext* profile) {
  return static_cast<ChromeNativeFileSystemPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
NativeFileSystemPermissionContextFactory*
NativeFileSystemPermissionContextFactory::GetInstance() {
  static base::NoDestructor<NativeFileSystemPermissionContextFactory> instance;
  return instance.get();
}

NativeFileSystemPermissionContextFactory::
    NativeFileSystemPermissionContextFactory()
    : BrowserContextKeyedServiceFactory(
          "NativeFileSystemPermissionContext",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

NativeFileSystemPermissionContextFactory::
    ~NativeFileSystemPermissionContextFactory() = default;

content::BrowserContext*
NativeFileSystemPermissionContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

KeyedService* NativeFileSystemPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new ChromeNativeFileSystemPermissionContext(profile);
}
