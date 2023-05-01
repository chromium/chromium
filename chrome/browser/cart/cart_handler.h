// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CART_CART_HANDLER_H_
#define CHROME_BROWSER_CART_CART_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/cart/chrome_cart.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;
class PrefService;

namespace content {
class WebContents;
}  // namespace content

// Handles requests of chrome cart module sent from JS.
class CartHandler : public chrome_cart::mojom::CartHandler {
 public:
  CartHandler(mojo::PendingReceiver<chrome_cart::mojom::CartHandler> handler,
              Profile* profile,
              content::WebContents* web_contents);
  ~CartHandler() override;

  // chrome_cart::mojom::CartHandler:
  void GetMerchantCarts(GetMerchantCartsCallback callback) override;
  void GetCartFeatureEnabled(GetCartFeatureEnabledCallback callback) override;
  void HideCartModule() override;
  void RestoreHiddenCartModule() override;
  void HideCart(const GURL& cart_url, HideCartCallback callback) override;
  void RestoreHiddenCart(const GURL& cart_url,
                         RestoreHiddenCartCallback callback) override;
  void RemoveCart(const GURL& cart_url, RemoveCartCallback callback) override;
  void RestoreRemovedCart(const GURL& cart_url,
                          RestoreRemovedCartCallback callback) override;
  void GetWarmWelcomeVisible(GetWarmWelcomeVisibleCallback callback) override;
  void GetDiscountURL(const GURL& cart_url,
                      GetDiscountURLCallback callback) override;
  void GetDiscountConsentCardVisible(
      GetDiscountConsentCardVisibleCallback callback) override;
  void GetDiscountToggleVisible(
      GetDiscountToggleVisibleCallback callback) override;
  void OnDiscountConsentAcknowledged(bool accept) override;
  void OnDiscountConsentDismissed() override;
  void OnDiscountConsentContinued() override;
  void ShowNativeConsentDialog(
      ShowNativeConsentDialogCallback callback) override;
  void GetDiscountEnabled(GetDiscountEnabledCallback callback) override;
  void SetDiscountEnabled(bool enabled) override;
  void PrepareForNavigation(const GURL& cart_url, bool is_navigating) override;

 private:
  void GetCartDataCallback(GetMerchantCartsCallback callback,
                           bool success,
                           std::vector<CartDB::KeyAndValue> res);
  mojo::Receiver<chrome_cart::mojom::CartHandler> handler_;
  raw_ptr<CartService> cart_service_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<PrefService> pref_service_;
  base::WeakPtrFactory<CartHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_CART_CART_HANDLER_H_
