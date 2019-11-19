// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_PRECONNECT_CLIENT_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_PRECONNECT_CLIENT_H_

#include "base/macros.h"
#include "base/optional.h"
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

class NavigationPredictorPreconnectClient
    : public content::WebContentsObserver,
      public content::WebContentsUserData<NavigationPredictorPreconnectClient> {
 public:
  ~NavigationPredictorPreconnectClient() override;

 private:
  friend class content::WebContentsUserData<
      NavigationPredictorPreconnectClient>;
  explicit NavigationPredictorPreconnectClient(
      content::WebContents* web_contents);

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Returns template URL service. Guaranteed to be non-null.
  bool IsSearchEnginePage() const;

  base::Optional<url::Origin> GetOriginToPreconnect(
      const GURL& document_url) const;

  // MaybePreconnectNow preconnects to an origin server if it's allowed.
  void MaybePreconnectNow();

  // Used to get keyed services.
  content::BrowserContext* const browser_context_;

  // Current visibility state of the web contents.
  content::Visibility current_visibility_;

  // Used to preconnect regularly.
  base::OneShotTimer timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(NavigationPredictorPreconnectClient);
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_PRECONNECT_CLIENT_H_
