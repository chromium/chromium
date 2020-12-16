// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_handler.h"

CartHandler::CartHandler(
    mojo::PendingReceiver<chrome_cart::mojom::CartHandler> handler,
    Profile* profile)
    : handler_(this, std::move(handler)) {}

CartHandler::~CartHandler() = default;

void CartHandler::GetMerchantCarts(GetMerchantCartsCallback callback) {
  std::vector<chrome_cart::mojom::MerchantCartPtr> carts;
  std::move(callback).Run(std::move(carts));
}
