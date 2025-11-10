// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_IPH_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_IPH_CONTROLLER_H_

#include "base/timer/timer.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"

namespace glic {

// Provides logic of when to show the IPH promo for this feature.
//
// Current logic is "periodically, starting after the session start grace
// period". This is really not great, except that none of the alternatives were
// any better.
//
// For example, consider checking every time the active page changes. This would
// require lots of observers (tab strip selection, active web contents
// navigation) that would need to be kept updated. It would also run during
// grace and cooldown periods where the promo could not possibly be shown, and
// without additional delay logic would show the IPH - and interrupt the user -
// as soon as they landed on a new page, which could happen during e.g.
// tab-switching or clicking links (which would be very bad).
//
// A potential future upgrade is to do the above, but put showing the promo on a
// delay timer so that it schedules to show, say, ten seconds after an eligible
// page is active and ready, and any tab-switching or navigation in that window
// results in the timer being reset. For now, however, this implementation
// suffices.
class GlicIphController {
 public:
  GlicIphController(BrowserWindowInterface* browser_window,
                    GlicKeyedService& glic_service);
  ~GlicIphController();

  void MaybeShowPromoForTest() { MaybeShowPromo(); }

 private:
  void MaybeShowPromo();

  void OnShowPromoResult(user_education::FeaturePromoResult result);
  void OnShowPromoWithCtaResult(user_education::FeaturePromoResult result);

  // Whether to show the old or the new IPH with a Call to Action.
  bool show_cta_;

  const raw_ref<BrowserWindowInterface> window_;
  const raw_ref<GlicKeyedService> glic_service_;

  // Limit how often we check to see if a promo can be shown; this prevents
  // hammering the feature promo system constantly.
  base::RepeatingTimer show_timer_;

  base::WeakPtrFactory<GlicIphController> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_IPH_CONTROLLER_H_
