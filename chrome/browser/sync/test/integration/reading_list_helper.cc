// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/reading_list_helper.h"

#include <utility>

#include "components/reading_list/core/reading_list_model.h"
#include "components/sync/base/time.h"
#include "url/gurl.h"

namespace reading_list_helper {

void WaitForReadingListModelLoaded(ReadingListModel* reading_list_model) {
  if (reading_list_model->loaded()) {
    return;
  }
  testing::NiceMock<MockReadingListModelObserver> observer_;
  base::RunLoop run_loop;
  EXPECT_CALL(observer_, ReadingListModelLoaded).WillOnce([&run_loop] {
    run_loop.Quit();
  });
  reading_list_model->AddObserver(&observer_);
  run_loop.Run();
  reading_list_model->RemoveObserver(&observer_);
}

std::set<GURL> GetReadingListURLsFromFakeServer(
    fake_server::FakeServer* fake_server) {
  std::vector<sync_pb::SyncEntity> entities =
      fake_server->GetSyncEntitiesByDataType(syncer::READING_LIST);
  std::set<GURL> urls;
  std::transform(entities.begin(), entities.end(),
                 std::inserter(urls, urls.begin()),
                 [](const sync_pb::SyncEntity& entity) {
                   return GURL(entity.specifics().reading_list().url());
                 });
  return urls;
}

std::unique_ptr<syncer::LoopbackServerEntity> CreateTestReadingListEntity(
    const GURL& url,
    const std::string& entry_title) {
  sync_pb::EntitySpecifics specifics;
  *specifics.mutable_reading_list() = *base::MakeRefCounted<ReadingListEntry>(
                                           url, entry_title, base::Time::Now())
                                           ->AsReadingListSpecifics()
                                           .get();
  return syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
      "non_unique_name", url.spec(), specifics,
      /*creation_time=*/syncer::TimeToProtoTime(base::Time::Now()),
      /*last_modified_time=*/syncer::TimeToProtoTime(base::Time::Now()));
}

ServerReadingListURLsEqualityChecker::ServerReadingListURLsEqualityChecker(
    const std::set<GURL>& expected_urls)
    : expected_urls_(std::move(expected_urls)) {}

ServerReadingListURLsEqualityChecker::~ServerReadingListURLsEqualityChecker() =
    default;

bool ServerReadingListURLsEqualityChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for server-side reading list URLs to match expected.";

  const std::set<GURL> actual_urls =
      GetReadingListURLsFromFakeServer(fake_server());

  testing::StringMatchResultListener result_listener;
  const bool matches = ExplainMatchResult(testing::ContainerEq(expected_urls_),
                                          actual_urls, &result_listener);
  *os << result_listener.str();
  return matches;
}

LocalReadingListURLsEqualityChecker::LocalReadingListURLsEqualityChecker(
    ReadingListModel* model,
    const base::flat_set<GURL>& expected_urls)
    : model_(model), expected_urls_(std::move(expected_urls)) {
  model_->AddObserver(this);
}

LocalReadingListURLsEqualityChecker::~LocalReadingListURLsEqualityChecker() {
  model_->RemoveObserver(this);
}

bool LocalReadingListURLsEqualityChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for local reading list URLs to match the expected URLs.";

  testing::StringMatchResultListener result_listener;
  const bool matches = ExplainMatchResult(testing::ContainerEq(expected_urls_),
                                          model_->GetKeys(), &result_listener);
  *os << result_listener.str();
  return matches;
}

void LocalReadingListURLsEqualityChecker::ReadingListDidApplyChanges(
    ReadingListModel* model) {
  CheckExitCondition();
}

ServerReadingListTitlesEqualityChecker::ServerReadingListTitlesEqualityChecker(
    std::set<std::string> expected_titles)
    : expected_titles_(std::move(expected_titles)) {}

ServerReadingListTitlesEqualityChecker::
    ~ServerReadingListTitlesEqualityChecker() = default;

bool ServerReadingListTitlesEqualityChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for server-side reading list titles to match expected.";

  std::vector<sync_pb::SyncEntity> entities =
      fake_server()->GetSyncEntitiesByDataType(syncer::READING_LIST);

  std::set<std::string> actual_titles;
  for (const sync_pb::SyncEntity& entity : entities) {
    actual_titles.insert(entity.specifics().reading_list().title());
  }

  testing::StringMatchResultListener result_listener;
  const bool matches = ExplainMatchResult(
      testing::ContainerEq(expected_titles_), actual_titles, &result_listener);
  *os << result_listener.str();
  return matches;
}

}  // namespace reading_list_helper
