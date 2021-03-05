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
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace cart {

namespace {

// TODO(crbug/1164236): support multiple cart systems in the same domain.
// Returns eTLB+1 domain.
std::string GetDomain(const GURL& url) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

// Implementation of the Mojo CommerceHintObserver. This is called by the
// renderer to notify the browser that a commerce hint happens.
class CommerceHintObserverImpl : public mojom::CommerceHintObserver {
 public:
  explicit CommerceHintObserverImpl(base::WeakPtr<CommerceHintService> service)
      : service_(std::move(service)) {}

  ~CommerceHintObserverImpl() override = default;

  void OnAddToCart(const base::Optional<GURL>& cart_url) override {
    DVLOG(1) << "Received OnAddToCart in the browser process";
    if (!service_)
      return;
    service_->OnAddToCart(service_->WebContents()->GetLastCommittedURL(),
                          cart_url);
  }

  void OnVisitCart() override {
    DVLOG(1) << "Received OnVisitCart in the browser process";
    if (!service_)
      return;
    const GURL& main_frame_url = service_->WebContents()->GetLastCommittedURL();
    service_->OnAddToCart(main_frame_url, main_frame_url);
  }

  void OnVisitCheckout() override {
    DVLOG(1) << "Received OnVisitCheckout in the browser process";
    if (!service_)
      return;
    service_->OnRemoveCart(service_->WebContents()->GetLastCommittedURL());
  }

  void OnPurchase() override {
    DVLOG(1) << "Received OnPurchase in the browser process";
    if (!service_)
      return;
    service_->OnRemoveCart(service_->WebContents()->GetLastCommittedURL());
  }

 private:
  base::WeakPtr<CommerceHintService> service_;
};

CommerceHintService::CommerceHintService(content::WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(!web_contents->GetBrowserContext()->IsOffTheRecord());
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  service_ = CartServiceFactory::GetInstance()->GetForProfile(profile);
  optimization_guide_decider_ =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (optimization_guide_decider_) {
    optimization_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::SHOPPING_PAGE_PREDICTOR});
  }
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

bool CommerceHintService::ShouldSkip(const GURL& url) {
  if (!optimization_guide_decider_) {
    return false;
  }

  optimization_guide::OptimizationMetadata metadata;
  auto decision = optimization_guide_decider_->CanApplyOptimization(
      url, optimization_guide::proto::SHOPPING_PAGE_PREDICTOR, &metadata);
  DVLOG(1) << "SHOPPING_PAGE_PREDICTOR = " << static_cast<int>(decision);
  return optimization_guide::OptimizationGuideDecision::kFalse == decision;
}

void CommerceHintService::OnAddToCart(const GURL& navigation_url,
                                      const base::Optional<GURL>& cart_url) {
  if (ShouldSkip(navigation_url))
    return;
  base::Optional<GURL> validated_cart = cart_url;
  if (cart_url && GetDomain(*cart_url) != GetDomain(navigation_url)) {
    DVLOG(1) << "Reject cart URL with different eTLD+1 domain.";
    validated_cart = base::nullopt;
  }
  cart_db::ChromeCartContentProto proto;
  ConstructCartProto(&proto, navigation_url);
  service_->AddCart(GetDomain(navigation_url), validated_cart,
                    std::move(proto));
}

void CommerceHintService::OnRemoveCart(const GURL& url) {
  service_->DeleteCart(GetDomain(url));
}

void CommerceHintService::ConstructCartProto(
    cart_db::ChromeCartContentProto* proto,
    const GURL& navigation_url) {
  const std::string& domain = GetDomain(navigation_url);
  proto->set_key(domain);
  proto->set_merchant(domain);
  proto->set_merchant_cart_url(navigation_url.spec());
  proto->set_timestamp(base::Time::Now().ToDoubleT());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CommerceHintService)

}  // namespace cart
