// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_tab_indicator_helper.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/user_education_features.h"
#include "content/public/browser/web_contents.h"

namespace glic {

namespace {

// Use the session start grace period as a guide for how often we could check to
// see if the promo should show. As a side effect, will not attempt to show the
// promo during a grace period that occurs at browser process start. Note that
// this is typically an anti-pattern - "wait until after grace period then show"
// immediately" is not a substitute for "I'd like to show this to everyone at
// startup but can't", however in this case since we will be checking
// periodically anyway until/unless we're told the promo can't be shown, it's as
// good as any of an interval to use.
base::TimeDelta GetPromoCheckInterval() {
  return user_education::features::GetSessionStartGracePeriod() +
         base::Minutes(1);
}

}  // namespace

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
class GlicTabIndicatorHelper::PromoHelper {
 public:
  explicit PromoHelper(GlicTabIndicatorHelper& owner) : owner_(owner) {
    show_timer_.Start(FROM_HERE, GetPromoCheckInterval(),
                      base::BindRepeating(&PromoHelper::MaybeShowPromo,
                                          weak_ptr_factory_.GetWeakPtr()));
  }
  ~PromoHelper() = default;

 private:
  BrowserWindowInterface& browser() {
    return *owner_->tab_->GetBrowserWindowInterface();
  }

  void MaybeShowPromo() {
    // Determine that there is a valid active tab we could show the promo for.
    auto* const tab = browser().GetActiveTabInterface();
    if (!tab) {
      return;
    }
    auto* const contents = tab->GetContents();
    if (!contents || !contents->GetURL().SchemeIsHTTPOrHTTPS() ||
        !contents->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
      return;
    }

    // Attempt to show the promo. Results will be sent to `OnShowPromoResult()`.
    //
    // Note that under User Education Experience 2.0 this could interrupt the
    // user at an inopportune time. However in 2.5 there will be much less
    // chance of an interruption because promos are paused when the user is
    // interacting with the browser.
    user_education::FeaturePromoParams params(
        feature_engagement::kIPHGlicPromoFeature);
    params.show_promo_result_callback = base::BindOnce(
        &PromoHelper::OnShowPromoResult, weak_ptr_factory_.GetWeakPtr());
    browser().GetUserEducationInterface()->MaybeShowFeaturePromo(
        std::move(params));
  }

  void OnShowPromoResult(user_education::FeaturePromoResult result) {
    // If there's no chance a promo could be shown in this browser window, stop
    // trying to check.
    if (result.is_blocked_this_instance()) {
      show_timer_.Stop();
    }
  }

  const raw_ref<GlicTabIndicatorHelper> owner_;

  // Limit how often we check to see if a promo can be shown; this prevents
  // hammering the feature promo system constantly.
  base::RepeatingTimer show_timer_;

  base::WeakPtrFactory<PromoHelper> weak_ptr_factory_{this};
};

// Tab indicator helper implementation:

GlicTabIndicatorHelper::GlicTabIndicatorHelper(tabs::TabInterface* tab)
    : tab_(tab), promo_helper_(std::make_unique<PromoHelper>(*this)) {
  auto* const service = glic::GlicKeyedServiceFactory::GetGlicKeyedService(
      tab_->GetBrowserWindowInterface()->GetProfile());
  focus_change_subscription_ = service->AddFocusedTabChangedCallback(
      base::BindRepeating(&GlicTabIndicatorHelper::OnFocusedTabChanged,
                          base::Unretained(this)));
  indicator_change_subscription_ =
      service->AddContextAccessIndicatorStatusChangedCallback(
          base::BindRepeating(&GlicTabIndicatorHelper::OnIndicatorStatusChanged,
                              base::Unretained(this)));
  will_detach_subscription_ = tab_->RegisterWillDetach(base::BindRepeating(
      &GlicTabIndicatorHelper::OnTabWillDetach, base::Unretained(this)));
  did_insert_subscription_ = tab_->RegisterDidInsert(base::BindRepeating(
      &GlicTabIndicatorHelper::OnTabDidInsert, base::Unretained(this)));
}

GlicTabIndicatorHelper::~GlicTabIndicatorHelper() = default;

void GlicTabIndicatorHelper::MaybeUpdateTab(
    const content::WebContents* contents) {
  if (!contents) {
    return;
  }
  auto* const model = tab_->GetBrowserWindowInterface()->GetTabStripModel();
  CHECK(model);
  const int index = model->GetIndexOfWebContents(contents);
  if (index == TabStripModel::kNoTab) {
    return;
  }
  model->UpdateWebContentsStateAt(index, TabChangeType::kAll);
}

void GlicTabIndicatorHelper::OnFocusedTabChanged(
    const content::WebContents* contents) {
  if (is_detached_) {
    return;
  }

  // TODO(crbug.com/388595318): Simplify this once focus manager changes are
  // finalized.
  MaybeUpdateTab(last_focused_tab_.get());
  MaybeUpdateTab(contents);
  if (contents) {
    // GetWeakPtr() isn't const, but we store a const pointer, so this is
    // safe.
    last_focused_tab_ =
        const_cast<content::WebContents*>(contents)->GetWeakPtr();
  } else {
    last_focused_tab_.reset();
  }
}

void GlicTabIndicatorHelper::OnIndicatorStatusChanged(bool enabled) {
  if (context_access_indicator_enabled_ == enabled) {
    return;
  }
  context_access_indicator_enabled_ = enabled;
  MaybeUpdateTab(last_focused_tab_.get());
}

void GlicTabIndicatorHelper::OnTabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  is_detached_ = true;
}

void GlicTabIndicatorHelper::OnTabDidInsert(tabs::TabInterface* tab) {
  is_detached_ = false;
}

}  // namespace glic
