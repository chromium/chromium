// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CART_CART_DB_H_
#define CHROME_BROWSER_CART_CART_DB_H_

#include "base/memory/weak_ptr.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace cart_db {
class ChromeCartContentProto;
}  // namespace cart_db

template <typename T>
class ProfileProtoDB;

class CartDB {
 public:
  explicit CartDB(content::BrowserContext* browser_context);
  CartDB(const CartDB&) = delete;
  CartDB& operator=(const CartDB&) = delete;
  ~CartDB();

 private:
  ProfileProtoDB<cart_db::ChromeCartContentProto>* proto_db_;
  base::WeakPtrFactory<CartDB> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_CART_CART_DB_H_
