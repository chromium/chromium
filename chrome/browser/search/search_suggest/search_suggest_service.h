// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SEARCH_SUGGEST_SEARCH_SUGGEST_SERVICE_H_
#define CHROME_BROWSER_SEARCH_SEARCH_SUGGEST_SEARCH_SUGGEST_SERVICE_H_

#include <memory>
#include <string>

#include "base/observer_list.h"
#include "base/optional.h"
#include "chrome/browser/search/search_suggest/search_suggest_data.h"
#include "chrome/browser/search/search_suggest/search_suggest_loader.h"
#include "chrome/browser/search/search_suggest/search_suggest_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

class Profile;

namespace signin {
class IdentityManager;
}  // namespace signin

// A service that downloads, caches, and hands out SearchSuggestData. It never
// initiates a download automatically, only when Refresh is called. When the
// user signs in or out, the cached value is cleared.
class SearchSuggestService : public KeyedService {
 public:
  // Search suggestions should be disabled when on-focus zero-prefix suggestions
  // are displaying in the NTP. Returns false if omnibox::kZeroSuggestionsOnNTP
  // or omnibox::kZeroSuggestionsOnNTPRealboxkNtpRealbox are enabled; or
  // omnibox::kOnFocusSuggestions is enabled and configured to show suggestions
  // of some type in the NTP Omnibox or Realbox.
  static bool IsEnabled();

  SearchSuggestService(Profile* profile,
                       signin::IdentityManager* identity_manager,
                       std::unique_ptr<SearchSuggestLoader> loader);
  ~SearchSuggestService() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Returns the currently cached SearchSuggestData, if any.
  // Virtual for testing.
  virtual const base::Optional<SearchSuggestData>& search_suggest_data() const;

  virtual const SearchSuggestLoader::Status& search_suggest_status() const;

  // Determines if a request for search suggestions should be made. If a request
  // should not be made immediately call SearchSuggestDataLoaded with the
  // reason. Otherwise requests an asynchronous refresh from the network. After
  // the update completes, regardless of success, OnSearchSuggestDataUpdated
  // will be called on the observers.
  // Virtual for testing.
  virtual void Refresh();

  // Add/remove observers. All observers must unregister themselves before the
  // SearchSuggestService is destroyed.
  void AddObserver(SearchSuggestServiceObserver* observer);
  void RemoveObserver(SearchSuggestServiceObserver* observer);

  // Register prefs associated with the NTP.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Add the task_id to the blocklist stored in user prefs. Overrides any
  // existing entry for the given task_id.
  //
  // A task_id represents a category of searches such as "Camping", a
  // task_version represents the selection criteria used to generate the
  // suggestion.
  void BlocklistSearchSuggestion(int task_version, long task_id);

  // Add the hash to the list of hashes for the task_id. Stored as a
  // dict of task_ids to lists of hashes in user prefs.
  //
  // A task_id represents a category of searches such as "Camping", a hash
  // is a specific search within the category such as "Camping equipment", and
  // a task_version represents the selection criteria used ti generate the
  // suggestion.
  void BlocklistSearchSuggestionWithHash(int task_version,
                                         long task_id,
                                         const uint8_t hash[4]);

  // Issue a new request with the selected suggestion appended to the blocklist
  // but NOT stored in user prefs.  This prevents a race condition where the
  // request completes before the data server side is updated to reflect the
  // selection, resulting in the same suggestion appearing in the next set.
  void SearchSuggestionSelected(int task_version,
                                long task_id,
                                const uint8_t hash[4]);

  // Opt the current profile out of seeing search suggestions. Requests will
  // no longer be made.
  void OptOutOfSearchSuggestions();

  SearchSuggestLoader* loader_for_testing() { return loader_.get(); }

  // Returns the string representation of the suggestions blocklist in the form:
  // "task_id1:hash1,hash2,hash3;task_id2;task_id3:hash1,hash2".
  std::string GetBlocklistAsString();

  // Called when suggestions are displayed on the NTP, clears the cached data
  // and updates timestamps and impression counts.
  // Virtual for testing.
  virtual void SuggestionsDisplayed();

 protected:
  // Called when a Refresh() is requested. If |status|==OK, |data| will contain
  // the fetched data. Otherwise |data| will be nullopt and |status| will
  // indicate if the request failed or the reason it was not sent.
  //
  // If the |status|==FATAL_ERROR freeze future requests until the request
  // freeze interval has elapsed.
  void SearchSuggestDataLoaded(SearchSuggestLoader::Status status,
                               const base::Optional<SearchSuggestData>& data);

 private:
  class SigninObserver;

  void SigninStatusChanged();

  // Either calls SearchSuggestLoader::Load with |blocklist| or immediately
  // calls SearchSuggestDataLoaded with the reason a request was not made.
  void MaybeLoadWithBlocklist(const std::string& blocklist);

  void NotifyObservers();

  // Returns true if the number of impressions has reached the maxmium allowed
  // for the impression interval (e.g. 4 impressions / 12 hours), and false
  // otherwise.
  bool ImpressionCapReached();

  // Returns true if requests are frozen and request freeze time interval has
  // not elapsed, false otherwise.
  //
  // Requests are frozen on any request that results in a FATAL_ERROR.
  bool RequestsFrozen();

  std::unique_ptr<SearchSuggestLoader> loader_;

  std::unique_ptr<SigninObserver> signin_observer_;

  Profile* profile_;

  base::ObserverList<SearchSuggestServiceObserver, true>::Unchecked observers_;

  base::Optional<SearchSuggestData> search_suggest_data_;

  SearchSuggestLoader::Status search_suggest_status_;
};

#endif  // CHROME_BROWSER_SEARCH_SEARCH_SUGGEST_SEARCH_SUGGEST_SERVICE_H_
