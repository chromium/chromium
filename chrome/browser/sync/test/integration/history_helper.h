// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_HISTORY_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_HISTORY_HELPER_H_

#include <map>
#include <vector>

#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "components/history/core/browser/history_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace history {
void PrintTo(const VisitRow& row, std::ostream* os);
}  // namespace history

namespace sync_pb {
void PrintTo(const HistorySpecifics& history, std::ostream* os);
}  // namespace sync_pb

namespace history_helper {

// Matchers for sync_pb::HistorySpecifics.

MATCHER_P(UrlIs, url, "") {
  if (arg.redirect_entries_size() != 1) {
    return false;
  }
  return arg.redirect_entries(0).url() == url;
}

MATCHER_P2(UrlsAre, url1, url2, "") {
  if (arg.redirect_entries_size() != 2) {
    return false;
  }
  return arg.redirect_entries(0).url() == url1 &&
         arg.redirect_entries(1).url() == url2;
}

MATCHER_P(CoreTransitionIs, transition, "") {
  return arg.page_transition().core_transition() == transition;
}

MATCHER(IsChainStart, "") {
  return !arg.redirect_chain_start_incomplete();
}

MATCHER(IsChainEnd, "") {
  return !arg.redirect_chain_end_incomplete();
}

MATCHER(HasReferringVisit, "") {
  return arg.originator_referring_visit_id() != 0;
}

MATCHER(HasOpenerVisit, "") {
  return arg.originator_opener_visit_id() != 0;
}

MATCHER(HasReferrerURL, "") {
  return !arg.referrer_url().empty();
}

MATCHER_P(ReferrerURLIs, referrer_url, "") {
  return arg.referrer_url() == referrer_url;
}

MATCHER(HasVisitDuration, "") {
  return arg.visit_duration_micros() > 0;
}

MATCHER(HasHttpResponseCode, "") {
  return arg.http_response_code() > 0;
}

MATCHER(StandardFieldsArePopulated, "") {
  // Checks all fields that should never be empty/unset/default. Some fields can
  // be legitimately empty, or are set after an entity is first created.
  // May be legitimately empty:
  //   redirect_entries.title (may simply be empty)
  //   redirect_entries.redirect_type (empty if it's not a redirect)
  //   originator_referring_visit_id, originator_opener_visit_id (may not exist)
  //   root_task_id, parent_task_id (not always set)
  //   http_response_code (unset for replaced navigations)
  // Populated later:
  //   visit_duration_micros, page_language, password_state
  return arg.visit_time_windows_epoch_micros() > 0 &&
         !arg.originator_cache_guid().empty() &&
         arg.redirect_entries_size() > 0 &&
         arg.redirect_entries(0).originator_visit_id() > 0 &&
         !arg.redirect_entries(0).url().empty() && arg.has_browser_type() &&
         arg.window_id() > 0 && arg.tab_id() > 0 && arg.task_id() > 0;
}

// Matchers for history::VisitRow.

MATCHER_P(VisitRowIdIs, visit_id, "") {
  return arg.visit_id == visit_id;
}

MATCHER(VisitRowHasDuration, "") {
  return !arg.visit_duration.is_zero();
}

MATCHER_P(VisitRowDurationIs, duration, "") {
  return arg.visit_duration == duration;
}

// Gets the URLRow for a specific URL from a specific sync profile. Returns
// false if the URL was not found in the history DB.
bool GetUrlFromClient(int index, const GURL& url, history::URLRow* row);

// Similar, but queries by URL ID rather than URL.
bool GetUrlFromClient(int index, history::URLID url_id, history::URLRow* row);

// Gets the visits for a URL from a specific sync profile.
history::VisitVector GetVisitsFromClient(int index, history::URLID id);

// Gets the visits for a URL from a specific sync profile. Like above, but
// takes a GURL instead of URLID. Returns empty vector if |url| is not returned
// by GetUrlFromClient().
history::VisitVector GetVisitsForURLFromClient(int index, const GURL& url);

// As above, but return `AnnotatedVisit` instead of just `VisitRow`.
std::vector<history::AnnotatedVisit> GetAnnotatedVisitsFromClient(
    int index,
    history::URLID id);
std::vector<history::AnnotatedVisit> GetAnnotatedVisitsForURLFromClient(
    int index,
    const GURL& url);

history::VisitVector GetRedirectChainFromClient(int index,
                                                history::VisitRow final_visit);

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

// A helper class that waits for entries in the local history DB that match the
// given matchers.
// Note that this only checks URLs that were passed in - any additional URLs in
// the DB (and their corresponding visits) are ignored.
class LocalHistoryMatchChecker : public SingleClientStatusChangeChecker {
 public:
  using Matcher = testing::Matcher<std::vector<history::VisitRow>>;

  explicit LocalHistoryMatchChecker(int profile_index,
                                    syncer::SyncServiceImpl* service,
                                    const std::map<GURL, Matcher>& matchers);
  ~LocalHistoryMatchChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // syncer::SyncServiceObserver implementation.
  void OnSyncCycleCompleted(syncer::SyncService* sync) override;

 private:
  const int profile_index_;
  const std::map<GURL, Matcher> matchers_;
};

// A helper class that waits for the HISTORY entities on the FakeServer to match
// a given GMock matcher.
class ServerHistoryMatchChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  using Matcher = testing::Matcher<std::vector<sync_pb::HistorySpecifics>>;

  explicit ServerHistoryMatchChecker(const Matcher& matcher);
  ~ServerHistoryMatchChecker() override;
  ServerHistoryMatchChecker(const ServerHistoryMatchChecker&) = delete;
  ServerHistoryMatchChecker& operator=(const ServerHistoryMatchChecker&) =
      delete;

  // FakeServer::Observer overrides.
  void OnCommit(syncer::DataTypeSet committed_data_types) override;

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const Matcher matcher_;
};

}  // namespace history_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_HISTORY_HELPER_H_
