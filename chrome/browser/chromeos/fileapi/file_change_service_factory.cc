// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/file_change_service_factory.h"

#include "chrome/browser/chromeos/fileapi/file_change_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {

// static
FileChangeServiceFactory* FileChangeServiceFactory::GetInstance() {
  static base::NoDestructor<FileChangeServiceFactory> instance;
  return instance.get();
}

FileChangeService* FileChangeServiceFactory::GetService(
    content::BrowserContext* context) {
  return static_cast<FileChangeService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

FileChangeServiceFactory::FileChangeServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "FileChangeService",
          BrowserContextDependencyManager::GetInstance()) {}

FileChangeServiceFactory::~FileChangeServiceFactory() = default;

KeyedService* FileChangeServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new FileChangeService();
}

bool FileChangeServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace chromeos
