// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_H_
#define CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_table.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

struct AutocompleteMatch;
class AutocompleteResult;
struct OmniboxLog;
class PredictorsHandler;
class Profile;

namespace gfx {
class Size;
}

namespace history {
class URLDatabase;
}

namespace predictors {
// This class is responsible for determining the correct predictive network
// action to take given for a given AutocompleteMatch and entered text. It can
// be instantiated for both normal and incognito profiles.  For normal profiles,
// it uses an AutocompleteActionPredictorTable accessed asynchronously on the DB
// thread to permanently store the data used to make predictions, and keeps
// local caches of that data to be able to make predictions synchronously on the
// UI thread where it lives.  For incognito profiles, there is no table; the
// local caches are copied from the main profile at creation and from there on
// are the only thing used.
//
// This class can be accessed as a weak pointer so that it can safely use
// PostTaskAndReply without fear of crashes if it is destroyed before the reply
// triggers. This is necessary during initialization.
class AutocompleteActionPredictor : public KeyedService,
                                    public history::HistoryServiceObserver {
 public:
  // An `Action` is a recommendation on what pre* technology to invoke on a
  // given `AutocompleteMatch`.
  enum Action {
    // Trigger Prerendering.
    ACTION_PRERENDER = 0,

    // Invoke `LoadingPredictor::PrepareForPageLoad` to
    // prefetch, preconnect, and preresolve.
    ACTION_PRECONNECT,

    // The recommendation is to not perform any action.
    ACTION_NONE,
  };

  explicit AutocompleteActionPredictor(Profile* profile);

  AutocompleteActionPredictor(const AutocompleteActionPredictor&) = delete;
  AutocompleteActionPredictor& operator=(const AutocompleteActionPredictor&) =
      delete;

  ~AutocompleteActionPredictor() override;

  class Observer : public base::CheckedObserver {
   public:
    // Called once per FinishInitialization() call.
    virtual void OnInitialized() {}
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Registers an AutocompleteResult for a given |user_text|. This will be used
  // when the user navigates from the Omnibox to determine early opportunities
  // to predict their actions.
  void RegisterTransitionalMatches(const std::u16string& user_text,
                                   const AutocompleteResult& result);

  // Updates the database using the current transitional matches, given the URL
  // the user navigated to (or an empty URL if the user did not navigate). This
  // clears the transitional matches.
  void UpdateDatabaseFromTransitionalMatches(const GURL& opened_url);

  // Clears any transitional matches that have been registered. Called when, for
  // example, the OmniboxEditModel is reverted.
  void ClearTransitionalMatches();

  // Returns the recommended action given |user_text|, the text the user has
  // entered in the Omnibox associated with |web_contents|, and |match|, the
  // suggestion from Autocomplete. This method uses information from the
  // ShortcutsBackend including how much of the matching entry the user typed,
  // and how long it's been since the user visited the matching URL, to
  // calculate a score between 0 and 1. This score is then mapped to an Action.
  Action RecommendAction(const std::u16string& user_text,
                         const AutocompleteMatch& match,
                         content::WebContents* web_contents) const;

  // Begins prerendering or prefetch with `url`. The `size` gives the initial
  // size for the target prefetch. The predictor will run at most one prerender
  // at a time, so launching a prerender will cancel our previous prerenders (if
  // any).
  void StartPrerendering(const GURL& url,
                         content::WebContents& web_contents,
                         const gfx::Size& size);

  // Returns true if the suggestion type warrants a TCP/IP preconnection.
  // i.e., it is now quite likely that the user will select the related domain.
  static bool IsPreconnectable(const AutocompleteMatch& match);

  // Should be called when a URL is opened from the omnibox.
  void OnOmniboxOpenedUrl(const OmniboxLog& log);

  // Uses local caches to calculate an exact percentage prediction that the user
  // will take a particular match given what they have typed.
  double CalculateConfidence(const std::u16string& user_text,
                             const AutocompleteMatch& match) const;

  bool initialized() { return initialized_; }

  static Action DecideActionByConfidence(double confidence);

 private:
  friend class AutocompleteActionPredictorTest;
  friend class ::PredictorsHandler;

  struct TransitionalMatch {
    TransitionalMatch();
    explicit TransitionalMatch(const std::u16string in_user_text);
    TransitionalMatch(const TransitionalMatch& other);
    ~TransitionalMatch();

    std::u16string user_text;
    std::vector<GURL> urls;

    bool operator==(const std::u16string& other_user_text) const {
      return user_text == other_user_text;
    }
  };

  struct DBCacheKey {
    std::u16string user_text;
    GURL url;

    bool operator<(const DBCacheKey& rhs) const {
      return std::tie(user_text, url) < std::tie(rhs.user_text, rhs.url);
    }

    bool operator==(const DBCacheKey& rhs) const {
      return (user_text == rhs.user_text) && (url == rhs.url);
    }
  };

  struct DBCacheValue {
    int number_of_hits;
    int number_of_misses;
  };

  typedef std::map<DBCacheKey, DBCacheValue> DBCacheMap;
  typedef std::map<DBCacheKey, AutocompleteActionPredictorTable::Row::Id>
      DBIdCacheMap;

  static const int kMaximumDaysToKeepEntry;
  static const size_t kMinimumUserTextLength;
  static const size_t kMaximumStringLength;

  // Removes all rows from the database and caches.
  void DeleteAllRows();

  // Removes rows that contain a URL in |rows| from the local caches.
  // |id_list| must not be nullptr. Every row id deleted will be added to
  // |id_list|.
  void DeleteRowsFromCaches(
      const history::URLRows& rows,
      std::vector<AutocompleteActionPredictorTable::Row::Id>* id_list);

  // Adds and updates rows in the database and caches.
  void AddAndUpdateRows(
    const AutocompleteActionPredictorTable::Rows& rows_to_add,
    const AutocompleteActionPredictorTable::Rows& rows_to_update);

  // Called to populate the local caches. This also calls DeleteOldEntries
  // if the history service is available, or registers for the notification of
  // it becoming available.
  void CreateCaches(
      std::unique_ptr<std::vector<AutocompleteActionPredictorTable::Row>> rows);

  // Attempts to call DeleteOldEntries if the in-memory database has been loaded
  // by |service|.
  void TryDeleteOldEntries(history::HistoryService* service);

  // Called to delete any old or invalid entries from the database. Called after
  // the local caches are created once the history service is available.
  void DeleteOldEntries(history::URLDatabase* url_db);

  // Deletes any old or invalid entries from the local caches. |url_db| and
  // |id_list| must not be nullptr. Every row id deleted will be added to
  // |id_list|.
  void DeleteOldIdsFromCaches(
      history::URLDatabase* url_db,
      std::vector<AutocompleteActionPredictorTable::Row::Id>* id_list);

  // Deletes up to |count| rows having lowest confidence scores from the local
  // caches. Deleted row ids will be added to |id_list|.
  void DeleteLowestConfidenceRowsFromCaches(
      size_t count,
      std::vector<AutocompleteActionPredictorTable::Row::Id>* id_list);

  // Called on an incognito-owned predictor to copy the current caches from the
  // main profile.
  void CopyFromMainProfile();

  // Registers for notifications and sets the |initialized_| flag.
  void FinishInitialization();

  // Calculates the confidence for an entry in the DBCacheMap.
  double CalculateConfidenceForDbEntry(DBCacheMap::const_iterator iter) const;

  // KeyedService:
  void Shutdown() override;

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;
  void OnHistoryServiceLoaded(
      history::HistoryService* history_service) override;

  raw_ptr<Profile> profile_ = nullptr;

  // Set when this is a predictor for an incognito profile.
  raw_ptr<AutocompleteActionPredictor> main_profile_predictor_ = nullptr;

  // Set when this is a predictor for a non-incognito profile, and the incognito
  // profile creates a predictor.  If this is non-NULL when we finish
  // initialization, we should call CopyFromMainProfile() on it.
  raw_ptr<AutocompleteActionPredictor> incognito_predictor_ = nullptr;

  // The backing data store.  This is nullptr for incognito-owned predictors.
  scoped_refptr<AutocompleteActionPredictorTable> table_;

  // This is cleared after every Omnibox navigation.
  std::vector<TransitionalMatch> transitional_matches_;

  // The aggregated size of all user text and GURLs in |transitional_matches_|.
  // This is used to limit the maximum size of |transitional_matches_|.
  size_t transitional_matches_size_ = 0;

  base::WeakPtr<content::PrerenderHandle> direct_url_input_prerender_handle_;

  // Local caches of the data store.  For incognito-owned predictors this is the
  // only copy of the data.
  DBCacheMap db_cache_;
  DBIdCacheMap db_id_cache_;

  bool initialized_ = false;

  base::ObserverList<Observer> observers_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  base::WeakPtrFactory<AutocompleteActionPredictor> weak_ptr_factory_{this};
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_H_
