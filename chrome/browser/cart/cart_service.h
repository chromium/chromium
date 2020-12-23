// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CART_CART_SERVICE_H_
#define CHROME_BROWSER_CART_CART_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_registry_simple.h"

// Service to maintain and read/write data for chrome cart module.
class CartService : public KeyedService {
 public:
  CartService(const CartService&) = delete;
  CartService& operator=(const CartService&) = delete;
  ~CartService() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  // Gets called when cart module is dismissed.
  void Dismiss();
  // Gets called when the dismiss of cart module is restored.
  void Restore();
  // Returns the current dismiss status of cart module.
  bool IsDismissed();

 private:
  friend class CartServiceFactory;

  // Use |CartServiceFactory::GetForProfile(...)| to get an instance of this
  // service.
  explicit CartService(Profile* profile);
  Profile* profile_;
  base::WeakPtrFactory<CartService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_CART_CART_SERVICE_H_
