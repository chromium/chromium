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

  void OnVisitCart() override {
    VLOG(1) << "Received OnVisitCart in the browser process";
    service_->OnAddToCart(service_->WebContents()->GetLastCommittedURL());
  }

  void OnVisitCheckout() override {
    VLOG(1) << "Received OnVisitCheckout in the browser process";
    service_->OnRemoveCart(service_->WebContents()->GetLastCommittedURL());
  }

  void OnPurchase() override {
    VLOG(1) << "Received OnPurchase in the browser process";
    service_->OnRemoveCart(service_->WebContents()->GetLastCommittedURL());
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

void CommerceHintService::OnRemoveCart(const GURL& url) {
  service_->DeleteCart(eTLDPlusOne(url));
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
  proto->set_key(domain);
  proto->set_merchant(domain);
  proto->set_merchant_cart_url(potential_cart_url.spec());
  proto->set_timestamp(base::Time::Now().ToDoubleT());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CommerceHintService)

}  // namespace cart
