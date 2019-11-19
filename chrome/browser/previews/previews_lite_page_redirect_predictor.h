// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_PREDICTOR_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_PREDICTOR_H_

#include <memory>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/nqe/effective_connection_type.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace data_reduction_proxy {
class DataReductionProxySettings;
}

namespace previews {
class PreviewsOptimizationGuide;
}

// Manages background preresolving or preconnecting for lite page redirect
// previews. When a set of conditions are met, this class causes the litepage
// version of the current committed page URL to be preresolved/preconnected in a
// loop so that the DNS entry for the litepage is always cached, or the origin
// URL to be preresolved/preconnected if the current page is a preview. This
// helps to mitigate the delay of loading a preview or the original page of the
// same origin.
//
// All of the following conditions must be met to preresolve/preconnect the
// litepages host on a background loop during a page load:
// * We allow preresolving/preconnecting by feature param.
// * A page is committed.
// * There is no ongoing navigation.
// * Lite mode is enabled for the user. - Note this change is not directly
//   observed because we find that it seldom changes in practice.
// * The lite page redirect feature is enabled.
// * ECT is at or below the lite page redirect threshold.
// * The current page's host is not blacklisted by server hints.
// * The current page isn't a litepage preview already.
// * Chrome is in the foreground.
// * This' |web_contents| is in the foreground.
class PreviewsLitePageRedirectPredictor
    : public network::NetworkQualityTracker::EffectiveConnectionTypeObserver,
      public content::WebContentsObserver,
      public content::WebContentsUserData<PreviewsLitePageRedirectPredictor> {
 public:
  ~PreviewsLitePageRedirectPredictor() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void DidStartNavigation(content::NavigationHandle* handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // network::NetworkQualityTracker::EffectiveConnectionTypeObserver:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType ect) override;

 protected:
  explicit PreviewsLitePageRedirectPredictor(
      content::WebContents* web_contents);

  // Virtual for testing.
  virtual bool DataSaverIsEnabled() const;
  virtual bool ECTIsSlow() const;
  virtual bool PageIsBlacklisted(
      content::NavigationHandle* navigation_handle) const;

  // Returns true when this |web_contents| is the active tab and Chrome is in
  // the foreground. Virtual for testing.
  virtual bool IsVisible() const;

 private:
  friend class content::WebContentsUserData<PreviewsLitePageRedirectPredictor>;
  FRIEND_TEST_ALL_PREFIXES(PreviewsLitePageRedirectPredictorUnitTest,
                           AllConditionsMet_Origin);
  FRIEND_TEST_ALL_PREFIXES(PreviewsLitePageRedirectPredictorUnitTest,
                           AllConditionsMet_Preview);
  FRIEND_TEST_ALL_PREFIXES(PreviewsLitePageRedirectPredictorUnitTest,
                           FeatureDisabled);
  FRIEND_TEST_ALL_PREFIXES(PreviewsLitePageRedirectPredictorUnitTest,
                           DataSaverDisabled);
  FRIEND_TEST_ALL_PREFIXES(PreviewsLitePageRedirectPredictorUnitTest,
                           NoNavigation);
  FRIEND_TEST_ALL_PREFIXES(PreviewsLitePageRedirectPredictorUnitTest,
                           ECTNotSlow);
  FRIEND_TEST_ALL_PREFIXES(PreviewsLitePageRedirectPredictorUnitTest,
                           ECTNotSlowOnPreview);
  FRIEND_TEST_ALL_PREFIXES(PreviewsLitePageRedirectPredictorUnitTest,
                           PageBlacklisted);
  FRIEND_TEST_ALL_PREFIXES(PreviewsLitePageRedirectPredictorUnitTest,
                           NotVisible);
  FRIEND_TEST_ALL_PREFIXES(PreviewsLitePageRedirectPredictorUnitTest,
                           InsecurePage);
  FRIEND_TEST_ALL_PREFIXES(PreviewsLitePageRedirectPredictorUnitTest,
                           ToggleMultipleTimes_Navigations);
  FRIEND_TEST_ALL_PREFIXES(PreviewsLitePageRedirectPredictorUnitTest,
                           ToggleMultipleTimes_ECT);
  FRIEND_TEST_ALL_PREFIXES(PreviewsLitePageRedirectPredictorUnitTest,
                           ToggleMultipleTimes_Visibility);

  // Returns the GURL that should be preresolved, if any.
  base::Optional<GURL> ShouldActOnPage(
      content::NavigationHandle* navigation_handle) const;

  // Toggles preresolving to either start or stop as needed. This operates
  // statelessly, but should be called every time a change potentially needs to
  // be made. See observers below.
  void MaybeToggleTimer(content::NavigationHandle* navigation_handle);

  // Immediately causes |url_| to be preresolved/preconnected.
  void PreresolveOrPreconnect() const;

  // Set if the preview URL is actively being preresolved/preconnected.
  std::unique_ptr<base::RepeatingTimer> timer_;

  // The url that should be preresolved/preconnected, if any.
  base::Optional<GURL> url_;

  // A reference to the DRP Settings.
  data_reduction_proxy::DataReductionProxySettings* drp_settings_;

  // A reference to the PreviewsOptimizationGuide.
  previews::PreviewsOptimizationGuide* opt_guide_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PreviewsLitePageRedirectPredictor);
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_PREDICTOR_H_
