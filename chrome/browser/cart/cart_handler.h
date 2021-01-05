// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CART_CART_HANDLER_H_
#define CHROME_BROWSER_CART_CART_HANDLER_H_

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
  void DismissCartModule() override;
  void RestoreCartModule() override;

 private:
  mojo::Receiver<chrome_cart::mojom::CartHandler> handler_;
  Profile* profile_;
};

#endif  // CHROME_BROWSER_CART_CART_HANDLER_H_
