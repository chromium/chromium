// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RLZ_CHROME_RLZ_TRACKER_DELEGATE_H_
#define CHROME_BROWSER_RLZ_CHROME_RLZ_TRACKER_DELEGATE_H_

#include "base/functional/callback.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/rlz/rlz_tracker_delegate.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

// ChromeRLZTrackerDelegate implements RLZTrackerDelegate abstract interface
// and provides access to Chrome features.
class ChromeRLZTrackerDelegate : public rlz::RLZTrackerDelegate {
 public:
  ChromeRLZTrackerDelegate();

  ChromeRLZTrackerDelegate(const ChromeRLZTrackerDelegate&) = delete;
  ChromeRLZTrackerDelegate& operator=(const ChromeRLZTrackerDelegate&) = delete;

  ~ChromeRLZTrackerDelegate() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static bool IsGoogleDefaultSearch(Profile* profile);
  static bool IsGoogleHomepage(Profile* profile);
  static bool IsGoogleInStartpages(Profile* profile);

  // RLZTrackerDelegate implementation.
  void Cleanup() override;
  bool IsOnUIThread() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  bool GetBrand(std::string* brand) override;
  bool IsBrandOrganic(const std::string& brand) override;
  bool GetReactivationBrand(std::string* brand) override;
  bool ShouldEnableZeroDelayForTesting() override;
  bool GetLanguage(std::u16string* language) override;
  bool GetReferral(std::u16string* referral) override;
  bool ClearReferral() override;
  void SetOmniboxSearchCallback(base::OnceClosure callback) override;
  void SetHomepageSearchCallback(base::OnceClosure callback) override;
  void RunHomepageSearchCallback() override;
  bool ShouldUpdateExistingAccessPointRlz() override;

 private:
  // Called when a URL is opened from the Omnibox.
  void OnURLOpenedFromOmnibox(OmniboxLog* log);

  base::OnceClosure on_omnibox_search_callback_;
  base::OnceClosure on_homepage_search_callback_;

  // Subscription for receiving callbacks that a URL was opened from the
  // omnibox.
  base::CallbackListSubscription omnibox_url_opened_subscription_;
};

#endif  // CHROME_BROWSER_RLZ_CHROME_RLZ_TRACKER_DELEGATE_H_
