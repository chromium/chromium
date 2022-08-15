// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/shopping_service_factory.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/commerce/core/proto/commerce_subscription_db_content.pb.h"
#include "components/commerce/core/shopping_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"

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
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(BookmarkModelFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SessionProtoDBFactory<
            commerce_subscription_db::CommerceSubscriptionContentProto>::
                GetInstance());
}

KeyedService* ShoppingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new ShoppingService(
      BookmarkModelFactory::GetInstance()->GetForBrowserContext(context),
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
      profile->GetPrefs(), IdentityManagerFactory::GetForProfile(profile),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      SessionProtoDBFactory<commerce_subscription_db::
                                CommerceSubscriptionContentProto>::GetInstance()
          ->GetForProfile(context));
}

bool ShoppingServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool ShoppingServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace commerce
