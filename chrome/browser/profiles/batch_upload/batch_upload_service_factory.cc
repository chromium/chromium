// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/batch_upload/batch_upload_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/profiles/batch_upload_ui_delegate.h"
#include "components/signin/public/base/signin_switches.h"

namespace {

ProfileSelections CreateBatchUploadProfileSelections() {
  if (base::FeatureList::IsEnabled(switches::kBatchUploadDesktop)) {
    return ProfileSelections::BuildForRegularProfile();
  }

  return ProfileSelections::BuildNoProfilesSelected();
}

}  // namespace

BatchUploadServiceFactory::BatchUploadServiceFactory()
    : ProfileKeyedServiceFactory("BatchUpload",
                                 CreateBatchUploadProfileSelections()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

BatchUploadServiceFactory::~BatchUploadServiceFactory() = default;

// static
BatchUploadService* BatchUploadServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<BatchUploadService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BatchUploadServiceFactory* BatchUploadServiceFactory::GetInstance() {
  static base::NoDestructor<BatchUploadServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
BatchUploadServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // TODO(b/359146556): consider passing in the needed services instead of the
  // `profile` when the providers will be implemented.
  return std::make_unique<BatchUploadService>(
      *Profile::FromBrowserContext(context),
      std::make_unique<BatchUploadUIDelegate>());
}
