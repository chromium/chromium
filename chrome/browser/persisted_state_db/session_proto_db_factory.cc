// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/commerce/core/proto/persisted_state_db_content.pb.h"

SessionProtoDBFactory<persisted_state_db::PersistedStateContentProto>*
GetPersistedStateSessionProtoDBFactory() {
  static base::NoDestructor<
      SessionProtoDBFactory<persisted_state_db::PersistedStateContentProto>>
      instance;
  return instance.get();
}

template <>
SessionProtoDBFactory<persisted_state_db::PersistedStateContentProto>*
SessionProtoDBFactory<
    persisted_state_db::PersistedStateContentProto>::GetInstance() {
  return GetPersistedStateSessionProtoDBFactory();
}

SessionProtoDBFactory<
    commerce_subscription_db::CommerceSubscriptionContentProto>*
GetCommerceSubscriptionSessionProtoDBFactory() {
  static base::NoDestructor<SessionProtoDBFactory<
      commerce_subscription_db::CommerceSubscriptionContentProto>>
      instance;
  return instance.get();
}

template <>
SessionProtoDBFactory<
    commerce_subscription_db::CommerceSubscriptionContentProto>*
SessionProtoDBFactory<
    commerce_subscription_db::CommerceSubscriptionContentProto>::GetInstance() {
  return GetCommerceSubscriptionSessionProtoDBFactory();
}

SessionProtoDBFactory<parcel_tracking_db::ParcelTrackingContent>*
GetParcelTrackingSessionProtoDBFactory() {
  static base::NoDestructor<
      SessionProtoDBFactory<parcel_tracking_db::ParcelTrackingContent>>
      instance;
  return instance.get();
}

template <>
SessionProtoDBFactory<parcel_tracking_db::ParcelTrackingContent>*
SessionProtoDBFactory<
    parcel_tracking_db::ParcelTrackingContent>::GetInstance() {
  return GetParcelTrackingSessionProtoDBFactory();
}

#if !BUILDFLAG(IS_ANDROID)
SessionProtoDBFactory<cart_db::ChromeCartContentProto>*
GetChromeCartSessionProtoDBFactory() {
  static base::NoDestructor<
      SessionProtoDBFactory<cart_db::ChromeCartContentProto>>
      instance;
  return instance.get();
}

template <>
SessionProtoDBFactory<cart_db::ChromeCartContentProto>*
SessionProtoDBFactory<cart_db::ChromeCartContentProto>::GetInstance() {
  return GetChromeCartSessionProtoDBFactory();
}

SessionProtoDBFactory<coupon_db::CouponContentProto>*
GetCouponSessionProtoDBFactory() {
  static base::NoDestructor<
      SessionProtoDBFactory<coupon_db::CouponContentProto>>
      instance;
  return instance.get();
}

template <>
SessionProtoDBFactory<coupon_db::CouponContentProto>*
SessionProtoDBFactory<coupon_db::CouponContentProto>::GetInstance() {
  return GetCouponSessionProtoDBFactory();
}

SessionProtoDBFactory<discounts_db::DiscountsContentProto>*
GetDiscountsSessionProtoDBFactory() {
  static base::NoDestructor<
      SessionProtoDBFactory<discounts_db::DiscountsContentProto>>
      instance;
  return instance.get();
}

template <>
SessionProtoDBFactory<discounts_db::DiscountsContentProto>*
SessionProtoDBFactory<discounts_db::DiscountsContentProto>::GetInstance() {
  return GetDiscountsSessionProtoDBFactory();
}

#else
SessionProtoDBFactory<merchant_signal_db::MerchantSignalContentProto>*
GetMerchantSignalSessionProtoDBFactory() {
  static base::NoDestructor<
      SessionProtoDBFactory<merchant_signal_db::MerchantSignalContentProto>>
      instance;
  return instance.get();
}

template <>
SessionProtoDBFactory<merchant_signal_db::MerchantSignalContentProto>*
SessionProtoDBFactory<
    merchant_signal_db::MerchantSignalContentProto>::GetInstance() {
  return GetMerchantSignalSessionProtoDBFactory();
}
#endif
