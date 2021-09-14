// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/shopping_list/shopping_data_provider.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/power_bookmarks/proto/power_bookmark_meta.pb.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/grit/browser_resources.h"
#include "components/commerce/core/proto/price_tracking.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "content/public/browser/navigation_handle.h"
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
    optimization_guide::OptimizationGuideDecider* optimization_guide)
    : content::WebContentsObserver(content),
      optimization_guide_(optimization_guide),
      weak_ptr_factory_(this) {
  std::vector<optimization_guide::proto::OptimizationType> types;
  types.push_back(optimization_guide::proto::OptimizationType::PRICE_TRACKING);
  optimization_guide_->RegisterOptimizationTypes(types);
}

ShoppingDataProvider::~ShoppingDataProvider() = default;

void ShoppingDataProvider::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame() || !optimization_guide_) {
    return;
  }

  meta_for_navigation_.reset();
  // This will cancel the callbacks holding a reference to this object so that
  // they do not conflict with the new ones for this navigation.
  weak_ptr_factory_.InvalidateWeakPtrs();

  optimization_guide_->CanApplyOptimizationAsync(
      navigation_handle,
      optimization_guide::proto::OptimizationType::PRICE_TRACKING,
      base::BindOnce(&ShoppingDataProvider::OnOptimizationGuideDecision,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ShoppingDataProvider::OnOptimizationGuideDecision(
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  // If the page was determined to be shopping related, run the on-page
  // extractor.
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue)
    return;

  // We should only be creating one bookmark meta object per navigation.
  DCHECK(!meta_for_navigation_);

  meta_for_navigation_ = std::make_unique<power_bookmarks::PowerBookmarkMeta>();
  meta_for_navigation_->set_type(power_bookmarks::PowerBookmarkType::SHOPPING);

  if (metadata.any_metadata().has_value()) {
    commerce::PriceTrackingData price_data;
    // Optimization Guide's metadata provides an absl::optional which holds a
    // proto::Any value -- each having a .value() function. Consequently, the
    // parse logic below looks a bit strange.
    price_data.ParseFromString(metadata.any_metadata().value().value());
    if (price_data.IsInitialized()) {
      commerce::BuyableProduct buyable_product = price_data.buyable_product();

      if (buyable_product.has_image_url()) {
        meta_for_navigation_->mutable_lead_image()->set_url(
            buyable_product.image_url());
      }
      meta_for_navigation_->mutable_shopping_specifics()->CopyFrom(
          buyable_product);
    }
  }

  std::string script =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_QUERY_SHOPPING_META_JS);

  if (script.empty())
    return;

  base::OnceCallback<void(base::Value)> callback =
      base::BindOnce(&ShoppingDataProvider::OnJavascriptExecutionCompleted,
                     weak_ptr_factory_.GetWeakPtr());
  web_contents()->GetMainFrame()->ExecuteJavaScriptInIsolatedWorld(
      base::UTF8ToUTF16(script), std::move(callback),
      ISOLATED_WORLD_ID_CHROME_INTERNAL);
}

void ShoppingDataProvider::OnJavascriptExecutionCompleted(base::Value result) {
  absl::optional<base::Value> json_root =
      base::JSONReader::Read(result.GetString());

  if (!json_root.has_value())
    return;

  MergeData(meta_for_navigation_.get(), json_root.value());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ShoppingDataProvider)

void MergeData(power_bookmarks::PowerBookmarkMeta* meta,
               base::Value& on_page_data_map) {
  if (!meta)
    return;

  commerce::BuyableProduct* product_data = meta->mutable_shopping_specifics();

  for (auto it : on_page_data_map.DictItems()) {
    if (base::CompareCaseInsensitiveASCII(it.first, kOgTitle) == 0) {
      if (!product_data->has_title())
        product_data->set_title(it.second.GetString());
    } else if (base::CompareCaseInsensitiveASCII(it.first, kOgImage) == 0) {
      // If the product already has an image, add the one found on the page as
      // a fallback. The original image, if it exists, should have been
      // retrieved from the proto received from optimization guide before this
      // callback runs.
      if (!meta->has_lead_image()) {
        meta->mutable_lead_image()->set_url(it.second.GetString());
      } else {
        meta->add_fallback_images()->set_url(it.second.GetString());
      }
    } else if (base::CompareCaseInsensitiveASCII(it.first, kOgPriceCurrency) ==
               0) {
      if (!product_data->has_current_price()) {
        double amount = 0;
        if (base::StringToDouble(
                *on_page_data_map.FindStringKey(kOgPriceAmount), &amount)) {
          commerce::ProductPrice* price_proto =
              product_data->mutable_current_price();
          // Currency is stored in micro-units rather than standard units, so we
          // need to convert (open graph provides standard units).
          price_proto->set_amount_micros(amount * kToMicroCurrency);
          price_proto->set_currency_code(it.second.GetString());
        }
      }
    }
  }
}

}  // namespace shopping_list
