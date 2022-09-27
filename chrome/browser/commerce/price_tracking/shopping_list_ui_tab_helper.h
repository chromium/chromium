// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_PRICE_TRACKING_SHOPPING_LIST_UI_TAB_HELPER_H_
#define CHROME_BROWSER_COMMERCE_PRICE_TRACKING_SHOPPING_LIST_UI_TAB_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/commerce/core/shopping_service.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/prefs/pref_registry_simple.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/image/image.h"

class GURL;
class PrefService;

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace image_fetcher {
class ImageFetcherService;
}

namespace commerce {

// This tab helper is used to update and maintain the state of the shopping list
// and price tracking UI on desktop.
class ShoppingListUiTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ShoppingListUiTabHelper> {
 public:
  ~ShoppingListUiTabHelper() override;
  ShoppingListUiTabHelper(const ShoppingListUiTabHelper& other) = delete;
  ShoppingListUiTabHelper& operator=(const ShoppingListUiTabHelper& other) =
      delete;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Get the image for the last fetched product URL. A reference to this object
  // should not be kept directly, if one is needed, a copy should be made.
  const gfx::Image& GetProductImage();

  // The URL for the last fetched product image. A reference to this object
  // should not be kept directly, if one is needed, a copy should be made.
  const GURL& GetProductImageURL();

  // content::WebContentsObserver implementation
  void PrimaryPageChanged(content::Page& page) override;

 private:
  friend class content::WebContentsUserData<ShoppingListUiTabHelper>;

  ShoppingListUiTabHelper(
      content::WebContents* contents,
      ShoppingService* shopping_service,
      image_fetcher::ImageFetcherService* image_fetcher_service,
      PrefService* prefs);

  void HandleProductInfoResponse(const GURL& url,
                                 const absl::optional<ProductInfo>& info);

  void HandleImageFetcherResponse(
      const GURL image_url,
      const gfx::Image& image,
      const image_fetcher::RequestMetadata& request_metadata);

  // The shopping service is tied to the lifetime of the browser context
  // which will always outlive this tab helper.
  raw_ptr<ShoppingService> shopping_service_;
  raw_ptr<PrefService> prefs_;
  raw_ptr<image_fetcher::ImageFetcher> image_fetcher_;

  // The URL of the last product image that was fetched.
  GURL last_fetched_image_url_;

  // The last image that was fetched. See |last_image_fetched_url_| for the
  // URL that was used.
  gfx::Image last_fetched_image_;

  base::WeakPtrFactory<ShoppingListUiTabHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace commerce

#endif  // CHROME_BROWSER_COMMERCE_PRICE_TRACKING_SHOPPING_LIST_UI_TAB_HELPER_H_
