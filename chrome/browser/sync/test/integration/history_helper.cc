// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/history_helper.h"

#include <ostream>
#include <utility>

#include "chrome/browser/sync/test/integration/typed_urls_helper.h"
#include "components/sync/protocol/history_specifics.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"

namespace sync_pb {

// Makes the GMock matchers print out a readable version of the protobuf.
void PrintTo(const HistorySpecifics& history, std::ostream* os) {
  base::Value serialized = syncer::HistorySpecificsToValue(history);
  *os << serialized;
}

}  // namespace sync_pb

namespace history {

// Makes the GMock matchers print out a readable version of a VisitRow.
void PrintTo(const VisitRow& row, std::ostream* os) {
  *os << "[ VisitID: " << row.visit_id << ", Duration: " << row.visit_duration
      << " ]";
}

}  // namespace history

namespace history_helper {

namespace {

std::vector<sync_pb::HistorySpecifics> SyncEntitiesToHistorySpecifics(
    std::vector<sync_pb::SyncEntity> entities) {
  std::vector<sync_pb::HistorySpecifics> history;
  for (sync_pb::SyncEntity& entity : entities) {
    DCHECK(entity.specifics().has_history());
    history.push_back(std::move(entity.specifics().history()));
  }
  return history;
}

}  // namespace

LocalHistoryMatchChecker::LocalHistoryMatchChecker(
    int profile_index,
    syncer::SyncServiceImpl* service,
    const std::map<GURL, Matcher>& matchers)
    : SingleClientStatusChangeChecker(service),
      profile_index_(profile_index),
      matchers_(matchers) {}

LocalHistoryMatchChecker::~LocalHistoryMatchChecker() = default;

bool LocalHistoryMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  for (const auto& [url, matcher] : matchers_) {
    history::VisitVector visits =
        typed_urls_helper::GetVisitsForURLFromClient(profile_index_, url);
    testing::StringMatchResultListener result_listener;
    const bool matches =
        testing::ExplainMatchResult(matcher, visits, &result_listener);
    *os << result_listener.str();
    if (!matches) {
      return false;
    }
  }
  return true;
}

void LocalHistoryMatchChecker::OnSyncCycleCompleted(syncer::SyncService* sync) {
  CheckExitCondition();
}

ServerHistoryMatchChecker::ServerHistoryMatchChecker(const Matcher& matcher)
    : matcher_(matcher) {}

ServerHistoryMatchChecker::~ServerHistoryMatchChecker() = default;

void ServerHistoryMatchChecker::OnCommit(
    const std::string& committer_invalidator_client_id,
    syncer::ModelTypeSet committed_model_types) {
  if (committed_model_types.Has(syncer::HISTORY)) {
    CheckExitCondition();
  }
}

bool ServerHistoryMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  std::vector<sync_pb::HistorySpecifics> entities =
      SyncEntitiesToHistorySpecifics(
          fake_server()->GetSyncEntitiesByModelType(syncer::HISTORY));

  testing::StringMatchResultListener result_listener;
  const bool matches =
      testing::ExplainMatchResult(matcher_, entities, &result_listener);
  *os << result_listener.str();
  return matches;
}

}  // namespace history_helper
