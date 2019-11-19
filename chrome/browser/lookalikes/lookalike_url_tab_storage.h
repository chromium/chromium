// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_TAB_STORAGE_H_
#define CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_TAB_STORAGE_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/common/referrer.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

// A short-lived, per-tab storage for lookalike interstitials.
// Contains an allowlist for bypassing lookalike URL warnings tied to
// web_contents and extra URL parameters for handling interstitial reloads.
// Since lookalike URL interstitials only trigger when site
// engagement is low, Chrome does not need to persist interstitial bypasses
// beyond the life of the web_contents-- if the user proceeds to interact with a
// page, site engagement will go up and the allowlist will be bypassed entirely.
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

  ~LookalikeUrlTabStorage() override;

  static LookalikeUrlTabStorage* GetOrCreate(
      content::WebContents* web_contents);

  bool IsDomainAllowed(const std::string& domain);
  void AllowDomain(const std::string& domain);

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
  std::set<std::string> allowed_domains_;

  // Parameters associated with the currently displayed interstitial. These are
  // cleared immediately on next navigation.
  InterstitialParams interstitial_params_;

  DISALLOW_COPY_AND_ASSIGN(LookalikeUrlTabStorage);
};

#endif  // CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_TAB_STORAGE_H_
