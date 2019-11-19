// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_TYPED_URLS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_TYPED_URLS_HELPER_H_

#include <string>
#include <vector>

#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "components/history/core/browser/history_types.h"
#include "ui/base/page_transition_types.h"

namespace base {
class Time;
}

namespace typed_urls_helper {

// Gets the typed URLs from a specific sync profile.
history::URLRows GetTypedUrlsFromClient(int index);

// Gets a specific url from a specific sync profile. Returns false if the URL
// was not found in the history DB.
bool GetUrlFromClient(int index, const GURL& url, history::URLRow* row);

// Gets the visits for a URL from a specific sync profile.
history::VisitVector GetVisitsFromClient(int index, history::URLID id);

// Gets the visits for a URL from a specific sync profile. Like above, but
// takes a GURL instead of URLID. Returns empty vector if |url| is not returned
// by GetUrlFromClient().
history::VisitVector GetVisitsForURLFromClient(int index, const GURL& url);

// Removes the passed |visits| from a specific sync profile.
void RemoveVisitsFromClient(int index, const history::VisitVector& visits);

// Adds a URL to the history DB for a specific sync profile (just registers a
// new visit if the URL already exists) using a TYPED PageTransition.
void AddUrlToHistory(int index, const GURL& url);

// Adds a URL to the history DB for a specific sync profile (just registers a
// new visit if the URL already exists), using the passed PageTransition.
void AddUrlToHistoryWithTransition(int index,
                                   const GURL& url,
                                   ui::PageTransition transition,
                                   history::VisitSource source);

// Adds a URL to the history DB for a specific sync profile (just registers a
// new visit if the URL already exists), using the passed PageTransition and
// timestamp.
void AddUrlToHistoryWithTimestamp(int index,
                                  const GURL& url,
                                  ui::PageTransition transition,
                                  history::VisitSource source,
                                  const base::Time& timestamp);

// Expires all visits before |end_time| from the History DB for the sync profile
// at |index|. This mimicks the automatic expiration of old history.
void ExpireHistoryBefore(int index, base::Time end_time);

// Expires all visits between |begin_time| and |end_time|. This mimicks explicit
// removal of browsing data by the user.
void ExpireHistoryBetween(int index,
                          base::Time begin_time,
                          base::Time end_time);

// Deletes a URL from the history DB for a specific sync profile.
void DeleteUrlFromHistory(int index, const GURL& url);

// Deletes a list of URLs from the history DB for a specific sync
// profile.
void DeleteUrlsFromHistory(int index, const std::vector<GURL>& urls);

// Modifies an URL stored in history by setting a new title.
void SetPageTitle(int index, const GURL& url, const std::string& title);

// Returns true if all clients have the same typed URLs.
bool CheckAllProfilesHaveSameTypedURLs();

// Return true if there is sync metadata for the given typed |url| in the given
// sync profile.
bool CheckSyncHasURLMetadata(int index, const GURL& url);

// Return true if there is sync metadata for typed url with |url_id| in the
// given sync profile.
bool CheckSyncHasMetadataForURLID(int index, history::URLID url_id);

// Checks that the two vectors contain the same set of URLRows (possibly in
// a different order) w.r.t. typed URL sync.
bool CheckURLRowVectorsAreEqualForTypedURLs(const history::URLRows& left,
                                            const history::URLRows& right);

// Checks that the passed URLRows are equivalent w.r.t. typed URL sync.
bool CheckURLRowsAreEqualForTypedURLs(const history::URLRow& left,
                                      const history::URLRow& right);

// Returns true if two sets of visits are equivalent.
bool AreVisitsEqual(const history::VisitVector& visit1,
                    const history::VisitVector& visit2);

// Returns true if there are no duplicate visit times.
bool AreVisitsUnique(const history::VisitVector& visits);

// Returns a unique timestamp to use when generating page visits
// (HistoryService does not like having identical timestamps and will modify
// the timestamps behind the scenes if it encounters them, which leads to
// spurious test failures when the resulting timestamps aren't what we
// expect).
base::Time GetTimestamp();

}  // namespace typed_urls_helper

// Checker that blocks until all clients have the same Typed URLs.
class ProfilesHaveSameTypedURLsChecker : public MultiClientStatusChangeChecker {
 public:
  ProfilesHaveSameTypedURLsChecker();

  // Implementation of StatusChangeChecker.
  bool IsExitConditionSatisfied(std::ostream* os) override;
};

class TypedURLChecker : public SingleClientStatusChangeChecker {
 public:
  TypedURLChecker(int index, const std::string& url);
  ~TypedURLChecker() override;

  // StatusChangeChecker implementation
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  int index_;
  const std::string url_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_TYPED_URLS_HELPER_H_
