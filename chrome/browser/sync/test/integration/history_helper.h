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
  void OnCommit(const std::string& committer_invalidator_client_id,
                syncer::ModelTypeSet committed_model_types) override;

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const Matcher matcher_;
};

}  // namespace history_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_HISTORY_HELPER_H_
