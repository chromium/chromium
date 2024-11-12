// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/shopping_service_factory.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/commerce/product_specifications/product_specifications_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"
#include "chrome/browser/power_bookmarks/power_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/commerce/content/browser/commerce_tab_helper.h"
#include "components/commerce/content/browser/web_extractor_impl.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "components/commerce/core/proto/commerce_subscription_db_content.pb.h"
#include "components/commerce/core/proto/parcel_tracking_db_content.pb.h"
#include "components/commerce/core/shopping_service.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/variations_service_utils.h"
#include "content/public/browser/storage_partition.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/commerce/core/proto/discounts_db_content.pb.h"  // nogncheck
#endif

namespace commerce {

// static
ShoppingServiceFactory* ShoppingServiceFactory::GetInstance() {
  static base::NoDestructor<ShoppingServiceFactory> instance;
  return instance.get();
}

// static
ShoppingService* ShoppingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ShoppingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ShoppingService* ShoppingServiceFactory::GetForBrowserContextIfExists(
    content::BrowserContext* context) {
  return static_cast<ShoppingService*>(
      GetInstance()->GetServiceForBrowserContext(context, false));
}

ShoppingServiceFactory::ShoppingServiceFactory()
    : ProfileKeyedServiceFactory(
          "ShoppingService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(BookmarkModelFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  DependsOn(PowerBookmarkServiceFactory::GetInstance());
  DependsOn(SessionProtoDBFactory<
            commerce_subscription_db::CommerceSubscriptionContentProto>::
                GetInstance());
  DependsOn(SessionProtoDBFactory<
            parcel_tracking_db::ParcelTrackingContent>::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
#if !BUILDFLAG(IS_ANDROID)
  DependsOn(SessionProtoDBFactory<
            discounts_db::DiscountsContentProto>::GetInstance());
#endif
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(commerce::ProductSpecificationsServiceFactory::GetInstance());
  DependsOn(TabRestoreServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
ShoppingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ShoppingService>(
      GetCurrentCountryCode(g_browser_process->variations_service()),
      g_browser_process->GetApplicationLocale(),
      BookmarkModelFactory::GetInstance()->GetForBrowserContext(context),
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
      profile->GetPrefs(), IdentityManagerFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      SessionProtoDBFactory<commerce_subscription_db::
                                CommerceSubscriptionContentProto>::GetInstance()
          ->GetForProfile(context),
      PowerBookmarkServiceFactory::GetForBrowserContext(context),
      ProductSpecificationsServiceFactory::GetForBrowserContext(context),
#if !BUILDFLAG(IS_ANDROID)
      SessionProtoDBFactory<discounts_db::DiscountsContentProto>::GetInstance()
          ->GetForProfile(context),
#else
      nullptr,
#endif
      SessionProtoDBFactory<
          parcel_tracking_db::ParcelTrackingContent>::GetInstance()
          ->GetForProfile(context),
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      std::make_unique<commerce::WebExtractorImpl>(),
      TabRestoreServiceFactory::GetForProfile(profile));
}

bool ShoppingServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool ShoppingServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
}  // namespace commerce
