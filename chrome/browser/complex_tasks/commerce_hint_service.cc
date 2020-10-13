// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/complex_tasks/commerce_hint_service.h"

#include <map>
#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

using bookmarks::BookmarkNode;

namespace complex_tasks {

namespace {

const std::map<std::string, std::string>& GetDomainToTitle() {
  static const base::NoDestructor<std::map<std::string, std::string>> table({
      // TODO: add more
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
  });
  return *table;
}

const std::map<std::string, std::string>& GetDomainToCart() {
  static const base::NoDestructor<std::map<std::string, std::string>> table({
      // TODO: add more
      {"walmart.com", "https://walmart.com/cart"},
  });
  return *table;
}

// TODO: support multiple cart systems in the same domain
std::string eTLDPlusOne(const GURL& url) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

const BookmarkNode* GetMatchingCart(bookmarks::BookmarkModel* model,
                                    const GURL& url) {
  std::string domain = eTLDPlusOne(url);
  VLOG(1) << "etld_plus_1 = " << domain;
  std::vector<const BookmarkNode*> nodes;
  model->GetChromeCartNodes(nodes);
  for (const auto* node : nodes) {
    std::string is_cart;
    node->GetMetaInfo("is_cart", &is_cart);
    DCHECK_EQ(is_cart, "true");

    if (domain == eTLDPlusOne(node->url())) {
      return node;
    }
  }

  const BookmarkNode* chrome_cart_node = model->chrome_cart_node();
  BookmarkNode::MetaInfoMap meta_info;
  meta_info["is_cart"] = "true";
  std::string title = domain + " Shopping Cart";
  if (GetDomainToTitle().count(domain) > 0) {
    title = GetDomainToTitle().at(domain);
  }
  GURL cart_url = url;
  if (GetDomainToCart().count(domain) > 0) {
    cart_url = GURL(GetDomainToCart().at(domain));
  }

  return model->AddURL(chrome_cart_node, 0, base::ASCIIToUTF16(title), cart_url,
                       &meta_info);
}

void RemoveMatchingCart(bookmarks::BookmarkModel* model, const GURL& url) {
  std::string domain = eTLDPlusOne(url);
  VLOG(1) << "etld_plus_1 = " << domain;
  std::vector<const BookmarkNode*> nodes;
  model->GetChromeCartNodes(nodes);
  for (const auto* node : nodes) {
    std::string is_cart;
    node->GetMetaInfo("is_cart", &is_cart);
    DCHECK_EQ(is_cart, "true");

    if (domain == eTLDPlusOne(node->url())) {
      model->Remove(node);
      return;
    }
  }
}

}  // namespace

// Implementation of the Mojo CommerceHintObserver. This is called by the
// renderer to notify the browser that a commerce hint happens.
class CommerceHintObserverImpl : public mojom::CommerceHintObserver {
 public:
  explicit CommerceHintObserverImpl(base::WeakPtr<CommerceHintService> service)
      : service_(service), model_(nullptr) {}

  ~CommerceHintObserverImpl() override = default;

  bookmarks::BookmarkModel* model() {
    if (model_)
      return model_;

    auto* profile = Profile::FromBrowserContext(
        service_->WebContents()->GetBrowserContext());
    model_ = BookmarkModelFactory::GetForBrowserContext(profile);
    DCHECK(model_->loaded());
    return model_;
  }

  const BookmarkNode* GetMatchingCart() {
    const GURL current_url = service_->WebContents()->GetLastCommittedURL();
    return ::complex_tasks::GetMatchingCart(model(), current_url);
  }

  void RemoveMatchingCart() {
    const GURL current_url = service_->WebContents()->GetLastCommittedURL();
    ::complex_tasks::RemoveMatchingCart(model(), current_url);
  }

  void OnAddToCart() override {
    VLOG(1) << "Received OnAddToCart in the browser process";

    const BookmarkNode* cart_node = GetMatchingCart();
    std::string image_count_str;
    cart_node->GetMetaInfo("image_count", &image_count_str);
    int image_count_int;
    base::StringToInt(image_count_str, &image_count_int);
    if (image_count_int > 0)
      return;

    model()->SetNodeMetaInfo(cart_node, "image_count", "1");
    model()->SetNodeMetaInfo(cart_node, "image_url_0", "");
  }

  void OnVisitCart() override {
    VLOG(1) << "Received OnVisitCart in the browser process";
    // TODO: update bookmark URL for those not in GetDomainToCart().
  }

  void OnCartProductUpdated(std::vector<mojom::ProductPtr> products) override {
    VLOG(1) << "Received OnCartProductUpdated in the browser process";
    VLOG(1) << "Extracted " << products.size() << " product(s).";

    if (products.size() == 0) {
      RemoveMatchingCart();
      return;
    }
    const BookmarkNode* cart_node = GetMatchingCart();
    model()->SetNodeMetaInfo(cart_node, "image_count",
                             base::NumberToString(products.size()));
    for (unsigned i = 0; i < products.size(); i++) {
      const auto& product = products[i];
      VLOG(1) << "image_url = " << product->image_url;
      model()->SetNodeMetaInfo(cart_node,
                               "image_url_" + base::NumberToString(i),
                               product->image_url);
    }
    model()->SetNodeMetaInfo(
        cart_node, "time_stamp",
        base::NumberToString(base::Time::Now().ToDoubleT()));
  }

  void OnVisitCheckout() override {
    VLOG(1) << "Received OnVisitCheckout in the browser process";
  }

  void OnPurchase() override {
    VLOG(1) << "Received OnPurchase in the browser process";
    // TODO: remove bookmark.
  }

 private:
  base::WeakPtr<CommerceHintService> service_;
  bookmarks::BookmarkModel* model_;
};

CommerceHintService::CommerceHintService(content::WebContents* web_contents)
    : web_contents_(web_contents) {}

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

WEB_CONTENTS_USER_DATA_KEY_IMPL(CommerceHintService)

}  // namespace complex_tasks
