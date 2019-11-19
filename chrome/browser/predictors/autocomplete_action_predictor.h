// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_H_
#define CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_H_

#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_table.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "url/gurl.h"

struct AutocompleteMatch;
class AutocompleteResult;
struct OmniboxLog;
class PredictorsHandler;
class Profile;

namespace content {
class SessionStorageNamespace;
}

namespace gfx {
class Size;
}

namespace history {
class URLDatabase;
}

namespace prerender {
class PrerenderHandle;
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
class AutocompleteActionPredictor
    : public KeyedService,
      public content::NotificationObserver,
      public history::HistoryServiceObserver,
      public base::SupportsWeakPtr<AutocompleteActionPredictor> {
 public:
  enum Action {
    ACTION_PRERENDER = 0,
    ACTION_PRECONNECT,
    ACTION_NONE,
    LAST_PREDICT_ACTION = ACTION_NONE
  };

  explicit AutocompleteActionPredictor(Profile* profile);
  ~AutocompleteActionPredictor() override;

  // Registers an AutocompleteResult for a given |user_text|. This will be used
  // when the user navigates from the Omnibox to determine early opportunities
  // to predict their actions.
  void RegisterTransitionalMatches(const base::string16& user_text,
                                   const AutocompleteResult& result);

  // Clears any transitional matches that have been registered. Called when, for
  // example, the OmniboxEditModel is reverted.
  void ClearTransitionalMatches();

  // Return the recommended action given |user_text|, the text the user has
  // entered in the Omnibox, and |match|, the suggestion from Autocomplete.
  // This method uses information from the ShortcutsBackend including how much
  // of the matching entry the user typed, and how long it's been since the user
  // visited the matching URL, to calculate a score between 0 and 1. This score
  // is then mapped to an Action.
  Action RecommendAction(const base::string16& user_text,
                         const AutocompleteMatch& match) const;

  // Begin prerendering |url| with |session_storage_namespace|. The |size| gives
  // the initial size for the target prerender. The predictor will run at most
  // one prerender at a time, so launching a prerender will cancel our previous
  // prerenders (if any).
  void StartPrerendering(
      const GURL& url,
      content::SessionStorageNamespace* session_storage_namespace,
      const gfx::Size& size);

  // Cancels the current prerender, unless it has already been abandoned.
  void CancelPrerender();

  // Return true if the suggestion type warrants a TCP/IP preconnection.
  // i.e., it is now quite likely that the user will select the related domain.
  static bool IsPreconnectable(const AutocompleteMatch& match);

  // Returns true if there is an active Omnibox prerender and it has been
  // abandoned.
  bool IsPrerenderAbandonedForTesting();

  // Should be called when a URL is opened from the omnibox.
  void OnOmniboxOpenedUrl(const OmniboxLog& log);

 private:
  friend class AutocompleteActionPredictorTest;
  friend class ::PredictorsHandler;

  struct TransitionalMatch {
    TransitionalMatch();
    explicit TransitionalMatch(const base::string16 in_user_text);
    TransitionalMatch(const TransitionalMatch& other);
    ~TransitionalMatch();

    base::string16 user_text;
    std::vector<GURL> urls;

    bool operator==(const base::string16& other_user_text) const {
      return user_text == other_user_text;
    }
  };

  struct DBCacheKey {
    base::string16 user_text;
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

  // NotificationObserver
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // The first step in initializing the predictor is accessing the database and
  // building the local cache. This should be delayed until after critical DB
  // and IO processes have completed.
  void CreateLocalCachesFromDatabase();

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

  // Uses local caches to calculate an exact percentage prediction that the user
  // will take a particular match given what they have typed. |is_in_db| is set
  // to differentiate trivial zero results resulting from a match not being
  // found from actual zero results where the calculation returns 0.0.
  double CalculateConfidence(const base::string16& user_text,
                             const AutocompleteMatch& match,
                             bool* is_in_db) const;

  // Calculates the confidence for an entry in the DBCacheMap.
  double CalculateConfidenceForDbEntry(DBCacheMap::const_iterator iter) const;

  // KeyedService:
  void Shutdown() override;

  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;
  void OnHistoryServiceLoaded(
      history::HistoryService* history_service) override;

  Profile* profile_;

  // Set when this is a predictor for an incognito profile.
  AutocompleteActionPredictor* main_profile_predictor_;

  // Set when this is a predictor for a non-incognito profile, and the incognito
  // profile creates a predictor.  If this is non-NULL when we finish
  // initialization, we should call CopyFromMainProfile() on it.
  AutocompleteActionPredictor* incognito_predictor_;

  // The backing data store.  This is NULL for incognito-owned predictors.
  scoped_refptr<AutocompleteActionPredictorTable> table_;

  content::NotificationRegistrar notification_registrar_;

  // This is cleared after every Omnibox navigation.
  std::vector<TransitionalMatch> transitional_matches_;

  // The aggregated size of all user text and GURLs in |transitional_matches_|.
  // This is used to limit the maximum size of |transitional_matches_|.
  size_t transitional_matches_size_ = 0;

  std::unique_ptr<prerender::PrerenderHandle> prerender_handle_;

  // This allows us to predict the effect of confidence threshold changes on
  // accuracy.  This is cleared after every omnibox navigation.
  mutable std::vector<std::pair<GURL, double> > tracked_urls_;

  // Local caches of the data store.  For incognito-owned predictors this is the
  // only copy of the data.
  DBCacheMap db_cache_;
  DBIdCacheMap db_id_cache_;

  bool initialized_;

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(AutocompleteActionPredictor);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_H_
