// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/local_binary_upload_service_factory.h"

#include "chrome/browser/enterprise/connectors/analysis/local_binary_upload_service.h"
#include "chrome/browser/profiles/profile.h"

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
    : ProfileKeyedServiceFactory(
          "LocalBinaryUploadService",
          ProfileSelections::BuildForRegularAndIncognito()) {}

KeyedService* LocalBinaryUploadServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new LocalBinaryUploadService();
}

}  // namespace enterprise_connectors
