// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notebooks/notebooks_service_factory.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/notebooks/notebooks_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/notebooks/internal/empty_notebooks_service.h"
#include "components/notebooks/internal/notebooks_service_impl.h"
#include "components/notebooks/public/features.h"
#include "components/notebooks/public/notebooks_eligibility_service.h"
#include "components/notebooks/public/notebooks_service.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store_service.h"

namespace notebooks {

// static
NotebooksService* NotebooksServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<NotebooksService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
NotebooksServiceFactory* NotebooksServiceFactory::GetInstance() {
  static base::NoDestructor<NotebooksServiceFactory> instance;
  return instance.get();
}

NotebooksServiceFactory::NotebooksServiceFactory()
    : ProfileKeyedServiceFactory(
          "NotebooksService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(NotebooksEligibilityServiceFactory::GetInstance());
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

NotebooksServiceFactory::~NotebooksServiceFactory() = default;

std::unique_ptr<KeyedService>
NotebooksServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  NotebooksEligibilityService* eligibility_service =
      NotebooksEligibilityServiceFactory::GetForProfile(profile);
  if (!eligibility_service || !eligibility_service->IsEligible()) {
    return std::make_unique<EmptyNotebooksService>();
  }

  auto processor = std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
      syncer::NOTEBOOK, base::BindRepeating(&syncer::ReportUnrecoverableError,
                                            chrome::GetChannel()));

  syncer::OnceDataTypeStoreFactory store_factory =
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();

  return std::make_unique<NotebooksServiceImpl>(std::move(processor),
                                                std::move(store_factory));
}

}  // namespace notebooks
