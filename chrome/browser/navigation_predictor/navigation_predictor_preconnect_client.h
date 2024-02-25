// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_PRECONNECT_CLIENT_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_PRECONNECT_CLIENT_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

class NavigationPredictorKeyedService;

class NavigationPredictorPreconnectClient
    : public content::WebContentsObserver,
      public content::WebContentsUserData<NavigationPredictorPreconnectClient> {
 public:
  NavigationPredictorPreconnectClient(
      const NavigationPredictorPreconnectClient&) = delete;
  NavigationPredictorPreconnectClient& operator=(
      const NavigationPredictorPreconnectClient&) = delete;

  ~NavigationPredictorPreconnectClient() override;

  static void EnablePreconnectsForLocalIPsForTesting(
      bool enable_preconnects_for_local_ips) {
    enable_preconnects_for_local_ips_for_testing_ =
        enable_preconnects_for_local_ips;
  }

 private:
  friend class content::WebContentsUserData<
      NavigationPredictorPreconnectClient>;
  explicit NavigationPredictorPreconnectClient(
      content::WebContents* web_contents);

  NavigationPredictorKeyedService* GetNavigationPredictorKeyedService() const;

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Returns template URL service. Guaranteed to be non-null.
  bool IsSearchEnginePage() const;

  std::optional<url::Origin> GetOriginToPreconnect(
      const GURL& document_url) const;

  // MaybePreconnectNow preconnects to an origin server if it's allowed.
  void MaybePreconnectNow(size_t preconnects_attempted);

  // Returns true if the origin is publicly routable.
  std::optional<bool> IsPubliclyRoutable(
      content::NavigationHandle* navigation_handle) const;

  // Used to get keyed services.
  const raw_ptr<content::BrowserContext> browser_context_;

  // Set to true only if preconnects are allowed to local IPs. Defaulted to
  // false. Set to true only for testing.
  static bool enable_preconnects_for_local_ips_for_testing_;

  // Current visibility state of the web contents.
  content::Visibility current_visibility_;

  // Used to preconnect regularly.
  base::OneShotTimer timer_;

  // Set to true if the origin is publicly routable.
  bool is_publicly_routable_ = true;

  SEQUENCE_CHECKER(sequence_checker_);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_PRECONNECT_CLIENT_H_
