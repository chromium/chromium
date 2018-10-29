// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_history.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
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
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
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
using InfoVector = std::vector<history::DownloadRow>;
using StrictMockDownloadItem = testing::StrictMock<download::MockDownloadItem>;

class FakeHistoryAdapter : public DownloadHistory::HistoryAdapter {
 public:
  FakeHistoryAdapter() : DownloadHistory::HistoryAdapter(nullptr) {}

  void QueryDownloads(
      const history::HistoryService::DownloadQueryCallback& callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&FakeHistoryAdapter::QueryDownloadsDone,
                       base::Unretained(this), callback));
  }

  void QueryDownloadsDone(
      const history::HistoryService::DownloadQueryCallback& callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    CHECK(expect_query_downloads_.get());
    callback.Run(std::move(expect_query_downloads_));
  }

  void set_slow_create_download(bool slow) { slow_create_download_ = slow; }

  void CreateDownload(const history::DownloadRow& info,
                      const history::HistoryService::DownloadCreateCallback&
                          callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    create_download_info_ = info;
    // Must not call CreateDownload() again before FinishCreateDownload()!
    DCHECK(create_download_callback_.is_null());
    create_download_callback_ = base::Bind(callback, !fail_create_download_);
    fail_create_download_ = false;
    if (!slow_create_download_)
      FinishCreateDownload();
  }

  void FinishCreateDownload() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    create_download_callback_.Run();
    create_download_callback_.Reset();
  }

  void UpdateDownload(const history::DownloadRow& info,
                      bool should_commit_immediately) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    update_download_ = info;
    should_commit_immediately_ = should_commit_immediately;
  }

  void RemoveDownloads(const IdSet& ids) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    for (auto it = ids.begin(); it != ids.end(); ++it) {
      remove_downloads_.insert(*it);
    }
  }

  void ExpectWillQueryDownloads(std::unique_ptr<InfoVector> infos) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    expect_query_downloads_ = std::move(infos);
  }

  void ExpectQueryDownloadsDone() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    EXPECT_TRUE(NULL == expect_query_downloads_.get());
  }

  void FailCreateDownload() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    fail_create_download_ = true;
  }

  void ExpectDownloadCreated(
      const history::DownloadRow& info) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    EXPECT_EQ(info, create_download_info_);
    create_download_info_ = history::DownloadRow();
  }

  void ExpectNoDownloadCreated() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    EXPECT_EQ(history::DownloadRow(), create_download_info_);
  }

  void ExpectDownloadUpdated(const history::DownloadRow& info,
                             bool should_commit_immediately) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    EXPECT_EQ(update_download_, info);
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
  base::Closure create_download_callback_;
  history::DownloadRow update_download_;
  std::unique_ptr<InfoVector> expect_query_downloads_;
  IdSet remove_downloads_;
  history::DownloadRow create_download_info_;

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

  // Creates the DownloadHistory. If |call_on_download_created| is false,
  // DownloadHistory::OnDownloadCreated() will not be called by |manager_|.
  // If |return_null_item| is true, |manager_| will return nullptr on
  // CreateDownloadItem() call,
  void CreateDownloadHistory(std::unique_ptr<InfoVector> infos,
                             bool call_on_download_created = true,
                             bool return_null_item = false) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    CHECK(infos.get());
    EXPECT_CALL(manager(), AddObserver(_)).WillOnce(WithArg<0>(Invoke(
        this, &DownloadHistoryTest::SetManagerObserver)));
    EXPECT_CALL(manager(), RemoveObserver(_));
    download_created_index_ = 0;
    for (size_t index = 0; index < infos->size(); ++index) {
      const history::DownloadRow& row = infos->at(index);
      content::MockDownloadManager::CreateDownloadItemAdapter adapter(
          row.guid, history::ToContentDownloadId(row.id), row.current_path,
          row.target_path, row.url_chain, row.referrer_url, row.site_url,
          row.tab_url, row.tab_referrer_url, row.mime_type,
          row.original_mime_type, row.start_time, row.end_time, row.etag,
          row.last_modified, row.received_bytes, row.total_bytes, std::string(),
          history::ToContentDownloadState(row.state),
          history::ToContentDownloadDangerType(row.danger_type),
          history::ToContentDownloadInterruptReason(row.interrupt_reason),
          row.opened, row.last_access_time, row.transient,
          history::ToContentReceivedSlices(row.download_slice_info));
      if (call_on_download_created) {
        EXPECT_CALL(manager(), MockCreateDownloadItem(adapter))
            .WillOnce(DoAll(
                InvokeWithoutArgs(
                    this, &DownloadHistoryTest::CallOnDownloadCreatedInOrder),
                Return(&item(index))));
      } else {
        download::DownloadItem* download =
            return_null_item ? nullptr : &item(index);
        EXPECT_CALL(manager(), MockCreateDownloadItem(adapter))
            .WillOnce(Return(download));
      }
    }
    history_ = new FakeHistoryAdapter();
    history_->ExpectWillQueryDownloads(std::move(infos));
    if (call_on_download_created) {
      EXPECT_CALL(manager(), GetAllDownloads(_)).WillRepeatedly(Return());
    } else {
      EXPECT_CALL(manager(), GetAllDownloads(_))
          .WillRepeatedly(
              WithArg<0>(Invoke(this, &DownloadHistoryTest::AddAllDownloads)));
    }
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

  void ExpectDownloadCreated(
      const history::DownloadRow& info) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->ExpectDownloadCreated(info);
  }

  void ExpectNoDownloadCreated() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->ExpectNoDownloadCreated();
  }

  void ExpectDownloadUpdated(const history::DownloadRow& info,
                             bool should_commit_immediately) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->ExpectDownloadUpdated(info, should_commit_immediately);
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
                     history::DownloadRow* info) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    size_t index = items_.size();
    items_.push_back(std::make_unique<StrictMockDownloadItem>());

    base::Time now = base::Time::Now();

    info->current_path = base::FilePath(path);
    info->target_path = base::FilePath(path);
    info->url_chain.push_back(GURL(url_string));
    info->referrer_url = GURL(referrer_string);
    info->site_url = GURL("http://example.com");
    info->tab_url = GURL("http://example.com/tab-url");
    info->tab_referrer_url = GURL("http://example.com/tab-referrer-url");
    info->mime_type = "application/octet-stream";
    info->original_mime_type = "application/octet-stream";
    info->start_time = now - base::TimeDelta::FromMinutes(10);
    info->end_time = now - base::TimeDelta::FromMinutes(1);
    info->etag = "Etag";
    info->last_modified = "abc";
    info->received_bytes = 100;
    info->total_bytes = 100;
    info->state = history::ToHistoryDownloadState(state);
    info->danger_type = history::ToHistoryDownloadDangerType(
        download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
    info->interrupt_reason = history::ToHistoryDownloadInterruptReason(
        download::DOWNLOAD_INTERRUPT_REASON_NONE);
    info->id =
        history::ToHistoryDownloadId(static_cast<uint32_t>(items_.size() + 1));
    info->guid = base::GenerateGUID();
    info->opened = false;
    info->last_access_time = now;
    info->transient = false;

    EXPECT_CALL(item(index), GetId()).WillRepeatedly(Return(info->id));
    EXPECT_CALL(item(index), GetGuid())
        .WillRepeatedly(ReturnRefOfCopy(info->guid));
    EXPECT_CALL(item(index), GetFullPath())
        .WillRepeatedly(ReturnRefOfCopy(info->current_path));
    EXPECT_CALL(item(index), GetTargetFilePath())
        .WillRepeatedly(ReturnRefOfCopy(info->target_path));
    DCHECK_LE(1u, info->url_chain.size());
    EXPECT_CALL(item(index), GetURL())
        .WillRepeatedly(ReturnRefOfCopy(info->url_chain[0]));
    EXPECT_CALL(item(index), GetUrlChain())
        .WillRepeatedly(ReturnRefOfCopy(info->url_chain));
    EXPECT_CALL(item(index), GetMimeType())
        .WillRepeatedly(Return(info->mime_type));
    EXPECT_CALL(item(index), GetOriginalMimeType())
        .WillRepeatedly(Return(info->original_mime_type));
    EXPECT_CALL(item(index), GetReferrerUrl())
        .WillRepeatedly(ReturnRefOfCopy(info->referrer_url));
    EXPECT_CALL(item(index), GetSiteUrl())
        .WillRepeatedly(ReturnRefOfCopy(info->site_url));
    EXPECT_CALL(item(index), GetTabUrl())
        .WillRepeatedly(ReturnRefOfCopy(info->tab_url));
    EXPECT_CALL(item(index), GetTabReferrerUrl())
        .WillRepeatedly(ReturnRefOfCopy(info->tab_referrer_url));
    EXPECT_CALL(item(index), GetStartTime())
        .WillRepeatedly(Return(info->start_time));
    EXPECT_CALL(item(index), GetEndTime())
        .WillRepeatedly(Return(info->end_time));
    EXPECT_CALL(item(index), GetETag())
        .WillRepeatedly(ReturnRefOfCopy(info->etag));
    EXPECT_CALL(item(index), GetLastModifiedTime())
        .WillRepeatedly(ReturnRefOfCopy(info->last_modified));
    EXPECT_CALL(item(index), GetReceivedBytes())
        .WillRepeatedly(Return(info->received_bytes));
    EXPECT_CALL(item(index), GetReceivedSlices())
        .WillRepeatedly(ReturnRefOfCopy(
            std::vector<download::DownloadItem::ReceivedSlice>()));
    EXPECT_CALL(item(index), GetTotalBytes())
        .WillRepeatedly(Return(info->total_bytes));
    EXPECT_CALL(item(index), GetState()).WillRepeatedly(Return(state));
    EXPECT_CALL(item(index), GetDangerType())
        .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
    EXPECT_CALL(item(index), GetLastReason())
        .WillRepeatedly(Return(download::DOWNLOAD_INTERRUPT_REASON_NONE));
    EXPECT_CALL(item(index), GetOpened()).WillRepeatedly(Return(info->opened));
    EXPECT_CALL(item(index), GetLastAccessTime())
        .WillRepeatedly(Return(info->last_access_time));
    EXPECT_CALL(item(index), IsTransient())
        .WillRepeatedly(Return(info->transient));
    EXPECT_CALL(item(index), GetTargetDisposition())
        .WillRepeatedly(
            Return(download::DownloadItem::TARGET_DISPOSITION_OVERWRITE));
    EXPECT_CALL(item(index), IsSavePackageDownload())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(item(index), GetDownloadCreationType())
        .WillRepeatedly(
            Return(state == download::DownloadItem::IN_PROGRESS
                       ? download::DownloadItem::TYPE_ACTIVE_DOWNLOAD
                       : download::DownloadItem::TYPE_HISTORY_IMPORT));
    EXPECT_CALL(manager(), GetDownload(info->id))
        .WillRepeatedly(Return(&item(index)));
    EXPECT_CALL(item(index), IsTemporary()).WillRepeatedly(Return(false));
#if BUILDFLAG(ENABLE_EXTENSIONS)
    new extensions::DownloadedByExtension(&item(index), info->by_ext_id,
                                          info->by_ext_name);
#endif

    info->download_slice_info = history::GetHistoryDownloadSliceInfos(
        item(index));

    std::vector<download::DownloadItem*> items;
    for (size_t i = 0; i < items_.size(); ++i) {
      items.push_back(&item(i));
    }
    EXPECT_CALL(*manager_.get(), GetAllDownloads(_))
        .WillRepeatedly(SetArgPointee<0>(items));
  }

 private:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  std::vector<std::unique_ptr<StrictMockDownloadItem>> items_;
  std::unique_ptr<content::MockDownloadManager> manager_;
  FakeHistoryAdapter* history_ = nullptr;
  std::unique_ptr<DownloadHistory> download_history_;
  content::DownloadManager::Observer* manager_observer_ = nullptr;
  size_t download_created_index_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DownloadHistoryTest);
};

// Test loading an item from the database, changing it, saving it back, removing
// it.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_Load) {
  // Load a download from history, create the item, OnDownloadCreated,
  // OnDownloadUpdated, OnDownloadRemoved.
  history::DownloadRow info;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::IN_PROGRESS, &info);
  {
    std::unique_ptr<InfoVector> infos(new InfoVector());
    infos->push_back(info);
    CreateDownloadHistory(std::move(infos));
    ExpectNoDownloadCreated();
  }
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  // Pretend that something changed on the item.
  EXPECT_CALL(item(0), GetOpened()).WillRepeatedly(Return(true));
  item(0).NotifyObserversDownloadUpdated();
  info.opened = true;
  ExpectDownloadUpdated(info, false);

  // Pretend that the user removed the item.
  IdSet ids;
  ids.insert(info.id);
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
    void QueryDownloads(const history::HistoryService::DownloadQueryCallback&
                            callback) override {
      *query_callback_ = callback;
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
  std::unique_ptr<std::vector<history::DownloadRow>> query_results(
      new std::vector<history::DownloadRow>());
  query_callback.Run(std::move(query_results));
  EXPECT_TRUE(observer.on_history_query_complete_called_);
  history->RemoveObserver(&observer);
}

// Test that the OnHistoryQueryComplete() observer method is invoked for an
// observer that was added after the initial history query completing.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_OnHistoryQueryComplete_Post) {
  TestDownloadHistoryObserver observer;
  CreateDownloadHistory(std::unique_ptr<InfoVector>(new InfoVector()));
  download_history()->AddObserver(&observer);
  EXPECT_TRUE(observer.on_history_query_complete_called_);
  download_history()->RemoveObserver(&observer);
}

// Test creating an item, saving it to the database, changing it, saving it
// back, removing it.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_Create) {
  // Create a fresh item not from history, OnDownloadCreated, OnDownloadUpdated,
  // OnDownloadRemoved.
  CreateDownloadHistory(std::unique_ptr<InfoVector>(new InfoVector()));

  history::DownloadRow info;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::IN_PROGRESS, &info);

  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  ExpectDownloadCreated(info);
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  // Pretend that something changed on the item.
  EXPECT_CALL(item(0), GetOpened()).WillRepeatedly(Return(true));
  item(0).NotifyObserversDownloadUpdated();
  info.opened = true;
  ExpectDownloadUpdated(info, false);

  // Pretend that the user removed the item.
  IdSet ids;
  ids.insert(info.id);
  item(0).NotifyObserversDownloadRemoved();
  ExpectDownloadsRemoved(ids);
}

// Test that changes to persisted fields in a DownloadItem triggers database
// updates.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_Update) {
  CreateDownloadHistory(std::unique_ptr<InfoVector>(new InfoVector()));

  history::DownloadRow info;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::IN_PROGRESS, &info);

  CallOnDownloadCreated(0);
  ExpectDownloadCreated(info);
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  base::FilePath new_path(FILE_PATH_LITERAL("/foo/baz.txt"));
  base::Time new_time(base::Time::Now());
  std::string new_etag("new etag");
  std::string new_last_modifed("new last modified");

  // current_path
  EXPECT_CALL(item(0), GetFullPath()).WillRepeatedly(ReturnRefOfCopy(new_path));
  info.current_path = new_path;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info, true);

  // target_path
  EXPECT_CALL(item(0), GetTargetFilePath())
      .WillRepeatedly(ReturnRefOfCopy(new_path));
  info.target_path = new_path;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info, false);

  // end_time
  EXPECT_CALL(item(0), GetEndTime()).WillRepeatedly(Return(new_time));
  info.end_time = new_time;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info, false);

  // received_bytes
  EXPECT_CALL(item(0), GetReceivedBytes()).WillRepeatedly(Return(101));
  info.received_bytes = 101;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info, false);

  // received slices
  std::vector<download::DownloadItem::ReceivedSlice> slices;
  slices.push_back(download::DownloadItem::ReceivedSlice(0, 100));
  slices.push_back(download::DownloadItem::ReceivedSlice(1000, 500));
  EXPECT_CALL(item(0), GetReceivedSlices()).WillRepeatedly(
      ReturnRefOfCopy(slices));
  info.download_slice_info = history::GetHistoryDownloadSliceInfos(item(0));
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info, false);

  // total_bytes
  EXPECT_CALL(item(0), GetTotalBytes()).WillRepeatedly(Return(102));
  info.total_bytes = 102;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info, false);

  // etag
  EXPECT_CALL(item(0), GetETag()).WillRepeatedly(ReturnRefOfCopy(new_etag));
  info.etag = new_etag;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info, false);

  // last_modified
  EXPECT_CALL(item(0), GetLastModifiedTime())
      .WillRepeatedly(ReturnRefOfCopy(new_last_modifed));
  info.last_modified = new_last_modifed;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info, false);

  // state
  // Changing the state to INTERRUPTED will remove its stored state.
  EXPECT_CALL(item(0), GetState())
      .WillRepeatedly(Return(download::DownloadItem::INTERRUPTED));
  info.state = history::DownloadState::INTERRUPTED;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info, false);

  // Changing the state back to IN_PROGRESS to reset its stored state.
  EXPECT_CALL(item(0), GetState())
      .WillRepeatedly(Return(download::DownloadItem::IN_PROGRESS));
  info.state = history::DownloadState::IN_PROGRESS;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info, true);

  // danger_type
  EXPECT_CALL(item(0), GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT));
  info.danger_type = history::DownloadDangerType::DANGEROUS_CONTENT;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info, false);

  // interrupt_reason
  EXPECT_CALL(item(0), GetLastReason())
      .WillRepeatedly(
          Return(download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED));
  info.interrupt_reason = history::ToHistoryDownloadInterruptReason(
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED);
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info, false);

  // opened
  EXPECT_CALL(item(0), GetOpened()).WillRepeatedly(Return(true));
  info.opened = true;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info, false);
}

// Test creating a new item, saving it, removing it by setting it Temporary,
// changing it without saving it back because it's Temporary, clearing
// IsTemporary, saving it back, changing it, saving it back because it isn't
// Temporary anymore.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_Temporary) {
  // Create a fresh item not from history, OnDownloadCreated, OnDownloadUpdated,
  // OnDownloadRemoved.
  CreateDownloadHistory(std::unique_ptr<InfoVector>(new InfoVector()));

  history::DownloadRow info;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::COMPLETE, &info);

  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  ExpectDownloadCreated(info);
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  // Pretend the item was marked temporary. DownloadHistory should remove it
  // from history and start ignoring it.
  EXPECT_CALL(item(0), IsTemporary()).WillRepeatedly(Return(true));
  item(0).NotifyObserversDownloadUpdated();
  IdSet ids;
  ids.insert(info.id);
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
  info.received_bytes = 4200;
  ExpectDownloadCreated(info);
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  EXPECT_CALL(item(0), GetReceivedBytes()).WillRepeatedly(Return(100));
  item(0).NotifyObserversDownloadUpdated();
  info.received_bytes = 100;
  ExpectDownloadUpdated(info, true);
}

// Test removing downloads while they're still being added.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_RemoveWhileAdding) {
  CreateDownloadHistory(std::unique_ptr<InfoVector>(new InfoVector()));

  history::DownloadRow info;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::COMPLETE, &info);

  // Instruct CreateDownload() to not callback to DownloadHistory immediately,
  // but to wait for FinishCreateDownload().
  set_slow_create_download(true);

  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  ExpectDownloadCreated(info);
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
  ids.insert(info.id);
  ExpectDownloadsRemoved(ids);
  EXPECT_FALSE(DownloadHistory::IsPersisted(&item(0)));
}

// Test loading multiple items from the database and removing them all.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_Multiple) {
  // Load a download from history, create the item, OnDownloadCreated,
  // OnDownloadUpdated, OnDownloadRemoved.
  history::DownloadRow info0, info1;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::COMPLETE, &info0);
  InitBasicItem(FILE_PATH_LITERAL("/foo/qux.pdf"), "http://example.com/qux.pdf",
                "http://example.com/referrer1.html",
                download::DownloadItem::COMPLETE, &info1);
  {
    std::unique_ptr<InfoVector> infos(new InfoVector());
    infos->push_back(info0);
    infos->push_back(info1);
    CreateDownloadHistory(std::move(infos));
    ExpectNoDownloadCreated();
  }

  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(1)));

  // Pretend that the user removed both items.
  IdSet ids;
  ids.insert(info0.id);
  ids.insert(info1.id);
  item(0).NotifyObserversDownloadRemoved();
  item(1).NotifyObserversDownloadRemoved();
  ExpectDownloadsRemoved(ids);
}

// Test what happens when HistoryService/CreateDownload::CreateDownload() fails.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_CreateFailed) {
  // Create a fresh item not from history, OnDownloadCreated, OnDownloadUpdated,
  // OnDownloadRemoved.
  CreateDownloadHistory(std::unique_ptr<InfoVector>(new InfoVector()));

  history::DownloadRow info;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::COMPLETE, &info);

  FailCreateDownload();
  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  ExpectDownloadCreated(info);
  EXPECT_FALSE(DownloadHistory::IsPersisted(&item(0)));

  EXPECT_CALL(item(0), GetReceivedBytes()).WillRepeatedly(Return(100));
  item(0).NotifyObserversDownloadUpdated();
  info.received_bytes = 100;
  ExpectDownloadCreated(info);
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));
}

TEST_F(DownloadHistoryTest, DownloadHistoryTest_UpdateWhileAdding) {
  // Create a fresh item not from history, OnDownloadCreated, OnDownloadUpdated,
  // OnDownloadRemoved.
  CreateDownloadHistory(std::unique_ptr<InfoVector>(new InfoVector()));

  history::DownloadRow info;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::IN_PROGRESS, &info);

  // Instruct CreateDownload() to not callback to DownloadHistory immediately,
  // but to wait for FinishCreateDownload().
  set_slow_create_download(true);

  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  ExpectDownloadCreated(info);
  EXPECT_FALSE(DownloadHistory::IsPersisted(&item(0)));

  // Pretend that something changed on the item.
  EXPECT_CALL(item(0), GetOpened()).WillRepeatedly(Return(true));
  item(0).NotifyObserversDownloadUpdated();

  FinishCreateDownload();
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  // ItemAdded should call OnDownloadUpdated, which should detect that the item
  // changed while it was being added and call UpdateDownload immediately.
  info.opened = true;
  ExpectDownloadUpdated(info, false);
}

// Test creating and updating an item with DownloadDB enabled.
TEST_F(DownloadHistoryTest, CreateWithDownloadDB) {
  // Enable download DB.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      download::features::kDownloadDBForNewDownloads);

  // Create a fresh item not from download DB
  CreateDownloadHistory(std::unique_ptr<InfoVector>(new InfoVector()));

  history::DownloadRow info;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::IN_PROGRESS, &info);

  // Incomplete download will not be inserted into history.
  CallOnDownloadCreated(0);
  ExpectNoDownloadCreated();

  // Completed download should be inserted.
  EXPECT_CALL(item(0), GetState())
      .WillRepeatedly(Return(download::DownloadItem::COMPLETE));
  info.state = history::DownloadState::COMPLETE;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadCreated(info);
}

// Test creating history download item that exists in DownloadDB.
TEST_F(DownloadHistoryTest, CreateHistoryItemInDownloadDB) {
  // Enable download DB.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      download::features::kDownloadDBForNewDownloads);

  history::DownloadRow info;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::IN_PROGRESS, &info);

  // Modify the item so it doesn't match the history record.
  EXPECT_CALL(item(0), GetReceivedBytes()).WillRepeatedly(Return(50));
  std::unique_ptr<InfoVector> infos(new InfoVector());
  infos->push_back(info);
  CreateDownloadHistory(std::move(infos), false);
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  // Modify the item, it should not trigger any updates.
  EXPECT_CALL(item(0), GetOpened()).WillRepeatedly(Return(true));
  item(0).NotifyObserversDownloadUpdated();
  ExpectNoDownloadUpdated();

  // Completes the item, it should trigger an update.
  EXPECT_CALL(item(0), GetState())
      .WillRepeatedly(Return(download::DownloadItem::COMPLETE));
  info.opened = true;
  info.received_bytes = 50;
  info.state = history::DownloadState::COMPLETE;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info, false);
}

// Test loading history download item that will be cleared by |manager_|
TEST_F(DownloadHistoryTest, RemoveClearedItemFromHistory) {
  // Enable download DB.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      download::features::kDownloadDBForNewDownloads);

  history::DownloadRow info;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"), "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                download::DownloadItem::IN_PROGRESS, &info);

  std::unique_ptr<InfoVector> infos(new InfoVector());
  infos->push_back(info);
  CreateDownloadHistory(std::move(infos), false, true);

  // The download should be removed from history afterwards.
  IdSet ids;
  ids.insert(info.id);
  ExpectDownloadsRemoved(ids);
}

}  // anonymous namespace
