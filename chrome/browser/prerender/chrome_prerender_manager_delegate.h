// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_CHROME_PRERENDER_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_PRERENDER_CHROME_PRERENDER_MANAGER_DELEGATE_H_

#include "chrome/browser/net/prediction_options.h"
#include "components/prerender/browser/prerender_manager_delegate.h"
#include "components/prerender/common/prerender_origin.h"

class Profile;

namespace content_settings {
class CookieSettings;
}

namespace prerender {

class ChromePrerenderManagerDelegate : public PrerenderManagerDelegate {
 public:
  explicit ChromePrerenderManagerDelegate(Profile* profile);
  ~ChromePrerenderManagerDelegate() override = default;

  // PrerenderManagerDelegate overrides.
  scoped_refptr<content_settings::CookieSettings> GetCookieSettings() override;
  void MaybePreconnect(const GURL& url) override;
  std::unique_ptr<PrerenderContentsDelegate> GetPrerenderContentsDelegate()
      override;
  bool IsNetworkPredictionPreferenceEnabled() override;
  bool IsPredictionDisabledDueToNetwork(Origin origin) override;
  std::string GetReasonForDisablingPrediction() override;

 private:
  chrome_browser_net::NetworkPredictionStatus GetPredictionStatus() const;
  Profile* profile_;
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_CHROME_PRERENDER_MANAGER_DELEGATE_H_
