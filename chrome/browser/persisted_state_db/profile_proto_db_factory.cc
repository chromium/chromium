// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/persisted_state_db/profile_proto_db_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/persisted_state_db/persisted_state_db_content.pb.h"

ProfileProtoDBFactory<persisted_state_db::PersistedStateContentProto>*
GetPersistedStateProfileProtoDBFactory() {
  static base::NoDestructor<
      ProfileProtoDBFactory<persisted_state_db::PersistedStateContentProto>>
      instance;
  return instance.get();
}

template <>
ProfileProtoDBFactory<persisted_state_db::PersistedStateContentProto>*
ProfileProtoDBFactory<
    persisted_state_db::PersistedStateContentProto>::GetInstance() {
  return GetPersistedStateProfileProtoDBFactory();
}

#if !defined(OS_ANDROID)
ProfileProtoDBFactory<cart_db::ChromeCartContentProto>*
GetChromeCartProfileProtoDBFactory() {
  static base::NoDestructor<
      ProfileProtoDBFactory<cart_db::ChromeCartContentProto>>
      instance;
  return instance.get();
}

template <>
ProfileProtoDBFactory<cart_db::ChromeCartContentProto>*
ProfileProtoDBFactory<cart_db::ChromeCartContentProto>::GetInstance() {
  return GetChromeCartProfileProtoDBFactory();
}

#else
ProfileProtoDBFactory<
    commerce_subscription_db::CommerceSubscriptionContentProto>*
GetCommerceSubscriptionProfileProtoDBFactory() {
  static base::NoDestructor<ProfileProtoDBFactory<
      commerce_subscription_db::CommerceSubscriptionContentProto>>
      instance;
  return instance.get();
}

template <>
ProfileProtoDBFactory<
    commerce_subscription_db::CommerceSubscriptionContentProto>*
ProfileProtoDBFactory<
    commerce_subscription_db::CommerceSubscriptionContentProto>::GetInstance() {
  return GetCommerceSubscriptionProfileProtoDBFactory();
}

ProfileProtoDBFactory<merchant_signal_db::MerchantSignalContentProto>*
GetMerchantSignalProfileProtoDBFactory() {
  static base::NoDestructor<
      ProfileProtoDBFactory<merchant_signal_db::MerchantSignalContentProto>>
      instance;
  return instance.get();
}

template <>
ProfileProtoDBFactory<merchant_signal_db::MerchantSignalContentProto>*
ProfileProtoDBFactory<
    merchant_signal_db::MerchantSignalContentProto>::GetInstance() {
  return GetMerchantSignalProfileProtoDBFactory();
}
#endif
