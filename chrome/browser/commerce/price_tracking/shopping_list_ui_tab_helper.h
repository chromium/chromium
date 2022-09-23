// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_PRICE_TRACKING_SHOPPING_LIST_UI_TAB_HELPER_H_
#define CHROME_BROWSER_COMMERCE_PRICE_TRACKING_SHOPPING_LIST_UI_TAB_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_registry_simple.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

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

 private:
  friend class content::WebContentsUserData<ShoppingListUiTabHelper>;

  explicit ShoppingListUiTabHelper(content::WebContents* contents);
  base::WeakPtrFactory<ShoppingListUiTabHelper> weak_ptr_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};
}  // namespace commerce
#endif  // CHROME_BROWSER_COMMERCE_PRICE_TRACKING_SHOPPING_LIST_UI_TAB_HELPER_H_
