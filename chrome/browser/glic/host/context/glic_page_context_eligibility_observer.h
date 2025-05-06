// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PAGE_CONTEXT_ELIGIBILITY_OBSERVER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PAGE_CONTEXT_ELIGIBILITY_OBSERVER_H_

#include <optional>
#include <variant>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace optimization_guide {
class OptimizationMetadata;
}  // namespace optimization_guide

class OptimizationGuideKeyedService;

namespace glic {

// A class to handle whether a page is eligible for context sharing with GLIC.
class GlicPageContextEligibilityObserver : public content::WebContentsObserver {
 public:
  ~GlicPageContextEligibilityObserver() override;
  GlicPageContextEligibilityObserver(
      const GlicPageContextEligibilityObserver&) = delete;
  GlicPageContextEligibilityObserver& operator=(
      const GlicPageContextEligibilityObserver&) = delete;

  // Creates a `GlicPageContextEligibilityObserver` if eligible.
  static std::unique_ptr<GlicPageContextEligibilityObserver>
  MaybeCreateForWebContents(content::WebContents* web_contents);

  // Gets eligibility for `web_contents` if allowed. Returns whether callback
  // was actually registered. If registered, `eligibility_callback` is
  // guaranteed to be invoked.
  static bool MaybeGetEligibilityForWebContents(
      content::WebContents* web_contents,
      base::OnceCallback<void(bool)> eligibility_callback);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Gets the eligibility for the current page.
  void GetEligibility(base::OnceCallback<void(bool)> eligibility_callback);

 private:
  GlicPageContextEligibilityObserver(
      content::WebContents* web_contents,
      OptimizationGuideKeyedService* optimization_guide_keyed_service);

  // Callback invoked when eligibility result has returned.
  void OnEligibilityResult(
      const GURL& url,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Notifies callbacks with `is_eligible`.
  void NotifyCallbacks(bool is_eligible);

  // Tracks whether the current main-frame navigation is eligible.
  std::optional<bool> is_eligible_;
  bool is_currently_waiting_ = false;

  // Tracks any outstanding requests for eligibility.
  // Note that we do not use a `base::CallbackList` here to have some more
  // control since CallbackList does not have a Reset function.
  std::vector<base::OnceCallback<void(bool)>> eligibility_callbacks_;

  // Not owned.
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_;

  base::WeakPtrFactory<GlicPageContextEligibilityObserver> weak_ptr_factory_{
      this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PAGE_CONTEXT_ELIGIBILITY_OBSERVER_H_
