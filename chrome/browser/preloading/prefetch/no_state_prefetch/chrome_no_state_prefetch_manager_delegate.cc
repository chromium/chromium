// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_manager_delegate.h"

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager_delegate.h"
#include "content/public/browser/browser_thread.h"

namespace prerender {

ChromeNoStatePrefetchManagerDelegate::ChromeNoStatePrefetchManagerDelegate(
    Profile* profile)
    : profile_(profile) {}

scoped_refptr<content_settings::CookieSettings>
ChromeNoStatePrefetchManagerDelegate::GetCookieSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return CookieSettingsFactory::GetForProfile(profile_);
}

void ChromeNoStatePrefetchManagerDelegate::MaybePreconnect(const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(features::kPrerenderFallbackToPreconnect)) {
    return;
  }

  if (GetCookieSettings()->ShouldBlockThirdPartyCookies()) {
    return;
  }

  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile_);
  if (loading_predictor) {
    loading_predictor->PrepareForPageLoad(
        /*initiator_origin=*/std::nullopt, url,
        predictors::HintOrigin::OMNIBOX_PRERENDER_FALLBACK, true);
  }
}

std::unique_ptr<NoStatePrefetchContentsDelegate>
ChromeNoStatePrefetchManagerDelegate::GetNoStatePrefetchContentsDelegate() {
  return std::make_unique<ChromeNoStatePrefetchContentsDelegate>();
}

bool ChromeNoStatePrefetchManagerDelegate::
    IsNetworkPredictionPreferenceEnabled() {
  return prefetch::IsSomePreloadingEnabled(*profile_->GetPrefs()) ==
         content::PreloadingEligibility::kEligible;
}

std::string
ChromeNoStatePrefetchManagerDelegate::GetReasonForDisablingPrediction() {
  if (!IsNetworkPredictionPreferenceEnabled()) {
    return "Disabled by user setting";
  }
  return "";
}

}  // namespace prerender
