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
    if (!service_)
      return;
    service_->OnAddToCart(service_->WebContents()->GetLastCommittedURL(),
                          base::nullopt);
  }

  void OnVisitCart() override {
    VLOG(1) << "Received OnVisitCart in the browser process";
    if (!service_)
      return;
    const GURL& main_frame_url = service_->WebContents()->GetLastCommittedURL();
    service_->OnAddToCart(main_frame_url, main_frame_url);
  }

  void OnVisitCheckout() override {
    VLOG(1) << "Received OnVisitCheckout in the browser process";
    if (!service_)
      return;
    service_->OnRemoveCart(service_->WebContents()->GetLastCommittedURL());
  }

  void OnPurchase() override {
    VLOG(1) << "Received OnPurchase in the browser process";
    if (!service_)
      return;
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

void CommerceHintService::OnAddToCart(const GURL& navigation_url,
                                      const base::Optional<GURL>& cart_url) {
  cart_db::ChromeCartContentProto proto;
  ConstructCartProto(&proto, navigation_url);
  service_->AddCart(eTLDPlusOne(navigation_url), cart_url, std::move(proto));
}

void CommerceHintService::OnRemoveCart(const GURL& url) {
  service_->DeleteCart(eTLDPlusOne(url));
}

void CommerceHintService::ConstructCartProto(
    cart_db::ChromeCartContentProto* proto,
    const GURL& navigation_url) {
  const std::string& domain = eTLDPlusOne(navigation_url);
  proto->set_key(domain);
  proto->set_merchant(domain);
  proto->set_merchant_cart_url(navigation_url.spec());
  proto->set_timestamp(base::Time::Now().ToDoubleT());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CommerceHintService)

}  // namespace cart
