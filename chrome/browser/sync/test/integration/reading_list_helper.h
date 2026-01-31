// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_READING_LIST_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_READING_LIST_HELPER_H_

#include <ostream>
#include <set>
#include <string>

#include "base/containers/flat_set.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "components/reading_list/core/mock_reading_list_model_observer.h"
#include "components/sync/test/fake_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class GURL;
class ReadingListModel;

namespace reading_list_helper {

void WaitForReadingListModelLoaded(ReadingListModel* reading_list_model);

std::set<GURL> GetReadingListURLsFromFakeServer(
    fake_server::FakeServer* fake_server);

std::unique_ptr<syncer::LoopbackServerEntity> CreateTestReadingListEntity(
    const GURL& url,
    const std::string& entry_title);

// Checker used to block until the reading list URLs on the server match a
// given set of expected reading list URLs.
class ServerReadingListURLsEqualityChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  explicit ServerReadingListURLsEqualityChecker(
      const std::set<GURL>& expected_urls);

  ServerReadingListURLsEqualityChecker(
      const ServerReadingListURLsEqualityChecker&) = delete;
  ServerReadingListURLsEqualityChecker& operator=(
      const ServerReadingListURLsEqualityChecker&) = delete;

  ~ServerReadingListURLsEqualityChecker() override;

  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const std::set<GURL> expected_urls_;
};

class LocalReadingListURLsEqualityChecker
    : public StatusChangeChecker,
      public testing::NiceMock<MockReadingListModelObserver> {
 public:
  LocalReadingListURLsEqualityChecker(
      ReadingListModel* model,
      const base::flat_set<GURL>& expected_urls);

  LocalReadingListURLsEqualityChecker(
      const LocalReadingListURLsEqualityChecker&) = delete;
  LocalReadingListURLsEqualityChecker& operator=(
      const LocalReadingListURLsEqualityChecker&) = delete;

  ~LocalReadingListURLsEqualityChecker() override;

  bool IsExitConditionSatisfied(std::ostream* os) override;

  // ReadingListModelObserver implementation.
  void ReadingListDidApplyChanges(ReadingListModel* model) override;

 private:
  const raw_ptr<ReadingListModel> model_;
  const base::flat_set<GURL> expected_urls_;
};

// Checker used to block until the reading set titles on the server match a
// given set of expected reading list titles.
class ServerReadingListTitlesEqualityChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  explicit ServerReadingListTitlesEqualityChecker(
      std::set<std::string> expected_titles);

  ServerReadingListTitlesEqualityChecker(
      const ServerReadingListTitlesEqualityChecker&) = delete;
  ServerReadingListTitlesEqualityChecker& operator=(
      const ServerReadingListTitlesEqualityChecker&) = delete;

  ~ServerReadingListTitlesEqualityChecker() override;

  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const std::set<std::string> expected_titles_;
};

}  // namespace reading_list_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_READING_LIST_HELPER_H_
