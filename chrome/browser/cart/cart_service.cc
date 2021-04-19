// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_service.h"
#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/cart/cart_db_content.pb.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {
constexpr char kFakeDataPrefix[] = "Fake:";

std::string eTLDPlusOne(const GURL& url) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

std::string GetKeyForURL(const GURL& url) {
  std::string domain = eTLDPlusOne(url);
  return base::GetFieldTrialParamValueByFeature(
             ntp_features::kNtpChromeCartModule,
             ntp_features::kNtpChromeCartModuleDataParam) == "fake"
             ? std::string(kFakeDataPrefix) + domain
             : domain;
}

bool CompareTimeStampForProtoPair(const CartDB::KeyAndValue pair1,
                                  CartDB::KeyAndValue pair2) {
  return pair1.second.timestamp() > pair2.second.timestamp();
}

base::Optional<base::Value> JSONToDictionary(int resource_id) {
  base::StringPiece json_resource(
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id));
  base::Optional<base::Value> value = base::JSONReader::Read(json_resource);
  DCHECK(value && value.has_value() && value->is_dict());
  return value;
}

bool IsExpired(const cart_db::ChromeCartContentProto& proto) {
  return (base::Time::Now() - base::Time::FromDoubleT(proto.timestamp()))
             .InDays() > 14;
}
}  // namespace

CartService::CartService(Profile* profile)
    : profile_(profile),
      cart_db_(std::make_unique<CartDB>(profile_)),
      history_service_(HistoryServiceFactory::GetForProfile(
          profile_,
          ServiceAccessType::EXPLICIT_ACCESS)),
      domain_name_mapping_(JSONToDictionary(IDR_CART_DOMAIN_NAME_MAPPING_JSON)),
      domain_cart_url_mapping_(
          JSONToDictionary(IDR_CART_DOMAIN_CART_URL_MAPPING_JSON)) {
  if (history_service_) {
    history_service_observation_.Observe(history_service_);
  }
  if (base::GetFieldTrialParamValueByFeature(
          ntp_features::kNtpChromeCartModule,
          ntp_features::kNtpChromeCartModuleDataParam) == "fake") {
    AddCartsWithFakeData();
  } else {
    // In case last deconstruction is interrupted and fake data is not deleted.
    DeleteCartsWithFakeData();
  }
}

CartService::~CartService() = default;

void CartService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kCartModuleHidden, false);
  registry->RegisterIntegerPref(prefs::kCartModuleWelcomeSurfaceShownTimes, 0);
  registry->RegisterBooleanPref(prefs::kCartDiscountAcknowledged, false);
  registry->RegisterBooleanPref(prefs::kCartDiscountEnabled, false);
}

void CartService::Hide() {
  profile_->GetPrefs()->SetBoolean(prefs::kCartModuleHidden, true);
}

void CartService::RestoreHidden() {
  profile_->GetPrefs()->SetBoolean(prefs::kCartModuleHidden, false);
}

bool CartService::IsHidden() {
  return profile_->GetPrefs()->GetBoolean(prefs::kCartModuleHidden);
}

void CartService::LoadCart(const std::string& domain,
                           CartDB::LoadCallback callback) {
  cart_db_->LoadCart(domain, std::move(callback));
}

void CartService::LoadAllActiveCarts(CartDB::LoadCallback callback) {
  cart_db_->LoadAllCarts(base::BindOnce(&CartService::OnLoadCarts,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        std::move(callback)));
}

void CartService::AddCart(const std::string& domain,
                          const base::Optional<GURL>& cart_url,
                          const cart_db::ChromeCartContentProto& proto) {
  cart_db_->LoadCart(domain, base::BindOnce(&CartService::OnAddCart,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            domain, cart_url, proto));
}

void CartService::DeleteCart(const std::string& domain) {
  cart_db_->DeleteCart(domain, base::BindOnce(&CartService::OnOperationFinished,
                                              weak_ptr_factory_.GetWeakPtr()));
}

void CartService::HideCart(const GURL& cart_url,
                           CartDB::OperationCallback callback) {
  cart_db_->LoadCart(GetKeyForURL(cart_url),
                     base::BindOnce(&CartService::SetCartHiddenStatus,
                                    weak_ptr_factory_.GetWeakPtr(), true,
                                    std::move(callback)));
}

void CartService::RestoreHiddenCart(const GURL& cart_url,
                                    CartDB::OperationCallback callback) {
  cart_db_->LoadCart(GetKeyForURL(cart_url),
                     base::BindOnce(&CartService::SetCartHiddenStatus,
                                    weak_ptr_factory_.GetWeakPtr(), false,
                                    std::move(callback)));
}

void CartService::RemoveCart(const GURL& cart_url,
                             CartDB::OperationCallback callback) {
  cart_db_->LoadCart(GetKeyForURL(cart_url),
                     base::BindOnce(&CartService::SetCartRemovedStatus,
                                    weak_ptr_factory_.GetWeakPtr(), true,
                                    std::move(callback)));
}

void CartService::RestoreRemovedCart(const GURL& cart_url,
                                     CartDB::OperationCallback callback) {
  cart_db_->LoadCart(GetKeyForURL(cart_url),
                     base::BindOnce(&CartService::SetCartRemovedStatus,
                                    weak_ptr_factory_.GetWeakPtr(), false,
                                    std::move(callback)));
}

void CartService::IncreaseWelcomeSurfaceCounter() {
  if (!ShouldShowWelcomeSurface())
    return;
  int times = profile_->GetPrefs()->GetInteger(
      prefs::kCartModuleWelcomeSurfaceShownTimes);
  profile_->GetPrefs()->SetInteger(prefs::kCartModuleWelcomeSurfaceShownTimes,
                                   times + 1);
}

bool CartService::ShouldShowWelcomeSurface() {
  return profile_->GetPrefs()->GetInteger(
             prefs::kCartModuleWelcomeSurfaceShownTimes) <
         kWelcomSurfaceShowLimit;
}

void CartService::AcknowledgeDiscountConsent(bool should_enable) {
  if (base::GetFieldTrialParamValueByFeature(
          ntp_features::kNtpChromeCartModule,
          ntp_features::kNtpChromeCartModuleDataParam) == "fake") {
    return;
  }
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountAcknowledged, true);
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, should_enable);
}

bool CartService::ShouldShowDiscountConsent() {
  if (ShouldShowWelcomeSurface() ||
      base::GetFieldTrialParamValueByFeature(
          ntp_features::kNtpChromeCartModule,
          ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam) !=
          "true") {
    return false;
  }
  if (base::GetFieldTrialParamValueByFeature(
          ntp_features::kNtpChromeCartModule,
          ntp_features::kNtpChromeCartModuleDataParam) == "fake") {
    return true;
  }
  return !profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountAcknowledged);
}

bool CartService::IsCartDiscountEnabled() {
  if (base::GetFieldTrialParamValueByFeature(
          ntp_features::kNtpChromeCartModule,
          ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam) !=
      "true") {
    return false;
  }
  return profile_->GetPrefs()->GetBoolean(prefs::kCartDiscountEnabled);
}

void CartService::SetCartDiscountEnabled(bool enabled) {
  DCHECK(base::GetFieldTrialParamValueByFeature(
             ntp_features::kNtpChromeCartModule,
             ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam) ==
         "true");
  profile_->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, enabled);
}

void CartService::LoadCartsWithFakeData(CartDB::LoadCallback callback) {
  cart_db_->LoadCartsWithPrefix(
      kFakeDataPrefix,
      base::BindOnce(&CartService::OnLoadCarts, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void CartService::OnOperationFinished(bool success) {
  DCHECK(success) << "database operation failed.";
}

void CartService::OnOperationFinishedWithCallback(
    CartDB::OperationCallback callback,
    bool success) {
  DCHECK(success) << "database operation failed.";
  std::move(callback).Run(success);
}

void CartService::Shutdown() {
  if (history_service_) {
    history_service_observation_.Reset();
  }
  DeleteCartsWithFakeData();
  // Delete content of all carts that are removed.
  cart_db_->LoadAllCarts(base::BindOnce(&CartService::DeleteRemovedCartsContent,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void CartService::OnURLsDeleted(history::HistoryService* history_service,
                                const history::DeletionInfo& deletion_info) {
  // TODO(crbug.com/1157892): Add more fine-grained deletion of cart data when
  // history deletion happens.
  cart_db_->DeleteAllCarts(base::BindOnce(&CartService::OnOperationFinished,
                                          weak_ptr_factory_.GetWeakPtr()));
}

CartDB* CartService::GetDB() {
  return cart_db_.get();
}

void CartService::AddCartsWithFakeData() {
  DeleteCartsWithFakeData();
  // Polulate and add some carts with fake data.
  double time_now = base::Time::Now().ToDoubleT();
  cart_db::ChromeCartContentProto dummy_proto1;
  GURL dummy_url1 = GURL("https://www.google.com/");
  dummy_proto1.set_key(std::string(kFakeDataPrefix) + eTLDPlusOne(dummy_url1));
  dummy_proto1.set_merchant("Cart Foo");
  dummy_proto1.set_merchant_cart_url(dummy_url1.spec());
  dummy_proto1.set_timestamp(time_now + 6);
  dummy_proto1.mutable_discount_info()->set_discount_text(
      l10n_util::GetStringFUTF8(IDS_NTP_MODULES_CART_DISCOUNT_CHIP_AMOUNT,
                                u"15%"));
  dummy_proto1.add_product_image_urls(
      "https://encrypted-tbn3.gstatic.com/"
      "shopping?q=tbn:ANd9GcQpn38jB2_BANnHUFa7kHJsf6SyubcgeU1lNYO_"
      "ZxM1Q2ju_ZMjv2EwNh0Zx_zbqYy_mFg_aiIhWYnD5PQ7t-uFzLM5cN77s_2_"
      "DFNeumI-LMPJMYjW-BOSaA&usqp=CAY");
  dummy_proto1.add_product_image_urls(
      "https://encrypted-tbn0.gstatic.com/"
      "shopping?q=tbn:ANd9GcQyMRYWeM2Yq095nOXTL0-"
      "EUUnm79kh6hnw8yctJUNrAuse607KEr1CVxEa24r-"
      "8XHBuhTwcuC4GXeN94h9Kn19DhdBGsXG0qrD74veYSDJNLrUP-sru0jH&usqp=CAY");
  dummy_proto1.add_product_image_urls(
      "https://encrypted-tbn1.gstatic.com/"
      "shopping?q=tbn:ANd9GcT2ew6Aydzu5VzRV756ORGha6fyjKp_On7iTlr_"
      "tL9vODnlNtFo_xsxj6_lCop-3J0Vk44lHfk-AxoBJDABVHPVFN-"
      "EiWLcZvzkdpHFqcurm7fBVmWtYKo2rg&usqp=CAY");
  cart_db_->AddCart(dummy_proto1.key(), dummy_proto1,
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));

  cart_db::ChromeCartContentProto dummy_proto2;
  GURL dummy_url2 = GURL("https://www.amazon.com/");
  dummy_proto2.set_key(std::string(kFakeDataPrefix) + eTLDPlusOne(dummy_url2));
  dummy_proto2.set_merchant("Cart Bar");
  dummy_proto2.set_merchant_cart_url(dummy_url2.spec());
  dummy_proto2.set_timestamp(time_now + 5);
  cart_db_->AddCart(dummy_proto2.key(), dummy_proto2,
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));

  cart_db::ChromeCartContentProto dummy_proto3;
  GURL dummy_url3 = GURL("https://www.ebay.com/");
  dummy_proto3.set_key(std::string(kFakeDataPrefix) + eTLDPlusOne(dummy_url3));
  dummy_proto3.set_merchant("Cart Baz");
  dummy_proto3.set_merchant_cart_url(dummy_url3.spec());
  dummy_proto3.set_timestamp(time_now + 4);
  dummy_proto3.mutable_discount_info()->set_discount_text(
      l10n_util::GetStringFUTF8(IDS_NTP_MODULES_CART_DISCOUNT_CHIP_UP_TO_AMOUNT,
                                u"$50"));
  cart_db_->AddCart(dummy_proto3.key(), dummy_proto3,
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));

  cart_db::ChromeCartContentProto dummy_proto4;
  GURL dummy_url4 = GURL("https://www.walmart.com/");
  dummy_proto4.set_key(std::string(kFakeDataPrefix) + eTLDPlusOne(dummy_url4));
  dummy_proto4.set_merchant("Cart Qux");
  dummy_proto4.set_merchant_cart_url(dummy_url4.spec());
  dummy_proto4.set_timestamp(time_now + 3);
  cart_db_->AddCart(dummy_proto4.key(), dummy_proto4,
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));

  cart_db::ChromeCartContentProto dummy_proto5;
  GURL dummy_url5 = GURL("https://www.bestbuy.com/");
  dummy_proto5.set_key(std::string(kFakeDataPrefix) + eTLDPlusOne(dummy_url5));
  dummy_proto5.set_merchant("Cart Corge");
  dummy_proto5.set_merchant_cart_url(dummy_url5.spec());
  dummy_proto5.set_timestamp(time_now + 2);
  cart_db_->AddCart(dummy_proto5.key(), dummy_proto5,
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));

  cart_db::ChromeCartContentProto dummy_proto6;
  GURL dummy_url6 = GURL("https://www.nike.com/");
  dummy_proto6.set_key(std::string(kFakeDataPrefix) + eTLDPlusOne(dummy_url6));
  dummy_proto6.set_merchant("Cart Flob");
  dummy_proto6.set_merchant_cart_url(dummy_url6.spec());
  dummy_proto6.set_timestamp(time_now + 1);
  cart_db_->AddCart(dummy_proto6.key(), dummy_proto6,
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void CartService::DeleteCartsWithFakeData() {
  cart_db_->DeleteCartsWithPrefix(
      kFakeDataPrefix, base::BindOnce(&CartService::OnOperationFinished,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void CartService::DeleteRemovedCartsContent(
    bool success,
    std::vector<CartDB::KeyAndValue> proto_pairs) {
  for (CartDB::KeyAndValue proto_pair : proto_pairs) {
    if (proto_pair.second.is_removed()) {
      // Delete removed cart content by overwriting it with an entry with only
      // removed status data.
      cart_db::ChromeCartContentProto empty_proto;
      empty_proto.set_key(proto_pair.first);
      empty_proto.set_is_removed(true);
      cart_db_->AddCart(proto_pair.first, empty_proto,
                        base::BindOnce(&CartService::OnOperationFinished,
                                       weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void CartService::OnLoadCarts(CartDB::LoadCallback callback,
                              bool success,
                              std::vector<CartDB::KeyAndValue> proto_pairs) {
  if (IsHidden() &&
      base::GetFieldTrialParamValueByFeature(
          ntp_features::kNtpChromeCartModule,
          ntp_features::kNtpChromeCartModuleDataParam) != "fake") {
    std::move(callback).Run(success, {});
    return;
  }
  std::set<std::string> expired_merchants;
  for (CartDB::KeyAndValue kv : proto_pairs) {
    if (IsExpired(kv.second)) {
      DeleteCart(kv.second.key());
      expired_merchants.emplace(kv.second.key());
    }
  }
  proto_pairs.erase(
      std::remove_if(proto_pairs.begin(), proto_pairs.end(),
                     [expired_merchants](CartDB::KeyAndValue kv) {
                       return kv.second.is_hidden() || kv.second.is_removed() ||
                              expired_merchants.find(kv.second.key()) !=
                                  expired_merchants.end();
                     }),
      proto_pairs.end());
  // Sort items in timestamp descending order.
  std::sort(proto_pairs.begin(), proto_pairs.end(),
            CompareTimeStampForProtoPair);
  std::move(callback).Run(success, std::move(proto_pairs));
}

void CartService::SetCartHiddenStatus(
    bool isHidden,
    CartDB::OperationCallback callback,
    bool success,
    std::vector<CartDB::KeyAndValue> proto_pairs) {
  if (!success) {
    return;
  }
  DCHECK_EQ(1U, proto_pairs.size());
  CartDB::KeyAndValue proto_pair = proto_pairs[0];
  proto_pair.second.set_is_hidden(isHidden);
  cart_db_->AddCart(
      proto_pair.first, proto_pair.second,
      base::BindOnce(&CartService::OnOperationFinishedWithCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CartService::SetCartRemovedStatus(
    bool isRemoved,
    CartDB::OperationCallback callback,
    bool success,
    std::vector<CartDB::KeyAndValue> proto_pairs) {
  if (!success) {
    return;
  }
  DCHECK_EQ(1U, proto_pairs.size());
  CartDB::KeyAndValue proto_pair = proto_pairs[0];
  proto_pair.second.set_is_removed(isRemoved);
  cart_db_->AddCart(
      proto_pair.first, proto_pair.second,
      base::BindOnce(&CartService::OnOperationFinishedWithCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CartService::OnAddCart(const std::string& domain,
                            const base::Optional<GURL>& cart_url,
                            cart_db::ChromeCartContentProto proto,
                            bool success,
                            std::vector<CartDB::KeyAndValue> proto_pairs) {
  if (!success) {
    return;
  }
  // Restore module visibility anytime a cart-related action happens.
  RestoreHidden();
  std::string* merchant_name = domain_name_mapping_->FindStringKey(domain);
  if (merchant_name) {
    proto.set_merchant(*merchant_name);
  }
  if (cart_url) {
    proto.set_merchant_cart_url(cart_url->spec());
  } else {
    std::string* fallback_url = domain_cart_url_mapping_->FindStringKey(domain);
    if (fallback_url) {
      proto.set_merchant_cart_url(*fallback_url);
    }
  }
  if (proto_pairs.size() == 0) {
    cart_db_->AddCart(domain, std::move(proto),
                      base::BindOnce(&CartService::OnOperationFinished,
                                     weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  DCHECK_EQ(1U, proto_pairs.size());
  cart_db::ChromeCartContentProto existing_proto = proto_pairs[0].second;
  // Do nothing for carts that has been explicitly removed.
  if (existing_proto.is_removed()) {
    return;
  }
  // If the new proto has no product images, keep the existing proto while
  // update timestamp and hidden status; otherwise add the new proto.
  if (proto.product_image_urls().size() == 0) {
    existing_proto.set_is_hidden(false);
    existing_proto.set_timestamp(proto.timestamp());
    if (cart_url) {
      existing_proto.set_merchant_cart_url(cart_url->spec());
    }
    cart_db_->AddCart(domain, std::move(existing_proto),
                      base::BindOnce(&CartService::OnOperationFinished,
                                     weak_ptr_factory_.GetWeakPtr()));
  } else {
    cart_db_->AddCart(domain, std::move(proto),
                      base::BindOnce(&CartService::OnOperationFinished,
                                     weak_ptr_factory_.GetWeakPtr()));
  }
}
