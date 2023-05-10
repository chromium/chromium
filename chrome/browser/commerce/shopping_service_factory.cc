// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/shopping_service_factory.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"
#include "chrome/browser/power_bookmarks/power_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/commerce/content/browser/commerce_tab_helper.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/proto/commerce_subscription_db_content.pb.h"
#include "components/commerce/core/shopping_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
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
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(BookmarkModelFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  DependsOn(PowerBookmarkServiceFactory::GetInstance());
  DependsOn(SessionProtoDBFactory<
            commerce_subscription_db::CommerceSubscriptionContentProto>::
                GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

KeyedService* ShoppingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new ShoppingService(
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
      PowerBookmarkServiceFactory::GetForBrowserContext(context));
}

bool ShoppingServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool ShoppingServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

KeyedService* ShoppingServiceFactory::SetTestingFactoryAndUse(
    content::BrowserContext* context,
    TestingFactory testing_factory) {
  KeyedService* mock_shopping_service =
      ProfileKeyedServiceFactory::SetTestingFactoryAndUse(
          context, std::move(testing_factory));
#if !BUILDFLAG(IS_ANDROID)
  Profile* profile = Profile::FromBrowserContext(context);
  Browser* browser = chrome::FindBrowserWithProfile(profile);
  for (int i = 0; i < browser->tab_strip_model()->GetTabCount(); i++) {
    CommerceTabHelper::FromWebContents(
        browser->tab_strip_model()->GetWebContentsAt(i))
        ->SetShoppingServiceForTesting(mock_shopping_service);  // IN-TEST
  }
#else
  // TODO(crbug.com/1356028): Update the ShoppingService in CommerceTabHelper.
  NOTIMPLEMENTED() << "No implementation for Android yet.";
#endif

  return mock_shopping_service;
}

}  // namespace commerce
