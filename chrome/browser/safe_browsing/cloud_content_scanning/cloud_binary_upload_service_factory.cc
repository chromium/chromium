// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_context.h"

namespace safe_browsing {

// static
BinaryUploadService* CloudBinaryUploadServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<BinaryUploadService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */
                                                 true));
}

// static
CloudBinaryUploadServiceFactory*
CloudBinaryUploadServiceFactory::GetInstance() {
  static base::NoDestructor<CloudBinaryUploadServiceFactory> instance;
  return instance.get();
}

CloudBinaryUploadServiceFactory::CloudBinaryUploadServiceFactory()
    : ProfileKeyedServiceFactory(
          "CloudBinaryUploadService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

std::unique_ptr<KeyedService>
CloudBinaryUploadServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // TODO(b/226679912): Add logic to select service based on analysis settings.
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<CloudBinaryUploadService>(profile);
}

}  // namespace safe_browsing
