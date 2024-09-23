// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/local_binary_upload_service_factory.h"

#include "chrome/browser/enterprise/connectors/analysis/local_binary_upload_service.h"
#include "chrome/browser/enterprise/signals/system_signals_service_host_factory.h"
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
  static base::NoDestructor<LocalBinaryUploadServiceFactory> instance;
  return instance.get();
}

LocalBinaryUploadServiceFactory::LocalBinaryUploadServiceFactory()
    : ProfileKeyedServiceFactory(
          "LocalBinaryUploadService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(enterprise_signals::SystemSignalsServiceHostFactory::GetInstance());
}

KeyedService* LocalBinaryUploadServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new LocalBinaryUploadService(profile);
}

}  // namespace enterprise_connectors
