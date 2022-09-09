// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/local_binary_upload_service_factory.h"

#include "chrome/browser/enterprise/connectors/analysis/local_binary_upload_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace enterprise_connectors {

// static
safe_browsing::BinaryUploadService*
LocalBinaryUploadServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<safe_browsing::BinaryUploadService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */
                                                 true));
}

// static
LocalBinaryUploadServiceFactory*
LocalBinaryUploadServiceFactory::GetInstance() {
  return base::Singleton<LocalBinaryUploadServiceFactory>::get();
}

LocalBinaryUploadServiceFactory::LocalBinaryUploadServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "LocalBinaryUploadService",
          BrowserContextDependencyManager::GetInstance()) {}

KeyedService* LocalBinaryUploadServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new LocalBinaryUploadService();
}

content::BrowserContext*
LocalBinaryUploadServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace enterprise_connectors
