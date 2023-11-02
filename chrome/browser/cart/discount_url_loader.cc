// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/discount_url_loader.h"

#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"

DiscountURLLoader::DiscountURLLoader(Browser* browser, Profile* profile) {
  if (!browser) {
    return;
  }
  browser->tab_strip_model()->AddObserver(this);
  BrowserList::GetInstance()->AddObserver(this);
  cart_service_ = CartServiceFactory::GetForProfile(profile);
}

DiscountURLLoader::~DiscountURLLoader() = default;

void DiscountURLLoader::ShutDown() {
  BrowserList::GetInstance()->RemoveObserver(this);
}

void DiscountURLLoader::PrepareURLForDiscountLoad(const GURL& url) {
  last_interacted_url_ = url;
}

void DiscountURLLoader::TabChangedAt(content::WebContents* contents,
                                     int index,
                                     TabChangeType change_type) {
  if (change_type != TabChangeType::kAll) {
    return;
  }
  if (last_interacted_url_) {
    if (last_interacted_url_ == contents->GetVisibleURL()) {
      cart_service_->GetDiscountURL(
          contents->GetVisibleURL(),
          base::BindOnce(&DiscountURLLoader::NavigateToDiscountURL,
                         weak_ptr_factory_.GetWeakPtr(),
                         contents->GetWeakPtr()));
    }
    last_interacted_url_.reset();
  }
}

void DiscountURLLoader::OnBrowserAdded(Browser* browser) {
  browser->tab_strip_model()->AddObserver(this);
}

void DiscountURLLoader::OnBrowserRemoved(Browser* browser) {
  browser->tab_strip_model()->RemoveObserver(this);
}

void DiscountURLLoader::NavigateToDiscountURL(
    base::WeakPtr<content::WebContents> contents,
    const GURL& discount_url) {
  if (!contents) {
    return;
  }
  contents->GetController().LoadURL(discount_url, content::Referrer(),
                                    ui::PAGE_TRANSITION_FIRST, std::string());
}
