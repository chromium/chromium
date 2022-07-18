// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/shopping_list/shopping_data_provider.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/power_bookmarks/proto/power_bookmark_meta.pb.h"
#include "chrome/browser/power_bookmarks/proto/shopping_specifics.pb.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/proto/price_tracking.pb.h"
#include "components/grit/components_resources.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/resource/resource_bundle.h"

namespace shopping_list {

const char kOgTitle[] = "title";
const char kOgImage[] = "image";
const char kOgPriceCurrency[] = "price:currency";
const char kOgPriceAmount[] = "price:amount";
const long kToMicroCurrency = 1e6;

ShoppingDataProvider::ShoppingDataProvider(
    content::WebContents* content,
    optimization_guide::NewOptimizationGuideDecider* optimization_guide)
    : content::WebContentsObserver(content),
      content::WebContentsUserData<ShoppingDataProvider>(*content),
      run_javascript_on_load_(false),
      optimization_guide_(optimization_guide),
      weak_ptr_factory_(this) {
  std::vector<optimization_guide::proto::OptimizationType> types;
  types.push_back(optimization_guide::proto::OptimizationType::PRICE_TRACKING);
  optimization_guide_->RegisterOptimizationTypes(types);
}

ShoppingDataProvider::~ShoppingDataProvider() = default;

void ShoppingDataProvider::PrimaryPageChanged(content::Page& page) {
  if (!optimization_guide_)
    return;

  run_javascript_on_load_ = false;
  meta_for_navigation_.reset();
  // This will cancel the callbacks holding a reference to this object so that
  // they do not conflict with the new ones for this navigation.
  weak_ptr_factory_.InvalidateWeakPtrs();

  optimization_guide_->CanApplyOptimization(
      web_contents()->GetURL(),
      optimization_guide::proto::OptimizationType::PRICE_TRACKING,
      base::BindOnce(&ShoppingDataProvider::OnOptimizationGuideDecision,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ShoppingDataProvider::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!run_javascript_on_load_)
    return;

  run_javascript_on_load_ = false;

  std::string script =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_QUERY_SHOPPING_META_JS);

  if (script.empty())
    return;

  base::OnceCallback<void(base::Value)> callback =
      base::BindOnce(&ShoppingDataProvider::OnJavascriptExecutionCompleted,
                     weak_ptr_factory_.GetWeakPtr());
  web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptInIsolatedWorld(
      base::UTF8ToUTF16(script), std::move(callback),
      ISOLATED_WORLD_ID_CHROME_INTERNAL);
}

void ShoppingDataProvider::OnOptimizationGuideDecision(
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  base::UmaHistogramBoolean(
      "Commerce.PowerBookmarks.ShoppingDataProvider.IsProductPage",
      decision == optimization_guide::OptimizationGuideDecision::kTrue);

  // If the page was determined to be shopping related, run the on-page
  // extractor.
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue)
    return;

  run_javascript_on_load_ = true;

  // We should only be creating one bookmark meta object per navigation.
  DCHECK(!meta_for_navigation_);

  meta_for_navigation_ = std::make_unique<power_bookmarks::PowerBookmarkMeta>();
  meta_for_navigation_->set_type(power_bookmarks::PowerBookmarkType::SHOPPING);

  if (metadata.any_metadata().has_value()) {
    absl::optional<commerce::PriceTrackingData> parsed_any =
        optimization_guide::ParsedAnyMetadata<commerce::PriceTrackingData>(
            metadata.any_metadata().value());
    commerce::PriceTrackingData price_data = parsed_any.value();
    if (parsed_any.has_value() && price_data.IsInitialized()) {
      commerce::BuyableProduct buyable_product = price_data.buyable_product();

      if (buyable_product.has_image_url() &&
          base::FeatureList::IsEnabled(commerce::kCommerceAllowServerImages)) {
        meta_for_navigation_->mutable_lead_image()->set_url(
            buyable_product.image_url());
      }
      PopulateShoppingSpecifics(
          buyable_product, meta_for_navigation_->mutable_shopping_specifics());
    }
  }
}

void ShoppingDataProvider::OnJavascriptExecutionCompleted(base::Value result) {
  data_decoder::JsonSanitizer::Sanitize(
      result.GetString(),
      base::BindOnce(&ShoppingDataProvider::OnJsonSanitizationCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ShoppingDataProvider::OnJsonSanitizationCompleted(
    data_decoder::JsonSanitizer::Result result) {
  if (!result.value.has_value())
    return;

  absl::optional<base::Value> json_root =
      base::JSONReader::Read(result.value.value());

  if (!json_root.has_value())
    return;

  MergeData(meta_for_navigation_.get(), json_root.value());
}

std::unique_ptr<power_bookmarks::PowerBookmarkMeta>
ShoppingDataProvider::GetCurrentMetadata() {
  if (!meta_for_navigation_)
    return nullptr;

  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      std::make_unique<power_bookmarks::PowerBookmarkMeta>();
  meta->CopyFrom(*meta_for_navigation_.get());
  return meta;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ShoppingDataProvider);

void MergeData(power_bookmarks::PowerBookmarkMeta* meta,
               base::Value& on_page_data_map) {
  if (!meta)
    return;

  // This will be true if any of the data found in |on_page_data_map| is used to
  // populate fields in |meta|.
  bool data_was_merged = false;

  power_bookmarks::ShoppingSpecifics* product_data =
      meta->mutable_shopping_specifics();

  for (auto it : on_page_data_map.DictItems()) {
    if (base::CompareCaseInsensitiveASCII(it.first, kOgTitle) == 0) {
      if (!product_data->has_title()) {
        product_data->set_title(it.second.GetString());
        base::UmaHistogramEnumeration(
            "Commerce.PowerBookmarks.ShoppingDataProvider.FallbackDataContent",
            ShoppingDataProviderFallback::kTitle,
            ShoppingDataProviderFallback::kMaxValue);
        data_was_merged = true;
      }
    } else if (base::CompareCaseInsensitiveASCII(it.first, kOgImage) == 0) {
      // If the product already has an image, add the one found on the page as
      // a fallback. The original image, if it exists, should have been
      // retrieved from the proto received from optimization guide before this
      // callback runs.
      if (!meta->has_lead_image()) {
        if (base::FeatureList::IsEnabled(commerce::kCommerceAllowLocalImages)) {
          meta->mutable_lead_image()->set_url(it.second.GetString());
        }
        base::UmaHistogramEnumeration(
            "Commerce.PowerBookmarks.ShoppingDataProvider.FallbackDataContent",
            ShoppingDataProviderFallback::kLeadImage,
            ShoppingDataProviderFallback::kMaxValue);
      } else {
        if (base::FeatureList::IsEnabled(commerce::kCommerceAllowLocalImages)) {
          meta->add_fallback_images()->set_url(it.second.GetString());
        }
        base::UmaHistogramEnumeration(
            "Commerce.PowerBookmarks.ShoppingDataProvider.FallbackDataContent",
            ShoppingDataProviderFallback::kFallbackImage,
            ShoppingDataProviderFallback::kMaxValue);
      }
      data_was_merged = true;
    } else if (base::CompareCaseInsensitiveASCII(it.first, kOgPriceCurrency) ==
               0) {
      if (!product_data->has_current_price()) {
        double amount = 0;
        if (base::StringToDouble(
                *on_page_data_map.FindStringKey(kOgPriceAmount), &amount)) {
          power_bookmarks::ProductPrice* price_proto =
              product_data->mutable_current_price();
          // Currency is stored in micro-units rather than standard units, so we
          // need to convert (open graph provides standard units).
          price_proto->set_amount_micros(amount * kToMicroCurrency);
          price_proto->set_currency_code(it.second.GetString());
          base::UmaHistogramEnumeration(
              "Commerce.PowerBookmarks.ShoppingDataProvider."
              "FallbackDataContent",
              ShoppingDataProviderFallback::kPrice,
              ShoppingDataProviderFallback::kMaxValue);
          data_was_merged = true;
        }
      }
    }
  }

  base::UmaHistogramBoolean(
      "Commerce.PowerBookmarks.ShoppingDataProvider.FallbackDataUsed",
      data_was_merged);
}

void PopulateShoppingSpecifics(
    const commerce::BuyableProduct& data,
    power_bookmarks::ShoppingSpecifics* shopping_specifics) {
  if (data.has_title())
    shopping_specifics->set_title(data.title());

  if (data.has_image_url())
    shopping_specifics->set_image_url(data.image_url());

  if (data.has_offer_id())
    shopping_specifics->set_offer_id(data.offer_id());

  if (data.has_product_cluster_id())
    shopping_specifics->set_product_cluster_id(data.product_cluster_id());

  if (data.has_current_price()) {
    power_bookmarks::ProductPrice* price =
        shopping_specifics->mutable_current_price();
    price->set_currency_code(data.current_price().currency_code());
    price->set_amount_micros(data.current_price().amount_micros());
  }

  if (data.has_country_code()) {
    shopping_specifics->set_country_code(data.country_code());
  }
}

}  // namespace shopping_list
