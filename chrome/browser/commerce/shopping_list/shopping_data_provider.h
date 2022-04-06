// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_SHOPPING_LIST_SHOPPING_DATA_PROVIDER_H_
#define CHROME_BROWSER_COMMERCE_SHOPPING_LIST_SHOPPING_DATA_PROVIDER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/data_decoder/public/cpp/json_sanitizer.h"

namespace base {
class Value;
}

namespace commerce {
class BuyableProduct;
}  // namespace commerce

namespace content {
class Page;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace power_bookmarks {
class PowerBookmarkMeta;
class ShoppingSpecifics;
}

namespace shopping_list {

extern const char kOgTitle[];
extern const char kOgImage[];
extern const char kOgPriceCurrency[];
extern const char kOgPriceAmount[];

// The conversion multiplier to go from standard currency units to
// micro-currency units.
extern const long kToMicroCurrency;

// The type of fallback data can be used when generating shopping meta.
enum class ShoppingDataProviderFallback {
  kTitle = 0,
  kLeadImage = 1,
  kFallbackImage = 2,
  kPrice = 3,
  kMaxValue = kPrice,
};

class ShoppingDataProvider
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ShoppingDataProvider> {
 public:
  ~ShoppingDataProvider() override;
  ShoppingDataProvider(const ShoppingDataProvider& other) = delete;
  ShoppingDataProvider& operator=(const ShoppingDataProvider& other) = delete;

  // Provides a copy of the metadata held by this provider.
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> GetCurrentMetadata();

  void PrimaryPageChanged(content::Page& page) override;

  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

 private:
  friend class content::WebContentsUserData<ShoppingDataProvider>;

  ShoppingDataProvider(
      content::WebContents* contents,
      optimization_guide::NewOptimizationGuideDecider* optimization_guide);

  void OnOptimizationGuideDecision(
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Handle the result of javascript on-page heuristics.
  void OnJavascriptExecutionCompleted(base::Value result);

  // Handle the result of the JSON (from the on-page javascript) sanitization.
  void OnJsonSanitizationCompleted(data_decoder::JsonSanitizer::Result result);

  // Whether the javascript heuristics should be run when the page has finished
  // loading.
  bool run_javascript_on_load_;

  // The metadata for the last navigation in the associated web contents.
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta_for_navigation_;

  raw_ptr<optimization_guide::NewOptimizationGuideDecider> optimization_guide_;

  base::WeakPtrFactory<ShoppingDataProvider> weak_ptr_factory_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

// Merge shopping data from existing |meta| and the result of on-page
// heuristics -- a JSON object holding key -> value pairs (a map) stored in
// |on_page_data_map|.
void MergeData(power_bookmarks::PowerBookmarkMeta* meta,
               base::Value& on_page_data_map);

// Populate the power bookmarks specific representation of shopping data from
// the over-the-wire version from commerce.
void PopulateShoppingSpecifics(
    const commerce::BuyableProduct& data,
    power_bookmarks::ShoppingSpecifics* shopping_specifics);

}  // namespace shopping_list

#endif  // CHROME_BROWSER_COMMERCE_SHOPPING_LIST_SHOPPING_DATA_PROVIDER_H_
