// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CART_CART_HANDLER_H_
#define CHROME_BROWSER_CART_CART_HANDLER_H_

#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/cart/chrome_cart.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

// Handles requests of chrome cart module sent from JS.
class CartHandler : public chrome_cart::mojom::CartHandler {
 public:
  CartHandler(mojo::PendingReceiver<chrome_cart::mojom::CartHandler> handler,
              Profile* profile);
  ~CartHandler() override;

  // chrome_cart::mojom::CartHandler:
  void GetMerchantCarts(GetMerchantCartsCallback callback) override;
  void HideCartModule() override;
  void RestoreHiddenCartModule() override;
  void HideCart(const GURL& cart_url, HideCartCallback callback) override;
  void RestoreHiddenCart(const GURL& cart_url,
                         RestoreHiddenCartCallback callback) override;
  void RemoveCart(const GURL& cart_url, RemoveCartCallback callback) override;
  void RestoreRemovedCart(const GURL& cart_url,
                          RestoreRemovedCartCallback callback) override;
  void GetWarmWelcomeVisible(GetWarmWelcomeVisibleCallback callback) override;
  void OnCartItemClicked(uint32_t index) override;
  void OnModuleCreated(uint32_t count) override;
  void GetDiscountConsentCardVisible(
      GetDiscountConsentCardVisibleCallback callback) override;
  void OnDiscountConsentAcknowledged(bool accept) override;
  void GetDiscountEnabled(GetDiscountEnabledCallback callback) override;
  void SetDiscountEnabled(bool enabled) override;

 private:
  void GetCartDataCallback(GetMerchantCartsCallback callback,
                           bool success,
                           std::vector<CartDB::KeyAndValue> res);
  mojo::Receiver<chrome_cart::mojom::CartHandler> handler_;
  CartService* cart_service_;
  base::WeakPtrFactory<CartHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_CART_CART_HANDLER_H_
