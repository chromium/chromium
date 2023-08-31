// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_CORE_PROMO_HANDLER_H_
#define CHROME_BROWSER_COMPANION_CORE_PROMO_HANDLER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/companion/core/mojom/companion.mojom.h"

class PrefRegistrySimple;
class PrefService;

namespace companion {
using side_panel::mojom::PromoAction;
using side_panel::mojom::PromoType;

class SigninDelegate;

// Central class to handle user actions on various promos displayed in the
// search companion.
class PromoHandler {
 public:
  PromoHandler(PrefService* pref_service, SigninDelegate* signin_delegate);
  ~PromoHandler();

  // Disallow copy/assign.
  PromoHandler(const PromoHandler&) = delete;
  PromoHandler& operator=(const PromoHandler&) = delete;

  // Registers preferences used by this class in the provided |registry|.  This
  // should be called for the Profile registry.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Called in response to the mojo call from renderer. Takes necessary action
  // to handle the user action on the promo.
  void OnPromoAction(PromoType promo_type, PromoAction promo_action);

 private:
  void OnSigninPromo(PromoAction promo_action);
  void OnMsbbPromo(PromoAction promo_action);
  void OnExpsPromo(PromoAction promo_action);
  void OnPcoPromo(PromoAction promo_action);
  void IncrementPref(const std::string& pref_name);

  // Lifetime of the PrefService is bound to profile which outlives the lifetime
  // of the companion page.
  raw_ptr<PrefService> pref_service_;

  // Delegate to handle promo acceptance flow.
  raw_ptr<SigninDelegate> signin_delegate_;
};

}  // namespace companion

#endif  // CHROME_BROWSER_COMPANION_CORE_PROMO_HANDLER_H_
