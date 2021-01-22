// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CART_CART_SERVICE_H_
#define CHROME_BROWSER_CART_CART_SERVICE_H_

#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/cart/cart_db.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_registry_simple.h"

// Service to maintain and read/write data for chrome cart module.
// TODO(crbug.com/1157892) Make this BrowserContext-based and get rid of Profile
// usage so that we can modularize this.
class CartService : public history::HistoryServiceObserver,
                    public KeyedService {
 public:
  CartService(const CartService&) = delete;
  CartService& operator=(const CartService&) = delete;
  ~CartService() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  // Gets called when cart module is temporarily hidden.
  void Hide();
  // Gets called when restoring the temporarily hidden cart module.
  void RestoreHidden();
  // Returns whether cart module has been temporarily hidden.
  bool IsHidden();
  // Gets called when cart module is permanently removed.
  void Remove();
  // Gets called when restoring the permanently removed cart module.
  void RestoreRemoved();
  // Returns whether cart module has been permanently removed.
  bool IsRemoved();
  // Get the proto database owned by the service.
  CartDB* GetDB();
  // Load the cart for a domain.
  void LoadCart(const std::string& domain, CartDB::LoadCallback callback);
  // Load all carts in this service.
  void LoadAllCarts(CartDB::LoadCallback callback);
  // Add a cart to the cart service.
  void AddCart(const std::string& domain,
               const cart_db::ChromeCartContentProto& proto);
  // Delete the cart from certain domain in the cart service.
  void DeleteCart(const std::string& domain);
  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;
  // KeyedService:
  void Shutdown() override;

 private:
  friend class CartServiceFactory;

  // Use |CartServiceFactory::GetForProfile(...)| to get an instance of this
  // service.
  explicit CartService(Profile* profile);
  // Callback when a database operation (e.g. insert or delete) is finished.
  void OnOperationFinished(bool success);

  Profile* profile_;
  std::unique_ptr<CartDB> cart_db_;
  history::HistoryService* history_service_;
  base::WeakPtrFactory<CartService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_CART_CART_SERVICE_H_
