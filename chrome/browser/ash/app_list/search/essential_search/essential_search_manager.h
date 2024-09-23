// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_ESSENTIAL_SEARCH_ESSENTIAL_SEARCH_MANAGER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_ESSENTIAL_SEARCH_ESSENTIAL_SEARCH_MANAGER_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/search/essential_search/socs_cookie_fetcher.h"
#include "components/prefs/pref_change_registrar.h"
#include "net/base/backoff_entry.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"

class Profile;

namespace app_list {

// This class is responsible for fetching SOCS cookie and adding it to the user
// cookie jar to make sure that search done through google.com would only use
// essential cookie and data.
// EssentialSearchManager is still WIP.
class EssentialSearchManager : public ash::SessionObserver,
                               public SocsCookieFetcher::Consumer {
 public:
  // Backoff policy for socs cookie fetch retry attempts in case cookie fetch
  // failed or returned invalid data.
  static const net::BackoffEntry::Policy kFetchSocsCookieRetryBackoffPolicy;

  explicit EssentialSearchManager(Profile* primary_profile);
  ~EssentialSearchManager() override;

  // Disallow copy and assign.
  EssentialSearchManager(const EssentialSearchManager&) = delete;
  EssentialSearchManager& operator=(const EssentialSearchManager&) = delete;

  // Returns instance of EssentialSearchManager.
  static std::unique_ptr<EssentialSearchManager> Create(
      Profile* primary_profile);

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // SocsCookieFetcher::Consumer
  void OnCookieFetched(const std::string& socs_cookie) override;
  void OnApiCallFailed(SocsCookieFetcher::Status status) override;

  // Returns whether search suggest should be disabled.
  // Search suggestions will be temporarily disabled until SOCS cookie is
  // fetched. This affects only managed users on chromeos that have
  // EssentialSearchEnabled policy set.
  bool ShouldDisableSearchSuggest() const;

  void set_cookie_insertion_closure_for_test(
      base::OnceClosure cookie_insertion_closure_for_test) {
    cookie_insertion_closure_for_test_ =
        std::move(cookie_insertion_closure_for_test);
  }

  void set_cookie_deletion_closure_for_test(
      base::OnceClosure cookie_deletion_closure_for_test) {
    cookie_deletion_closure_for_test_ =
        std::move(cookie_deletion_closure_for_test);
  }

 private:
  void MaybeFetchSocsCookie();

  // Sets flag that control search suggest.
  void MaybeDisableSearchSuggest();

  // Callback function to be called after Cookies are retrieved from user's
  // profile.
  void OnCookiesRetrieved(const net::CookieAccessResultList& list,
                          const net::CookieAccessResultList& excluded_list);

  // Callback function to be called after when a SOCS cookie is added to a
  // user's profile.
  void OnCookieAddedToUserProfile(net::CookieAccessResult result);

  void RemoveSocsCookie();

  void OnCookieDeleted(uint32_t number_of_cookies_deleted);

  // Refetch after given `delay`.
  void RefetchAfter(base::TimeDelta delay);

  // Cancel all active requests
  void CancelPendingRequests();

  // Flag to disable search suggest while fetching SOCS cookie.
  bool temporary_disable_search_suggest_ = false;

  // Observer for EssentialSearch-related prefs.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Used to notify the test that the cookie was inserted in the user's profile.
  base::OnceClosure cookie_insertion_closure_for_test_;

  // Used to notify the test that the cookie was deleted from the user's
  // profile.
  base::OnceClosure cookie_deletion_closure_for_test_;

  const raw_ptr<Profile> primary_profile_;

  std::unique_ptr<SocsCookieFetcher> socs_cookie_fetcher_;

  net::BackoffEntry retry_backoff_;

  base::WeakPtrFactory<EssentialSearchManager> weak_ptr_factory_{this};

  base::WeakPtrFactory<EssentialSearchManager> fetch_requests_weak_factory_{
      this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_ESSENTIAL_SEARCH_ESSENTIAL_SEARCH_MANAGER_H_
