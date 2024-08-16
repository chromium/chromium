// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/product_specifications/product_specifications_service_factory.h"

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "components/commerce/core/product_specifications/product_specifications_sync_bridge.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store_service.h"

namespace {

std::unique_ptr<syncer::ClientTagBasedDataTypeProcessor>
CreateChangeProcessor() {
  return std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
      syncer::PRODUCT_COMPARISON,
      base::BindRepeating(&syncer::ReportUnrecoverableError,
                          chrome::GetChannel()));
}

}  // namespace

namespace commerce {

// static
commerce::ProductSpecificationsService*
ProductSpecificationsServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  // Not available in incognito mode. Only available if kProductSpecifications
  // is enabled as the service is gated along with the rest of the feature.
  if (!context->IsOffTheRecord() &&
      base::FeatureList::IsEnabled(commerce::kProductSpecifications)) {
    return static_cast<commerce::ProductSpecificationsService*>(
        GetInstance()->GetServiceForBrowserContext(context, true));
  }
  return nullptr;
}

// static
ProductSpecificationsServiceFactory*
ProductSpecificationsServiceFactory::GetInstance() {
  static base::NoDestructor<ProductSpecificationsServiceFactory> instance;
  return instance.get();
}

ProductSpecificationsServiceFactory::ProductSpecificationsServiceFactory()
    : ProfileKeyedServiceFactory(
          "ProductSpecificationsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

ProductSpecificationsServiceFactory::~ProductSpecificationsServiceFactory() =
    default;

std::unique_ptr<KeyedService>
ProductSpecificationsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<commerce::ProductSpecificationsService>(
      DataTypeStoreServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context))
          ->GetStoreFactory(),
      CreateChangeProcessor());
}

}  // namespace commerce
