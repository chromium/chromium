// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_TAB_STORAGE_H_
#define CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_TAB_STORAGE_H_

#include <set>
#include <vector>

#include "base/supports_user_data.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/common/referrer.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

// A short-lived, per-tab storage for lookalike interstitials, containing
// extra URL parameters for handling interstitial reloads.
class LookalikeUrlTabStorage : public base::SupportsUserData::Data {
 public:
  struct InterstitialParams {
    // We store a limited amount of information here. This might not be
    // sufficient to construct the original navigation in some edge cases (e.g.
    // POST'd to the lookalike URL, which then redirected). However, the
    // original navigation will be blocked with an interstitial, so this is an
    // acceptable compromise.
    GURL url;
    std::vector<GURL> redirect_chain;
    content::Referrer referrer;

    InterstitialParams();
    InterstitialParams(const InterstitialParams& other);
    ~InterstitialParams();
  };

  LookalikeUrlTabStorage();

  LookalikeUrlTabStorage(const LookalikeUrlTabStorage&) = delete;
  LookalikeUrlTabStorage& operator=(const LookalikeUrlTabStorage&) = delete;

  ~LookalikeUrlTabStorage() override;

  static LookalikeUrlTabStorage* GetOrCreate(
      content::WebContents* web_contents);

  // Stores parameters associated with a lookalike interstitial. Must be called
  // when a lookalike interstitial is shown.
  void OnLookalikeInterstitialShown(const GURL& url,
                                    const content::Referrer& referrer,
                                    const std::vector<GURL>& redirect_chain);
  // Clears stored parameters associated with a lookalike interstitial.
  void ClearInterstitialParams();
  // Returns currently stored parameters associated with a lookalike
  // interstitial.
  InterstitialParams GetInterstitialParams() const;

 private:
  // Parameters associated with the currently displayed interstitial. These are
  // cleared immediately on next navigation.
  InterstitialParams interstitial_params_;
};

#endif  // CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_TAB_STORAGE_H_
