// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history_report/data_observer.h"

#include <memory>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/time/time.h"
#include "chrome/browser/android/history_report/delta_file_service.h"
#include "chrome/browser/android/history_report/usage_reports_buffer_service.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

using base::Time;
using bookmarks::BookmarkModel;
using bookmarks::TestBookmarkClient;
using history::HistoryService;

using testing::_;

namespace {

void MockRun() {}

}  // namespace

namespace history_report {

class MockDeltaFileService : public DeltaFileService {
 public:
  explicit MockDeltaFileService(const base::FilePath& dir)
      : DeltaFileService(dir) {}

  MOCK_METHOD1(PageAdded, void(const GURL& url));
};

class MockUsageReportsBufferService : public UsageReportsBufferService {
 public:
  explicit MockUsageReportsBufferService(const base::FilePath& dir)
      : UsageReportsBufferService(dir) {}

  MOCK_METHOD3(AddVisit,
               void(const std::string& id,
                    int64_t timestamp_ms,
                    bool typed_visit));
};

class DataObserverTest : public testing::Test {
 public:
  DataObserverTest() {}

  void SetUp() override {
    // Make unique temp directory.
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    delta_file_service_ =
        std::make_unique<MockDeltaFileService>(temp_dir_.GetPath());
    usage_report_service_ =
        std::make_unique<MockUsageReportsBufferService>(temp_dir_.GetPath());
    bookmark_model_ = TestBookmarkClient::CreateModel();
    history_service_ = std::make_unique<HistoryService>();
    data_observer_ = std::make_unique<DataObserver>(
        base::Bind(&MockRun), base::Bind(&MockRun), base::Bind(&MockRun),
        delta_file_service_.get(), usage_report_service_.get(),
        bookmark_model_.get(), history_service_.get());
  }

  void TearDown() override {
    delta_file_service_.reset();
    usage_report_service_.reset();
    bookmark_model_.reset();
    history_service_.reset();
    data_observer_.reset();
  }

 protected:
  content::BrowserTaskEnvironment
      task_environment_;  // To set up BrowserThreads.

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<MockDeltaFileService> delta_file_service_;
  std::unique_ptr<MockUsageReportsBufferService> usage_report_service_;
  std::unique_ptr<BookmarkModel> bookmark_model_;
  std::unique_ptr<HistoryService> history_service_;
  std::unique_ptr<DataObserver> data_observer_;

  DISALLOW_COPY_AND_ASSIGN(DataObserverTest);
};

TEST_F(DataObserverTest, VisitLinkShouldBeLogged) {
  EXPECT_CALL(*(delta_file_service_.get()), PageAdded(GURL()));
  EXPECT_CALL(*(usage_report_service_.get()), AddVisit(_, _, _));

  data_observer_->OnURLVisited(
      history_service_.get(),
      ui::PageTransition::PAGE_TRANSITION_LINK,
      history::URLRow(GURL()), history::RedirectList(), Time::Now());
}

TEST_F(DataObserverTest, VisitRedirectShouldNotBeLogged) {
  // These methods should not be triggered if a url is visited by redirect.
  EXPECT_CALL(*(delta_file_service_.get()), PageAdded(_)).Times(0);
  EXPECT_CALL(*(usage_report_service_.get()), AddVisit(_, _, _)).Times(0);

  data_observer_->OnURLVisited(
      history_service_.get(),
      ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
      history::URLRow(GURL()), history::RedirectList(), Time::Now());
}

}  // namespace history_report
