// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CART_CART_SERVICE_H_
#define CHROME_BROWSER_CART_CART_SERVICE_H_

#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observation.h"
#include "base/values.h"
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
  // The maximum number of times that cart welcome surface shows.
  static constexpr int kWelcomSurfaceShowLimit = 3;

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
  // Get the proto database owned by the service.
  CartDB* GetDB();
  // Load the cart for a domain.
  void LoadCart(const std::string& domain, CartDB::LoadCallback callback);
  // Load all active carts in this service.
  void LoadAllActiveCarts(CartDB::LoadCallback callback);
  // Add a cart to the cart service.
  void AddCart(const std::string& domain,
               const base::Optional<GURL>& cart_url,
               const cart_db::ChromeCartContentProto& proto);
  // Delete the cart from certain domain in the cart service.
  void DeleteCart(const std::string& domain);
  // Only load carts with fake data in the database.
  void LoadCartsWithFakeData(CartDB::LoadCallback callback);
  // Gets called when a single cart in module is temporarily hidden.
  void HideCart(const GURL& cart_url, CartDB::OperationCallback callback);
  // Gets called when restoring the temporarily hidden single cart.
  void RestoreHiddenCart(const GURL& cart_url,
                         CartDB::OperationCallback callback);
  // Gets called when a single cart in module is permanently removed.
  void RemoveCart(const GURL& cart_url, CartDB::OperationCallback callback);
  // Gets called when restoring the permanently removed single cart.
  void RestoreRemovedCart(const GURL& cart_url,
                          CartDB::OperationCallback callback);
  // Gets called when module shows welcome surface and increases the counter by
  // one.
  void IncreaseWelcomeSurfaceCounter();
  // Returns whether to show the welcome surface in module. It is related to how
  // many times the welcome surface has shown.
  bool ShouldShowWelcomeSurface();
  // Gets called when user has acknowledged the discount consent in cart module.
  // shouldEnable indicates whether user has chosen to opt-in or opt-out the
  // feature.
  void AcknowledgeDiscountConsent(bool should_enable);
  // Returns whether to show the consent card in module for rule-based discount.
  bool ShouldShowDiscountConsent();
  // Returns whether the rule-based discount feature in cart module is enabled,
  // and user has chosen to opt-in the feature.
  bool IsCartDiscountEnabled();
  // Updates whether the rule-based discount feature is enabled.
  void SetCartDiscountEnabled(bool enabled);
  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;
  // KeyedService:
  void Shutdown() override;

 private:
  friend class CartServiceFactory;
  friend class CartServiceTest;
  FRIEND_TEST_ALL_PREFIXES(CartHandlerNtpModuleFakeDataTest,
                           TestEnableFakeData);

  // Use |CartServiceFactory::GetForProfile(...)| to get an instance of this
  // service.
  explicit CartService(Profile* profile);
  // Callback when a database operation (e.g. insert or delete) is finished.
  void OnOperationFinished(bool success);
  // Callback when a database operation (e.g. insert or delete) is finished.
  // A callback will be passed in to notify whether the operation is successful.
  void OnOperationFinishedWithCallback(CartDB::OperationCallback callback,
                                       bool success);
  // Add carts with fake data to database.
  void AddCartsWithFakeData();
  // Delete carts with fake data from database.
  void DeleteCartsWithFakeData();
  // Delete content of carts that are removed from database.
  void DeleteRemovedCartsContent(bool success,
                                 std::vector<CartDB::KeyAndValue> proto_pairs);
  // A callback to filter out inactive carts for cart data loading.
  void OnLoadCarts(CartDB::LoadCallback callback,
                   bool success,
                   std::vector<CartDB::KeyAndValue> proto_pairs);
  // A callback to set the hidden status of a cart.
  void SetCartHiddenStatus(bool isHidden,
                           CartDB::OperationCallback callback,
                           bool success,
                           std::vector<CartDB::KeyAndValue> proto_pairs);
  // A callback to set the removed status of a cart.
  void SetCartRemovedStatus(bool isRemoved,
                            CartDB::OperationCallback callback,
                            bool success,
                            std::vector<CartDB::KeyAndValue> proto_pairs);
  // A callback to handle adding a cart.
  void OnAddCart(const std::string& domain,
                 const base::Optional<GURL>& cart_url,
                 cart_db::ChromeCartContentProto proto,
                 bool success,
                 std::vector<CartDB::KeyAndValue> proto_pairs);

  Profile* profile_;
  std::unique_ptr<CartDB> cart_db_;
  history::HistoryService* history_service_;
  base::ScopedObservation<history::HistoryService, HistoryServiceObserver>
      history_service_observation_{this};
  base::Optional<base::Value> domain_name_mapping_;
  base::Optional<base::Value> domain_cart_url_mapping_;
  base::WeakPtrFactory<CartService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_CART_CART_SERVICE_H_
