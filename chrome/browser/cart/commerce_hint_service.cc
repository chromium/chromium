// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/commerce_hint_service.h"

#include <map>
#include <memory>

#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chrome/browser/cart/cart_db_content.pb.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace cart {

namespace {

const std::map<std::string, std::string>& GetDomainToTitle() {
  static const base::NoDestructor<std::map<std::string, std::string>> table({
      // TODO(crbug/1164236): add more known sites.
      {"amazon.com", "Amazon"},
      {"ebay.com", "eBay"},
      {"etsy.com", "Etsy"},
      {"amazon.co.uk", "Amazon"},
      {"walmart.com", "Walmart"},
      {"steampowered.com", "Steam"},
      {"target.com", "Target"},
      {"hm.com", "H&M"},
      {"homedepot.com", "Home Depot"},
      {"lowes.com", "Lowe's"},
      {"bestbuy.com", "Best Buy"},
  });
  return *table;
}

const std::map<std::string, std::string>& GetDomainToCart() {
  static const base::NoDestructor<std::map<std::string, std::string>> table({
      // TODO(crbug/1164236): add more known sites.
      {"walmart.com", "https://walmart.com/cart"},
      {"amazon.com", "https://www.amazon.com/gp/cart/view.html"},
      {"hm.com", "https://www2.hm.com/en_us/cart"},
      {"ebay.com", "https://cart.payments.ebay.com/"},
      {"etsy.com", "https://www.etsy.com/cart"},
      {"bestbuy.com", "https://www.bestbuy.com/cart"},
      {"homedepot.com", "https://www.homedepot.com/mycart/home"},
  });
  return *table;
}

// TODO(crbug/1164236): support multiple cart systems in the same domain.
std::string eTLDPlusOne(const GURL& url) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

// Implementation of the Mojo CommerceHintObserver. This is called by the
// renderer to notify the browser that a commerce hint happens.
class CommerceHintObserverImpl : public mojom::CommerceHintObserver {
 public:
  explicit CommerceHintObserverImpl(base::WeakPtr<CommerceHintService> service)
      : service_(service) {}

  ~CommerceHintObserverImpl() override = default;

  void OnAddToCart() override {
    VLOG(1) << "Received OnAddToCart in the browser process";
    service_->OnAddToCart(service_->WebContents()->GetLastCommittedURL());
  }

 private:
  base::WeakPtr<CommerceHintService> service_;
};

CommerceHintService::CommerceHintService(content::WebContents* web_contents)
    : web_contents_(web_contents) {
  service_ = CartServiceFactory::GetInstance()->GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  DCHECK(!web_contents->GetBrowserContext()->IsOffTheRecord());
}

CommerceHintService::~CommerceHintService() = default;

content::WebContents* CommerceHintService::WebContents() {
  return web_contents_;
}

void CommerceHintService::BindCommerceHintObserver(
    mojo::PendingReceiver<mojom::CommerceHintObserver> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<CommerceHintObserverImpl>(weak_factory_.GetWeakPtr()),
      std::move(receiver));
}

void CommerceHintService::OnAddToCart(const GURL& url) {
  service_->LoadCart(eTLDPlusOne(url),
                     base::BindOnce(&CommerceHintService::AddCartToDB,
                                    weak_factory_.GetWeakPtr(), url));
}

void CommerceHintService::AddCartToDB(
    const GURL& potential_cart_url,
    bool success,
    std::vector<CartDB::KeyAndValue> proto_pairs) {
  if (!success)
    return;
  cart_db::ChromeCartContentProto proto;
  // If there is an existing cart from that domain, update timestamp; otherwise,
  // construct a new entry.
  if (proto_pairs.size() > 0) {
    DCHECK(proto_pairs.size() == 1);
    proto = std::move(proto_pairs.at(0).second);
    proto.set_timestamp(base::Time::Now().ToDoubleT());
  } else {
    ConstructCartProto(&proto, potential_cart_url);
  }
  service_->AddCart(proto.key(), std::move(proto));
}

void CommerceHintService::ConstructCartProto(
    cart_db::ChromeCartContentProto* proto,
    const GURL& potential_cart_url) {
  const std::string& domain = eTLDPlusOne(potential_cart_url);

  std::string title;
  const std::map<std::string, std::string>& domain_to_title =
      GetDomainToTitle();
  if (domain_to_title.count(domain) > 0) {
    title = domain_to_title.at(domain);
  } else {
    title = domain;
  }

  std::string cart_url;
  const std::map<std::string, std::string>& domain_to_cart = GetDomainToCart();
  if (domain_to_cart.count(domain) > 0) {
    cart_url = domain_to_cart.at(domain);
  } else {
    cart_url = potential_cart_url.spec();
  }

  proto->set_key(std::move(domain));
  proto->set_merchant(std::move(title));
  proto->set_merchant_cart_url(std::move(cart_url));
  proto->set_timestamp(base::Time::Now().ToDoubleT());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CommerceHintService)

}  // namespace cart
