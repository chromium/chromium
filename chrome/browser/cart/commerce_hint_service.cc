// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/commerce_hint_service.h"

#include <map>
#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_heuristics_data.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "crypto/random.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "components/commerce/core/proto/cart_db_content.pb.h"
#include "components/search/ntp_features.h"
#endif

namespace cart {

namespace {
// TODO(crbug.com/40163450): support multiple cart systems in the same domain.
// Returns eTLB+1 domain.
std::string GetDomain(const GURL& url) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

#if !BUILDFLAG(IS_ANDROID)
void ConstructCartProto(cart_db::ChromeCartContentProto* proto,
                        const GURL& navigation_url,
                        std::vector<mojom::ProductPtr> products) {
  const std::string& domain = GetDomain(navigation_url);
  proto->set_key(domain);
  proto->set_merchant(domain);
  proto->set_merchant_cart_url(navigation_url.spec());
  proto->set_timestamp(base::Time::Now().InSecondsFSinceUnixEpoch());
  for (auto& product : products) {
    if (product->image_url.spec().size() != 0) {
      proto->add_product_image_urls(product->image_url.spec());
    }
    if (!product->product_id.empty()) {
      cart_db::ChromeCartProductProto product_proto;
      product_proto.set_product_id(std::move(product->product_id));
      cart_db::ChromeCartProductProto* added_product =
          proto->add_product_infos();
      *added_product = std::move(product_proto);
    }
  }
}
#endif

}  // namespace

// Implementation of the Mojo CommerceHintObserver. This is called by the
// renderer to notify the browser that a commerce hint happens.
class CommerceHintObserverImpl
    : public content::DocumentService<mojom::CommerceHintObserver> {
 public:
  explicit CommerceHintObserverImpl(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<mojom::CommerceHintObserver> receiver,
      base::WeakPtr<CommerceHintService> service)
      : DocumentService(render_frame_host, std::move(receiver)),
        binding_url_(render_frame_host.GetLastCommittedURL()),
        service_(std::move(service)) {}

  ~CommerceHintObserverImpl() override = default;

  void OnAddToCart(const std::optional<GURL>& cart_url,
                   const std::string& product_id) override {
    DVLOG(1) << "Received OnAddToCart in the browser process on "
             << binding_url_;
    if (!service_ || !binding_url_.SchemeIsHTTPOrHTTPS())
      return;
    service_->OnAddToCart(binding_url_, cart_url, product_id);
  }

  void OnVisitCart() override {
    DVLOG(1) << "Received OnVisitCart in the browser process";
    if (!service_ || !binding_url_.SchemeIsHTTPOrHTTPS())
      return;
    service_->OnAddToCart(binding_url_, binding_url_);
  }

  void OnCartProductUpdated(std::vector<mojom::ProductPtr> products) override {
    DVLOG(1) << "Received OnCartProductUpdated in the browser process, with "
             << products.size() << " product(s).";
    if (!service_ || !binding_url_.SchemeIsHTTPOrHTTPS())
      return;

    if (products.empty()) {
      service_->OnRemoveCart(binding_url_);
    } else {
      service_->OnCartUpdated(binding_url_, std::move(products));
    }
  }

  void OnVisitCheckout() override {
    DVLOG(1) << "Received OnVisitCheckout in the browser process";
    if (!service_ || !binding_url_.SchemeIsHTTPOrHTTPS())
      return;
    service_->OnRemoveCart(binding_url_);
  }

  void OnPurchase() override {
    DVLOG(1) << "Received OnPurchase in the browser process";
    if (!service_ || !binding_url_.SchemeIsHTTPOrHTTPS())
      return;
    service_->OnRemoveCart(binding_url_);
  }

  void OnFormSubmit(bool is_purchase) override {
    DVLOG(1) << "Received OnFormSubmit in the browser process";
    if (!service_ || !binding_url_.SchemeIsHTTPOrHTTPS())
      return;
    service_->OnFormSubmit(binding_url_, is_purchase);
  }

  void OnWillSendRequest(bool is_addtocart) override {
    DVLOG(1) << "Received OnWillSendRequest in the browser process";
    if (!service_ || !binding_url_.SchemeIsHTTPOrHTTPS())
      return;
    service_->OnWillSendRequest(binding_url_, is_addtocart);
  }

  void OnNavigation(const GURL& url,
                    const std::string& version_number,
                    OnNavigationCallback callback) override {
    mojom::HeuristicsPtr ptr(mojom::Heuristics::New());
    bool should_skip = service_->ShouldSkip(url);
    if (should_skip) {
      std::move(callback).Run(should_skip, std::move(ptr));
      return;
    }
    ptr->version_number =
        commerce_heuristics::CommerceHeuristicsData::GetInstance().GetVersion();
    // If the version number of heuristics on renderer side is up to date, skip
    // sending heuristics.
    if (ptr->version_number == version_number) {
      std::move(callback).Run(should_skip, std::move(ptr));
      return;
    }
    auto hint_heuristics =
        commerce_heuristics::CommerceHeuristicsData::GetInstance()
            .GetHintHeuristicsJSONForDomain(GetDomain(url));
    auto global_heuristics =
        commerce_heuristics::CommerceHeuristicsData::GetInstance()
            .GetGlobalHeuristicsJSON();
    // Populate if there is heuristics data from component, otherwise initialize
    // heuristics with empty JSON.
    ptr->hint_json_data =
        hint_heuristics.has_value() ? std::move(*hint_heuristics) : "{}";
    ptr->global_json_data =
        global_heuristics.has_value() ? std::move(*global_heuristics) : "{}";
    std::move(callback).Run(should_skip, std::move(ptr));
  }

  void OnCartExtraction(OnCartExtractionCallback callback) override {
    std::move(callback).Run(
        commerce_heuristics::CommerceHeuristicsData::GetInstance()
            .GetProductIDExtractionJSON(),
        commerce_heuristics::CommerceHeuristicsData::GetInstance()
            .GetCartProductExtractionScript());
  }

 private:
  GURL binding_url_;
  base::WeakPtr<CommerceHintService> service_;
};

CommerceHintService::CommerceHintService(content::WebContents* web_contents)
    : content::WebContentsUserData<CommerceHintService>(*web_contents) {
  DCHECK(!web_contents->GetBrowserContext()->IsOffTheRecord());
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
#if !BUILDFLAG(IS_ANDROID)
  service_ = CartServiceFactory::GetInstance()->GetForProfile(profile);
#endif
  optimization_guide_decider_ =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (optimization_guide_decider_) {
    optimization_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::SHOPPING_PAGE_PREDICTOR});
  }
}

CommerceHintService::~CommerceHintService() = default;

content::WebContents* CommerceHintService::WebContents() {
  return &GetWebContents();
}

void CommerceHintService::BindCommerceHintObserver(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<mojom::CommerceHintObserver> receiver) {
  CHECK(host);
  // The object is bound to the lifetime of |host| and the mojo
  // connection. See DocumentService for details.
  new CommerceHintObserverImpl(*host, std::move(receiver),
                               weak_factory_.GetWeakPtr());
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
                                      const std::optional<GURL>& cart_url,
                                      const std::string& product_id) {
#if !BUILDFLAG(IS_ANDROID)
  if (ShouldSkip(navigation_url))
    return;
  std::optional<GURL> validated_cart = cart_url;
  if (cart_url && GetDomain(*cart_url) != GetDomain(navigation_url)) {
    DVLOG(1) << "Reject cart URL with different eTLD+1 domain.";
    validated_cart = std::nullopt;
  }
  // When rule-based discount is enabled, do not accept cart page URLs from
  // partner merchants as there could be things like discount tokens in them.
  if (service_->IsCartDiscountEnabled() &&
      commerce::IsRuleDiscountPartnerMerchant(navigation_url) &&
      product_id.empty()) {
    validated_cart = std::nullopt;
  }
  cart_db::ChromeCartContentProto proto;
  std::vector<mojom::ProductPtr> products;
  if (!product_id.empty()) {
    mojom::ProductPtr product_ptr(mojom::Product::New());
    product_ptr->product_id = product_id;
    products.push_back(std::move(product_ptr));
  }
  ConstructCartProto(&proto, navigation_url, std::move(products));
  service_->AddCart(navigation_url, validated_cart, std::move(proto));
#endif
}

void CommerceHintService::OnRemoveCart(const GURL& url) {
#if !BUILDFLAG(IS_ANDROID)
  service_->DeleteCart(url, false);
#endif
}

void CommerceHintService::OnCartUpdated(
    const GURL& cart_url,
    std::vector<mojom::ProductPtr> products) {
#if !BUILDFLAG(IS_ANDROID)
  if (ShouldSkip(cart_url))
    return;
  std::optional<GURL> validated_cart = cart_url;
  // When rule-based discount is enabled, do not accept cart page URLs from
  // partner merchants as there could be things like discount tokens in them.
  if (service_->IsCartDiscountEnabled() &&
      commerce::IsRuleDiscountPartnerMerchant(cart_url)) {
    validated_cart = std::nullopt;
  }
  cart_db::ChromeCartContentProto proto;
  ConstructCartProto(&proto, cart_url, std::move(products));
  service_->AddCart(cart_url, validated_cart, std::move(proto));
#endif
}

void CommerceHintService::OnFormSubmit(const GURL& navigation_url,
                                       bool is_purchase) {
  if (ShouldSkip(navigation_url))
    return;
  uint8_t bytes[1];
  crypto::RandBytes(bytes);
  bool report_truth = bytes[0] & 0x1;
  bool random = (bytes[0] >> 1) & 0x1;
  bool reported = report_truth ? is_purchase : random;
  ukm::builders::Shopping_FormSubmitted(
      GetWebContents().GetPrimaryMainFrame()->GetPageUkmSourceId())
      .SetIsTransaction(reported)
      .Record(ukm::UkmRecorder::Get());
  base::UmaHistogramBoolean("Commerce.Carts.FormSubmitIsTransaction", reported);
}

void CommerceHintService::OnWillSendRequest(const GURL& navigation_url,
                                            bool is_addtocart) {
  if (ShouldSkip(navigation_url))
    return;
  uint8_t bytes[1];
  crypto::RandBytes(bytes);
  bool report_truth = bytes[0] & 0x1;
  bool random = (bytes[0] >> 1) & 0x1;
  bool reported = report_truth ? is_addtocart : random;
  ukm::builders::Shopping_WillSendRequest(
      GetWebContents().GetPrimaryMainFrame()->GetPageUkmSourceId())
      .SetIsAddToCart(reported)
      .Record(ukm::UkmRecorder::Get());
  base::UmaHistogramBoolean("Commerce.Carts.XHRIsAddToCart", reported);
}

bool CommerceHintService::InitializeCommerceHeuristicsForTesting(
    base::Version version,
    const std::string& hint_json_data,
    const std::string& global_json_data,
    const std::string& product_id_json_data,
    const std::string& cart_extraction_script) {
  if (!commerce_heuristics::CommerceHeuristicsData::GetInstance()
           .PopulateDataFromComponent(hint_json_data, global_json_data,
                                      product_id_json_data,
                                      cart_extraction_script)) {
    return false;
  }
  commerce_heuristics::CommerceHeuristicsData::GetInstance().UpdateVersion(
      version);
  return true;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CommerceHintService);

}  // namespace cart
