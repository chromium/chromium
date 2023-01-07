// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CART_CART_DB_H_
#define CHROME_BROWSER_CART_CART_DB_H_

#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace cart_db {
class ChromeCartContentProto;
}  // namespace cart_db

template <typename T>
class SessionProtoDB;

class CartDB {
 public:
  using KeyAndValue = std::pair<std::string, cart_db::ChromeCartContentProto>;

  // Callback which is used when cart content is acquired.
  using LoadCallback = base::OnceCallback<void(bool, std::vector<KeyAndValue>)>;

  // Used for confirming an operation was completed successfully (e.g.
  // insert, delete).
  using OperationCallback = base::OnceCallback<void(bool)>;

  explicit CartDB(content::BrowserContext* browser_context);
  CartDB(const CartDB&) = delete;
  CartDB& operator=(const CartDB&) = delete;
  ~CartDB();

  // Load the cart for a domain.
  void LoadCart(const std::string& domain, LoadCallback callback);

  // Load all carts in the database.
  void LoadAllCarts(LoadCallback callback);

  // Load all carts with certain prefix in the database.
  void LoadCartsWithPrefix(const std::string& prefix, LoadCallback callback);

  // Add a cart to the database.
  void AddCart(const std::string& domain,
               const cart_db::ChromeCartContentProto& proto,
               OperationCallback callback);

  // Delete the cart from certain domain in the database.
  void DeleteCart(const std::string& domain, OperationCallback callback);

  // Delete all carts in the database.
  void DeleteAllCarts(OperationCallback callback);

  // Delete all carts with certain prefix in the database.
  void DeleteCartsWithPrefix(const std::string& prefix,
                             OperationCallback callback);

 private:
  raw_ptr<SessionProtoDB<cart_db::ChromeCartContentProto>, DanglingUntriaged>
      proto_db_;
  base::WeakPtrFactory<CartDB> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_CART_CART_DB_H_
