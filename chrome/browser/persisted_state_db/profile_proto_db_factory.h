// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERSISTED_STATE_DB_PROFILE_PROTO_DB_FACTORY_H_
#define CHROME_BROWSER_PERSISTED_STATE_DB_PROFILE_PROTO_DB_FACTORY_H_

#include "build/build_config.h"
#include "chrome/browser/persisted_state_db/profile_proto_db.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/cart/cart_db_content.pb.h"
#else
#include "chrome/browser/commerce/merchant_viewer/merchant_signal_db_content.pb.h"
#include "chrome/browser/commerce/subscriptions/commerce_subscription_db_content.pb.h"
#endif

namespace {
const char kPersistedStateDBFolder[] = "persisted_state_db";
const char kChromeCartDBFolder[] = "chrome_cart_db";
const char kMerchantTrustSignalDBFolder[] = "merchant_signal_db";
const char kCommerceSubscriptionDBFolder[] = "commerce_subscription_db";
}  // namespace

ProfileProtoDBFactory<persisted_state_db::PersistedStateContentProto>*
GetPersistedStateProfileProtoDBFactory();

#if !defined(OS_ANDROID)
ProfileProtoDBFactory<cart_db::ChromeCartContentProto>*
GetChromeCartProfileProtoDBFactory();
#else
ProfileProtoDBFactory<
    commerce_subscription_db::CommerceSubscriptionContentProto>*
GetCommerceSubscriptionProfileProtoDBFactory();
ProfileProtoDBFactory<merchant_signal_db::MerchantSignalContentProto>*
GetMerchantSignalProfileProtoDBFactory();
#endif

// Factory to create a ProtoDB per profile and per proto. Incognito is
// currently not supported and the factory will return nullptr for an incognito
// profile.
template <typename T>
class ProfileProtoDBFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Acquire instance of ProfileProtoDBFactory.
  static ProfileProtoDBFactory<T>* GetInstance();

  // Acquire ProtoDB - there is one per profile.
  static ProfileProtoDB<T>* GetForProfile(content::BrowserContext* context);

  // Call the parent Disassociate which is a protected method.
  void Disassociate(content::BrowserContext* context) {
    BrowserContextKeyedServiceFactory::Disassociate(context);
  }

 private:
  friend class base::NoDestructor<ProfileProtoDBFactory<T>>;

  ProfileProtoDBFactory();
  ~ProfileProtoDBFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

// static
template <typename T>
ProfileProtoDB<T>* ProfileProtoDBFactory<T>::GetForProfile(
    content::BrowserContext* context) {
  // Incognito is currently not supported
  if (context->IsOffTheRecord())
    return nullptr;

  return static_cast<ProfileProtoDB<T>*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

template <typename T>
ProfileProtoDBFactory<T>::ProfileProtoDBFactory()
    : BrowserContextKeyedServiceFactory(
          "ProfileProtoDBFactory",
          BrowserContextDependencyManager::GetInstance()) {}

template <typename T>
ProfileProtoDBFactory<T>::~ProfileProtoDBFactory() = default;

template <typename T>
KeyedService* ProfileProtoDBFactory<T>::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(!context->IsOffTheRecord());

  leveldb_proto::ProtoDatabaseProvider* proto_database_provider =
      content::BrowserContext::GetDefaultStoragePartition(context)
          ->GetProtoDatabaseProvider();

  // The following will become a proto -> dir and proto ->
  // leveldb_proto::ProtoDbType mapping as more protos are added.
  if (std::is_base_of<persisted_state_db::PersistedStateContentProto,
                      T>::value) {
    return new ProfileProtoDB<T>(
        context, proto_database_provider,
        context->GetPath().AppendASCII(kPersistedStateDBFolder),
        leveldb_proto::ProtoDbType::PERSISTED_STATE_DATABASE);
#if !defined(OS_ANDROID)
  } else if (std::is_base_of<cart_db::ChromeCartContentProto, T>::value) {
    return new ProfileProtoDB<T>(
        context, proto_database_provider,
        context->GetPath().AppendASCII(kChromeCartDBFolder),
        leveldb_proto::ProtoDbType::CART_DATABASE);
#else
  } else if (std::is_base_of<
                 commerce_subscription_db::CommerceSubscriptionContentProto,
                 T>::value) {
    return new ProfileProtoDB<T>(
        context, proto_database_provider,
        context->GetPath().AppendASCII(kCommerceSubscriptionDBFolder),
        leveldb_proto::ProtoDbType::COMMERCE_SUBSCRIPTION_DATABASE);
  } else if (std::is_base_of<merchant_signal_db::MerchantSignalContentProto,
                             T>::value) {
    return new ProfileProtoDB<T>(
        context, proto_database_provider,
        context->GetPath().AppendASCII(kMerchantTrustSignalDBFolder),
        leveldb_proto::ProtoDbType::MERCHANT_TRUST_SIGNAL_DATABASE);
#endif
  } else {
    // Must add in leveldb_proto::ProtoDbType and database directory folder for
    // new protos.
    DCHECK(false) << "Provided template is not supported. To support add "
                     "unique folder in the above proto -> folder name mapping. "
                     "This check could also fail because the template is not "
                     "supported on current platform.";
  }
}

#endif  // CHROME_BROWSER_PERSISTED_STATE_DB_PROFILE_PROTO_DB_FACTORY_H_
