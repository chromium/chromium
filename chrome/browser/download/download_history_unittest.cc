// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_history.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/history/content/browser/download_conversions.h"
#include "components/history/core/browser/download_constants.h"
#include "components/history/core/browser/download_row.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#endif

using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::ReturnRefOfCopy;
using testing::SetArgPointee;
using testing::WithArg;
using testing::_;

namespace {

using IdSet = DownloadHistory::IdSet;
using StrictMockDownloadItem = testing::StrictMock<download::MockDownloadItem>;

class FakeHistoryAdapter : public DownloadHistory::HistoryAdapter {
 public:
  FakeHistoryAdapter() : DownloadHistory::HistoryAdapter(nullptr) {}

  void QueryDownloads(
      history::HistoryService::DownloadQueryCallback callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(&FakeHistoryAdapter::QueryDownloadsDone,
                                  base::Unretained(this), std::move(callback)));
  }

  void QueryDownloadsDone(
      history::HistoryService::DownloadQueryCallback callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    CHECK(expect_query_downloads_.has_value());

    // Use swap to reset the base::Optional<...> to a known state before
    // moving the value (moving the value out of a base::Optional<...>
    // does not reset it to base::nullopt).
    using std::swap;
    base::Optional<std::vector<history::DownloadRow>> rows;
    swap(rows, expect_query_downloads_);

    std::move(callback).Run(std::move(*rows));
  }

  void set_slow_create_download(bool slow) { slow_create_download_ = slow; }

  void CreateDownload(
      const history::DownloadRow& row,
      history::HistoryService::DownloadCreateCallback callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    create_download_row_ = row;
    // Must not call CreateDownload() again before FinishCreateDownload()!
    DCHECK(create_download_callback_.is_null());
    create_download_callback_ =
        base::BindOnce(std::move(callback), !fail_create_download_);
    fail_create_download_ = false;
    if (!slow_create_download_)
      FinishCreateDownload();
  }

  void FinishCreateDownload() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    std::move(create_download_callback_).Run();
  }

  void UpdateDownload(const history::DownloadRow& row,
                      bool should_commit_immediately) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    update_download_ = row;
    should_commit_immediately_ = should_commit_immediately;
  }

  void RemoveDownloads(const IdSet& ids) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    for (auto it = ids.begin(); it != ids.end(); ++it) {
      remove_downloads_.insert(*it);
    }
  }

  void ExpectWillQueryDownloads(std::vector<history::DownloadRow> rows) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    expect_query_downloads_ = std::move(rows);
  }

  void ExpectQueryDownloadsDone() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    EXPECT_TRUE(!expect_query_downloads_.has_value());
  }

  void FailCreateDownload() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    fail_create_download_ = true;
  }

  void ExpectDownloadCreated(const history::DownloadRow& row) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    EXPECT_EQ(row, create_download_row_);
    create_download_row_ = history::DownloadRow();
  }

  void ExpectNoDownloadCreated() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    EXPECT_EQ(history::DownloadRow(), create_download_row_);
  }

  void ExpectDownloadUpdated(const history::DownloadRow& row,
                             bool should_commit_immediately) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    EXPECT_EQ(update_download_, row);
    EXPECT_EQ(should_commit_immediately_, should_commit_immediately);
    update_download_ = history::DownloadRow();
    should_commit_immediately_ = false;
  }

  void ExpectNoDownloadUpdated() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    EXPECT_EQ(history::DownloadRow(), update_download_);
  }

  void ExpectNoDownloadsRemoved() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    EXPECT_EQ(0, static_cast<int>(remove_downloads_.size()));
  }

  void ExpectDownloadsRemoved(const IdSet& ids) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    IdSet differences = base::STLSetDifference<IdSet>(ids, remove_downloads_);
    for (auto different = differences.begin(); different != differences.end();
         ++different) {
      EXPECT_TRUE(false) << *different;
    }
    remove_downloads_.clear();
  }

 private:
  bool slow_create_download_ = false;
  bool fail_create_download_ = false;
  bool should_commit_immediately_ = false;
  base::OnceClosure create_download_callback_;
  history::DownloadRow update_download_;
  base::Optional<std::vector<history::DownloadRow>> expect_query_downloads_;
  IdSet remove_downloads_;
  history::DownloadRow create_download_row_;

  DISALLOW_COPY_AND_ASSIGN(FakeHistoryAdapter);
};

class TestDownloadHistoryObserver : public DownloadHistory::Observer {
 public:
  void OnHistoryQueryComplete() override {
    on_history_query_complete_called_ = true;
  }
  bool on_history_query_complete_called_ = false;
};

class DownloadHistoryTest : public testing::Test {
 public:
  // Generic callback that receives a pointer to a StrictMockDownloadItem.
  using DownloadItemCallback =
      base::Callback<void(download::MockDownloadItem*)>;

  DownloadHistoryTest()
      : manager_(std::make_unique<content::MockDownloadManager>()) {}

 protected:
  void TearDown() override { download_history_.reset(); }

  content::MockDownloadManager& manager() { return *manager_.get(); }
  download::MockDownloadItem& item(size_t index) { return *items_[index]; }
  DownloadHistory* download_history() { return download_history_.get(); }

  void SetManagerObserver(
      content::DownloadManager::Observer* manager_observer) {
    manager_observer_ = manager_observer;
  }
  content::DownloadManager::Observer* manager_observer() {
    return manager_observer_;
  }

  // Creates the DownloadHistory. If |return_null_item| is true, |manager_|
  // will return nullptr on CreateDownloadItem() call,
  void CreateDownloadHistory(std::vector<history::DownloadRow> rows,
                             bool return_null_item = false) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    EXPECT_CALL(manager(), AddObserver(_)).WillOnce(WithArg<0>(Invoke(
        this, &DownloadHistoryTest::SetManagerObserver)));
    EXPECT_CALL(manager(), RemoveObserver(_));
    download_created_index_ = 0;
    for (size_t index = 0; index < rows.size(); ++index) {
      const history::DownloadRow& row = rows.at(index);
      content::MockDownloadManager::CreateDownloadItemAdapter adapter(
          row.guid, history::ToContentDownloadId(row.id), row.current_path,
          row.target_path, row.url_chain, row.referrer_url, row.site_url,
          row.tab_url, row.tab_referrer_url, base::nullopt, row.mime_type,
          row.original_mime_type, row.start_time, row.end_time, row.etag,
          row.last_modified, row.received_bytes, row.total_bytes, std::string(),
          history::ToContentDownloadState(row.state),
          history::ToContentDownloadDangerType(row.danger_type),
          history::ToContentDownloadInterruptReason(row.interrupt_reason),
          row.opened, row.last_access_time, row.transient,
          history::ToContentReceivedSlices(row.download_slice_info));
      if (return_null_item) {
        EXPECT_CALL(manager(), MockCreateDownloadItem(adapter))
            .WillOnce(Return(nullptr));
      } else {
        EXPECT_CALL(manager(), MockCreateDownloadItem(adapter))
            .WillOnce(DoAll(
                InvokeWithoutArgs(
                    this, &DownloadHistoryTest::CallOnDownloadCreatedInOrder),
                Return(&item(index))));
      }
    }
    history_ = new FakeHistoryAdapter();
    history_->ExpectWillQueryDownloads(std::move(rows));
    EXPECT_CALL(manager(), GetAllDownloads(_)).WillRepeatedly(Return());
    download_history_.reset(new DownloadHistory(
        &manager(),
        std::unique_ptr<DownloadHistory::HistoryAdapter>(history_)));
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    history_->ExpectQueryDownloadsDone();
  }

  void CallOnDownloadCreated(size_t index) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    manager_observer()->OnDownloadCreated(&manager(), &item(index));
  }

  void CallOnDownloadCreatedInOrder() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // Gmock doesn't appear to support something like InvokeWithTheseArgs. Maybe
    // gmock needs to learn about base::Callback.
    CallOnDownloadCreated(download_created_index_++);
  }

  void set_slow_create_download(bool slow) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->set_slow_create_download(slow);
  }

  void FinishCreateDownload() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->FinishCreateDownload();
  }

  void FailCreateDownload() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->FailCreateDownload();
  }

  void ExpectDownloadCreated(const history::DownloadRow& row) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->ExpectDownloadCreated(row);
  }

  void ExpectNoDownloadCreated() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->ExpectNoDownloadCreated();
  }

  void ExpectDownloadUpdated(const history::DownloadRow& row,
                             bool should_commit_immediately) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->ExpectDownloadUpdated(row, should_commit_immediately);
  }

  void ExpectNoDownloadUpdated() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->ExpectNoDownloadUpdated();
  }

  void ExpectNoDownloadsRemoved() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->ExpectNoDownloadsRemoved();
  }

  void ExpectDownloadsRemoved(const IdSet& ids) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->ExpectDownloadsRemoved(ids);
  }

  void AddAllDownloads(
      content::DownloadManager::DownloadVector* download_vector) {
    for (size_t i = 0; i < items_.size(); ++i)
      download_vector->push_back(&item(i));
  }

  void InitBasicItem(const base::FilePath::CharType* path,
                     const char* url_string,
                     const char* referrer_string,
                     download::DownloadItem::DownloadState state,
                     history::DownloadRow* row) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    size_t index = items_.size();
    items_.push_back(std::make_unique<StrictMockDownloadItem>());

    base::Time now = base::Time::Now();

    row->current_path = base::FilePath(path);
    row->target_path = base::FilePath(path);
    row->url_chain.push_back(GURL(url_string));
    row->referrer_url = GURL(referrer_string);
    row->site_url = GURL("http://example.com");
    row->tab_url = GURL("http://example.com/tab-url");
    row->tab_referrer_url = GURL("http://example.com/tab-referrer-url");
    row->mime_type = "application/octet-stream";
    row->original_mime_type = "application/octet-stream";
    row->start_time = now - base::TimeDelta::FromMinutes(10);
    row->end_time = now - base::TimeDelta::FromMinutes(1);
    row->etag = "Etag";
    row->last_modified = "abc";
    row->received_bytes = 100;
    row->total_bytes = 100;
    row->state = history::ToHistoryDownloadState(state);
    row->danger_type = history::ToHistoryDownloadDangerType(
        download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
    row->interrupt_reason = history::ToHistoryDownloadInterruptReason(
        download::DOWNLOAD_INTERRUPT_REASON_NONE);
    row->id =
        history::ToHistoryDownloadId(static_cast<uint32_t>(items_.size() + 1));
    row->guid = base::GenerateGUID();
    row->opened = false;
    row->last_access_time = now;
    row->transient = false;

    EXPECT_CALL(item(index), GetId()).WillRepeatedly(Return(row->id));
    EXPECT_CALL(item(index), GetGuid())
        .WillRepeatedly(ReturnRefOfCopy(row->guid));
    EXPECT_CALL(item(index), GetFullPath())
        .WillRepeatedly(ReturnRefOfCopy(row->current_path));
    EXPECT_CALL(item(index), GetTargetFilePath())
        .WillRepeatedly(ReturnRefOfCopy(row->target_path));
    DCHECK_LE(1u, row->url_chain.size());
    EXPECT_CALL(item(index), GetURL())
        .WillRepeatedly(ReturnRefOfCopy(row->url_chain[0]));
    EXPECT_CALL(item(index), GetUrlChain())
        .WillRepeatedly(ReturnRefOfCopy(row->url_chain));
    EXPECT_CALL(item(index), GetMimeType())
        .WillRepeatedly(Return(row->mime_type));
    EXPECT_CALL(item(index), GetOriginalMimeType())
        .WillRepeatedly(Return(row->original_mime_type));
    EXPECT_CALL(item(index), GetReferrerUrl())
        .WillRepeatedly(ReturnRefOfCopy(row->referrer_url));
    EXPECT_CALL(item(index), GetSiteUrl())
        .WillRepeatedly(ReturnRefOfCopy(row->site_url));
    EXPECT_CALL(item(index), GetTabUrl())
        .WillRepeatedly(ReturnRefOfCopy(row->tab_url));
    EXPECT_CALL(item(index), GetTabReferrerUrl())
        .WillRepeatedly(ReturnRefOfCopy(row->tab_referrer_url));
    EXPECT_CALL(item(index), GetStartTime())
        .WillRepeatedly(Return(row->start_time));
    EXPECT_CALL(item(index), GetEndTime())
        .WillRepeatedly(Return(row->end_time));
    EXPECT_CALL(item(index), GetETag())
        .WillRepeatedly(ReturnRefOfCopy(row->etag));
    EXPECT_CALL(item(index), GetLastModifiedTime())
        .WillRepeatedly(ReturnRefOfCopy(row->last_modified));
    EXPECT_CALL(item(index), GetReceivedBytes())
        .WillRepeatedly(Return(row->received_bytes));
    EXPECT_CALL(item(index), GetReceivedSlices())
        .WillRepeatedly(ReturnRefOfCopy(
            std::vector<download::DownloadItem::ReceivedSlice>()));
    EXPECT_CALL(item(index), GetTotalBytes())
        .WillRepeatedly(Return(row->total_bytes));
    EXPECT_CALL(item(index), GetState()).WillRepeatedly(Return(state));
    EXPECT_CALL(item(index), GetDangerType())
        .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
    EXPECT_CALL(item(index), GetLastReason())
        .WillRepeatedly(Return(download::DOWNLOAD_INTERRUPT_REASON_NONE));
    EXPECT_CALL(item(index), GetOpened()).WillRepeatedly(Return(row->opened));
    EXPECT_CALL(item(index), GetLastAccessTime())
        .WillRepeatedly(Return(row->last_access_time));
    EXPECT_CALL(item(index), IsTransient())
        .WillRepeatedly(Return(row->transient));
    EXPECT_CALL(item(index), GetTargetDisposition())
        .WillRepeatedly(
            Return(download::DownloadItem::TARGET_DISPOSITION_OVERWRITE));
    EXPECT_CALL(item(index), IsSavePackageDownload())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(item(index), IsDone()).WillRepeatedly(Return(false));
    EXPECT_CALL(item(index), GetDownloadCreationType())
        .WillRepeatedly(
            Return(state == download::DownloadItem::IN_PROGRESS
                       ? download::DownloadItem::TYPE_ACTIVE_DOWNLOAD
                       : download::DownloadItem::TYPE_HISTORY_IMPORT));
    EXPECT_CALL(manager(), GetDownload(row->id))
        .WillRepeatedly(Return(&item(index)));
    EXPECT_CALL(item(index), IsTemporary()).WillRepeatedly(Return(false));
#if BUILDFLAG(ENABLE_EXTENSIONS)
    new extensions::DownloadedByExtension(&item(index), row->by_ext_id,
                                          row->by_ext_name);
#endif

    row->download_slice_info =
        history::GetHistoryDownloadSliceInfos(item(index));

    std::vector<download::DownloadItem*> items;
    for (size_t i = 0; i < items_.size(); ++i) {
      items.push_back(&item(i));
    }
    EXPECT_CALL(*manager_.get(), GetAllDownloads(_))
        .WillRepeatedly(SetArgPointee<0>(items));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::vector<std::unique_ptr<StrictMockDownloadItem>> items_;
  std::unique_ptr<content::MockDownloadManager> manager_;
  FakeHistoryAdapter* history_ = nullptr;
  std::unique_ptr<DownloadHistory> download_history_;
  content::DownloadManager::Observer* manager_observer_ = nullptr;
  size_t download_created_index_ = 0;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(DownloadHistoryTest);
};


// Test loading an item from the database, changing it and removing it.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_LoadWithDownloadDB) {
  // Load a download from history, create the item, OnDownloadCreated,
  // OnDownloadUpdated, OnDownloadRemoved.
  history::DownloadRow row;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::IN_PROGRESS, &row);
  {
    std::vector<history::DownloadRow> rows = {row};
    CreateDownloadHistory(std::move(rows));
    ExpectNoDownloadCreated();
  }
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  // Pretend that something changed on the item, the update will not be
  // persisted.
  EXPECT_CALL(item(0), GetOpened()).WillRepeatedly(Return(true));
  item(0).NotifyObserversDownloadUpdated();
  ExpectNoDownloadUpdated();

  // Pretend that the user removed the item.
  IdSet ids;
  ids.insert(row.id);
  item(0).NotifyObserversDownloadRemoved();
  ExpectDownloadsRemoved(ids);
}

// Test that the OnHistoryQueryComplete() observer method is invoked for an
// observer that was added before the initial history query completing.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_OnHistoryQueryComplete_Pre) {
  class TestHistoryAdapter : public DownloadHistory::HistoryAdapter {
   public:
    TestHistoryAdapter(
        history::HistoryService::DownloadQueryCallback* callback_storage)
        : HistoryAdapter(nullptr),
          query_callback_(callback_storage) {}
    void QueryDownloads(
        history::HistoryService::DownloadQueryCallback callback) override {
      *query_callback_ = std::move(callback);
    }

    history::HistoryService::DownloadQueryCallback* query_callback_;
  };

  TestDownloadHistoryObserver observer;
  history::HistoryService::DownloadQueryCallback query_callback;
  std::unique_ptr<TestHistoryAdapter> test_history_adapter(
      new TestHistoryAdapter(&query_callback));

  // Create a new DownloadHistory object. This should cause
  // TestHistoryAdapter::QueryDownloads() to be called. The TestHistoryAdapter
  // stored the completion callback.
  std::unique_ptr<DownloadHistory> history(
      new DownloadHistory(&manager(), std::move(test_history_adapter)));
  history->AddObserver(&observer);
  EXPECT_FALSE(observer.on_history_query_complete_called_);
  ASSERT_FALSE(query_callback.is_null());

  // Now invoke the query completion callback.
  std::vector<history::DownloadRow> query_results;
  std::move(query_callback).Run(std::move(query_results));
  EXPECT_TRUE(observer.on_history_query_complete_called_);
  history->RemoveObserver(&observer);
}

// Test that the OnHistoryQueryComplete() observer method is invoked for an
// observer that was added after the initial history query completing.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_OnHistoryQueryComplete_Post) {
  TestDownloadHistoryObserver observer;
  CreateDownloadHistory({});
  download_history()->AddObserver(&observer);
  EXPECT_TRUE(observer.on_history_query_complete_called_);
  download_history()->RemoveObserver(&observer);
}

// Test creating an completed item, saving it to the database, changing it,
// saving it back, removing it.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_Create) {
  // Create a fresh item not from history, OnDownloadCreated, OnDownloadUpdated,
  // OnDownloadRemoved.
  CreateDownloadHistory({});

  history::DownloadRow row;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::COMPLETE, &row);
  EXPECT_CALL(item(0), IsDone()).WillRepeatedly(Return(true));

  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  ExpectDownloadCreated(row);
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  // Pretend that something changed on the item.
  EXPECT_CALL(item(0), GetOpened()).WillRepeatedly(Return(true));
  item(0).NotifyObserversDownloadUpdated();
  row.opened = true;
  // The previous row was cached in memory, all the changes will be updated
  // immediately
  ExpectDownloadUpdated(row, true);

  // Pretend that the user removed the item.
  IdSet ids;
  ids.insert(row.id);
  item(0).NotifyObserversDownloadRemoved();
  ExpectDownloadsRemoved(ids);
}

// Test creating a new item, saving it, removing it by setting it Temporary,
// changing it without saving it back because it's Temporary, clearing
// IsTemporary, saving it back, changing it, saving it back because it isn't
// Temporary anymore.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_Temporary) {
  // Create a fresh item not from history, OnDownloadCreated, OnDownloadUpdated,
  // OnDownloadRemoved.
  CreateDownloadHistory({});

  history::DownloadRow row;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::COMPLETE, &row);
  EXPECT_CALL(item(0), IsDone()).WillRepeatedly(Return(true));

  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  ExpectDownloadCreated(row);
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  // Pretend the item was marked temporary. DownloadHistory should remove it
  // from history and start ignoring it.
  EXPECT_CALL(item(0), IsTemporary()).WillRepeatedly(Return(true));
  item(0).NotifyObserversDownloadUpdated();
  IdSet ids;
  ids.insert(row.id);
  ExpectDownloadsRemoved(ids);

  // Change something that would make DownloadHistory call UpdateDownload if the
  // item weren't temporary.
  EXPECT_CALL(item(0), GetReceivedBytes()).WillRepeatedly(Return(4200));
  item(0).NotifyObserversDownloadUpdated();
  ExpectNoDownloadUpdated();

  // Changing a temporary item back to a non-temporary item should make
  // DownloadHistory call CreateDownload.
  EXPECT_CALL(item(0), IsTemporary()).WillRepeatedly(Return(false));
  item(0).NotifyObserversDownloadUpdated();
  row.received_bytes = 4200;
  ExpectDownloadCreated(row);
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  EXPECT_CALL(item(0), GetReceivedBytes()).WillRepeatedly(Return(100));
  item(0).NotifyObserversDownloadUpdated();
  row.received_bytes = 100;
  ExpectDownloadUpdated(row, true);
}

// Test removing downloads while they're still being added.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_RemoveWhileAdding) {
  CreateDownloadHistory({});

  history::DownloadRow row;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::COMPLETE, &row);
  EXPECT_CALL(item(0), IsDone()).WillRepeatedly(Return(true));
  // Instruct CreateDownload() to not callback to DownloadHistory immediately,
  // but to wait for FinishCreateDownload().
  set_slow_create_download(true);

  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  ExpectDownloadCreated(row);
  EXPECT_FALSE(DownloadHistory::IsPersisted(&item(0)));

  // Call OnDownloadRemoved before calling back to DownloadHistory::ItemAdded().
  // Instead of calling RemoveDownloads() immediately, DownloadHistory should
  // add the item's id to removed_while_adding_. Then, ItemAdded should
  // immediately remove the item's record from history.
  item(0).NotifyObserversDownloadRemoved();
  EXPECT_CALL(manager(), GetDownload(item(0).GetId()))
      .WillRepeatedly(Return(static_cast<download::DownloadItem*>(NULL)));
  ExpectNoDownloadsRemoved();
  EXPECT_FALSE(DownloadHistory::IsPersisted(&item(0)));

  // Now callback to DownloadHistory::ItemAdded(), and expect a call to
  // RemoveDownloads() for the item that was removed while it was being added.
  FinishCreateDownload();
  IdSet ids;
  ids.insert(row.id);
  ExpectDownloadsRemoved(ids);
  EXPECT_FALSE(DownloadHistory::IsPersisted(&item(0)));
}

// Test loading multiple items from the database and removing them all.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_Multiple) {
  // Load a download from history, create the item, OnDownloadCreated,
  // OnDownloadUpdated, OnDownloadRemoved.
  history::DownloadRow row0, row1;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::COMPLETE, &row0);
  InitBasicItem(FILE_PATH_LITERAL("/foo/qux.pdf"), "http://example.com/qux.pdf",
                "http://example.com/referrer1.html",
                download::DownloadItem::COMPLETE, &row1);
  {
    std::vector<history::DownloadRow> rows = {row0, row1};
    CreateDownloadHistory(std::move(rows));
    ExpectNoDownloadCreated();
  }

  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(1)));

  // Pretend that the user removed both items.
  IdSet ids;
  ids.insert(row0.id);
  ids.insert(row1.id);
  item(0).NotifyObserversDownloadRemoved();
  item(1).NotifyObserversDownloadRemoved();
  ExpectDownloadsRemoved(ids);
}

// Test what happens when HistoryService/CreateDownload::CreateDownload() fails.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_CreateFailed) {
  // Create a fresh item not from history, OnDownloadCreated, OnDownloadUpdated,
  // OnDownloadRemoved.
  CreateDownloadHistory({});

  history::DownloadRow row;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::COMPLETE, &row);
  EXPECT_CALL(item(0), IsDone()).WillRepeatedly(Return(true));

  FailCreateDownload();
  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  ExpectDownloadCreated(row);
  EXPECT_FALSE(DownloadHistory::IsPersisted(&item(0)));

  EXPECT_CALL(item(0), GetReceivedBytes()).WillRepeatedly(Return(100));
  item(0).NotifyObserversDownloadUpdated();
  row.received_bytes = 100;
  ExpectDownloadCreated(row);
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));
}

TEST_F(DownloadHistoryTest, DownloadHistoryTest_UpdateWhileAdding) {
  // Create a fresh item not from history, OnDownloadCreated, OnDownloadUpdated,
  // OnDownloadRemoved.
  CreateDownloadHistory({});

  history::DownloadRow row;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::COMPLETE, &row);
  EXPECT_CALL(item(0), IsDone()).WillRepeatedly(Return(true));
  // Instruct CreateDownload() to not callback to DownloadHistory immediately,
  // but to wait for FinishCreateDownload().
  set_slow_create_download(true);

  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  ExpectDownloadCreated(row);
  EXPECT_FALSE(DownloadHistory::IsPersisted(&item(0)));

  // Pretend that something changed on the item.
  EXPECT_CALL(item(0), GetOpened()).WillRepeatedly(Return(true));
  item(0).NotifyObserversDownloadUpdated();

  FinishCreateDownload();
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  // ItemAdded should call OnDownloadUpdated, which should detect that the item
  // changed while it was being added and call UpdateDownload immediately.
  row.opened = true;
  ExpectDownloadUpdated(row, true);
}

// Test creating and updating an completed item.
TEST_F(DownloadHistoryTest, CreateCompletedItem) {
  // Create a fresh item not from download DB
  CreateDownloadHistory({});

  history::DownloadRow row;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::IN_PROGRESS, &row);

  // Incomplete download will not be inserted into history.
  CallOnDownloadCreated(0);
  ExpectNoDownloadCreated();

  // Completed download should be inserted.
  EXPECT_CALL(item(0), IsDone()).WillRepeatedly(Return(true));
  EXPECT_CALL(item(0), GetState())
      .WillRepeatedly(Return(download::DownloadItem::COMPLETE));
  row.state = history::DownloadState::COMPLETE;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadCreated(row);
}

// Test creating history download item that exists in DownloadDB.
TEST_F(DownloadHistoryTest, CreateHistoryItemInDownloadDB) {
  history::DownloadRow row;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::IN_PROGRESS, &row);

  // Modify the item so it doesn't match the history record.
  EXPECT_CALL(item(0), GetReceivedBytes()).WillRepeatedly(Return(50));
  std::vector<history::DownloadRow> rows = {row};
  CreateDownloadHistory(std::move(rows));
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  // Modify the item, it should not trigger any updates.
  EXPECT_CALL(item(0), GetOpened()).WillRepeatedly(Return(true));
  item(0).NotifyObserversDownloadUpdated();
  ExpectNoDownloadUpdated();

  // Completes the item, it should trigger an update.
  EXPECT_CALL(item(0), GetState())
      .WillRepeatedly(Return(download::DownloadItem::COMPLETE));
  EXPECT_CALL(item(0), IsDone()).WillRepeatedly(Return(true));
  row.opened = true;
  row.received_bytes = 50;
  row.state = history::DownloadState::COMPLETE;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(row, true);
}

// Test that new in-progress download will not be added to history.
TEST_F(DownloadHistoryTest, CreateInProgressDownload) {
  // Create an in-progress download.
  CreateDownloadHistory({});

  history::DownloadRow row;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::IN_PROGRESS, &row);

  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  ExpectNoDownloadCreated();
  EXPECT_FALSE(DownloadHistory::IsPersisted(&item(0)));
}

// Test that in-progress download already in history will be updated once it
// becomes non-resumable.
TEST_F(DownloadHistoryTest, InProgressHistoryItemBecomesNonResumable) {
  history::DownloadRow row;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::IN_PROGRESS, &row);

  // Modify the item so it doesn't match the history record.
  EXPECT_CALL(item(0), GetLastReason())
      .WillRepeatedly(
          Return(download::DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN));
  EXPECT_CALL(item(0), GetState())
      .WillRepeatedly(Return(download::DownloadItem::INTERRUPTED));
  EXPECT_CALL(item(0), IsDone()).WillRepeatedly(Return(true));
  std::vector<history::DownloadRow> rows = {row};

  // Create the history and a db update should be triggered.
  CreateDownloadHistory(std::move(rows));
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));
  row.interrupt_reason = download::DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN;
  row.state = history::DownloadState::INTERRUPTED;
  ExpectDownloadUpdated(row, true);
}

// Test loading history download item that will be cleared by |manager_|
TEST_F(DownloadHistoryTest, RemoveClearedItemFromHistory) {
  history::DownloadRow row;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::IN_PROGRESS, &row);

  std::vector<history::DownloadRow> rows = {row};
  CreateDownloadHistory(std::move(rows), true);

  // The download should be removed from history afterwards.
  IdSet ids;
  ids.insert(row.id);
  ExpectDownloadsRemoved(ids);
}

}  // anonymous namespace
