// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CART_DISCOUNT_URL_LOADER_H_
#define CHROME_BROWSER_CART_DISCOUNT_URL_LOADER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents.h"

// TODO(crbug.com/40185906): This is a workaround to try to override navigation
// from context menu. Investigate if there are better ways to handle the second
// navigation.
class DiscountURLLoader : public BrowserListObserver,
                          public TabStripModelObserver {
 public:
  explicit DiscountURLLoader(Browser* browser, Profile* profile);
  ~DiscountURLLoader() override;
  // Called to destroy any observers.
  void ShutDown();

  // Gets called when partner merchant cart with |url| is right clicked. Cache
  // the |url| so that it can later be used to decide if a navigation originates
  // from cart module interaction, and reload page with discount URL if needed.
  void PrepareURLForDiscountLoad(const GURL& url);

  // TabStripModelObserver:
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;
  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

 private:
  void NavigateToDiscountURL(base::WeakPtr<content::WebContents> contents,
                             const GURL& discount_url);
  std::optional<GURL> last_interacted_url_;
  raw_ptr<CartService> cart_service_;
  base::WeakPtrFactory<DiscountURLLoader> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_CART_DISCOUNT_URL_LOADER_H_
