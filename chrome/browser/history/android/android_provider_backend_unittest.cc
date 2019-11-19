// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/android/android_provider_backend.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/chrome_history_client.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/history/core/browser/android/android_time.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/test/test_history_database.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

using base::Time;
using base::TimeDelta;
using base::UTF8ToUTF16;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using content::BrowserThread;

namespace history {

namespace {

struct BookmarkCacheRow {
 public:
  BookmarkCacheRow()
      : url_id_(0),
        bookmark_(false),
        favicon_id_(0) {
  }
  URLID url_id_;
  Time create_time_;
  Time last_visit_time_;
  bool bookmark_;
  favicon_base::FaviconID favicon_id_;
};

// Creates a 16x16 bitmap.
SkBitmap CreateBitmap() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  bitmap.eraseColor(SK_ColorBLUE);
  return bitmap;
}

}  // namespace

class AndroidProviderBackendDelegate : public HistoryBackend::Delegate {
 public:
  AndroidProviderBackendDelegate() {}

  void NotifyProfileError(sql::InitStatus init_status,
                          const std::string& diagnostics) override {}
  void SetInMemoryBackend(
      std::unique_ptr<InMemoryHistoryBackend> backend) override {}
  void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                             const GURL& icon_url) override {
    favicon_changed_.reset(
        new std::set<GURL>(page_urls.begin(), page_urls.end()));
  }
  void NotifyURLVisited(ui::PageTransition,
                        const history::URLRow& row,
                        const history::RedirectList& redirects,
                        base::Time visit_time) override {}
  void NotifyURLsModified(const history::URLRows& rows) override {
    modified_details_.reset(new history::URLRows(rows));
  }
  void NotifyURLsDeleted(DeletionInfo deletion_info) override {
    deleted_details_.reset(new history::URLRows(deletion_info.deleted_rows()));
  }
  void NotifyKeywordSearchTermUpdated(const URLRow& row,
                                      KeywordID keyword_id,
                                      const base::string16& term) override {}
  void NotifyKeywordSearchTermDeleted(URLID url_id) override {}
  void DBLoaded() override {}

  history::URLRows* deleted_details() const { return deleted_details_.get(); }

  history::URLRows* modified_details() const { return modified_details_.get(); }

  std::set<GURL>* favicon_changed() const { return favicon_changed_.get(); }

  void ResetDetails() {
    deleted_details_.reset();
    modified_details_.reset();
    favicon_changed_.reset();
  }

 private:
  std::unique_ptr<history::URLRows> deleted_details_;
  std::unique_ptr<history::URLRows> modified_details_;
  std::unique_ptr<std::set<GURL>> favicon_changed_;

  DISALLOW_COPY_AND_ASSIGN(AndroidProviderBackendDelegate);
};

class AndroidProviderBackendNotifier : public HistoryBackendNotifier {
 public:
  AndroidProviderBackendNotifier() {}

  // HistoryBackendNotifier:
  void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                             const GURL& icon_url) override {
    favicon_changed_.reset(
        new std::set<GURL>(page_urls.begin(), page_urls.end()));
  }
  void NotifyURLVisited(ui::PageTransition,
                        const history::URLRow& row,
                        const history::RedirectList& redirects,
                        base::Time visit_time) override {}
  void NotifyURLsModified(const history::URLRows& rows,
                          bool is_from_expiration) override {
    EXPECT_FALSE(is_from_expiration);
    modified_details_.reset(new history::URLRows(rows));
  }
  void NotifyURLsDeleted(DeletionInfo deletion_info) override {
    deleted_details_.reset(new history::URLRows(deletion_info.deleted_rows()));
  }

  history::URLRows* deleted_details() const { return deleted_details_.get(); }

  history::URLRows* modified_details() const { return modified_details_.get(); }

  std::set<GURL>* favicon_changed() const { return favicon_changed_.get(); }

  void ResetDetails() {
    deleted_details_.reset();
    modified_details_.reset();
    favicon_changed_.reset();
  }

 private:
  std::unique_ptr<history::URLRows> deleted_details_;
  std::unique_ptr<history::URLRows> modified_details_;
  std::unique_ptr<std::set<GURL>> favicon_changed_;

  DISALLOW_COPY_AND_ASSIGN(AndroidProviderBackendNotifier);
};

class AndroidProviderBackendTest : public testing::Test {
 public:
  AndroidProviderBackendTest()
      : thumbnail_db_(NULL),
        profile_manager_(TestingBrowserProcess::GetGlobal()),
        bookmark_model_(NULL) {}

  ~AndroidProviderBackendTest() override {
    // Avoid use after frees by running any unhandled tasks before freeing this
    // fixture's members.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void SetUp() override {
    // Setup the testing profile, so the bookmark_model_sql_handler could
    // get the bookmark model from it.
    ASSERT_TRUE(profile_manager_.SetUp());
    // It seems that the name has to be chrome::kInitialProfile, so it
    // could be found by ProfileManager::GetLastUsedProfile().
    TestingProfile* testing_profile = profile_manager_.CreateTestingProfile(
        chrome::kInitialProfile);
    testing_profile->CreateBookmarkModel(true);
    bookmark_model_ =
        BookmarkModelFactory::GetForBrowserContext(testing_profile);
    history_client_.reset(new ChromeHistoryClient(bookmark_model_));
    history_backend_client_ = history_client_->CreateBackendClient();
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);
    ASSERT_TRUE(bookmark_model_);

    // Get the BookmarkModel from LastUsedProfile, this is the same way that
    // how the BookmarkModelSQLHandler gets the BookmarkModel.
    Profile* profile = ProfileManager::GetLastUsedProfile();
    ASSERT_TRUE(profile);

    // Setup the database directory and files.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    history_db_name_ = temp_dir_.GetPath().AppendASCII(kHistoryFilename);
    thumbnail_db_name_ = temp_dir_.GetPath().AppendASCII(kFaviconsFilename);
    android_cache_db_name_ =
        temp_dir_.GetPath().AppendASCII("TestAndroidCache.db");
  }

  void AddBookmark(const GURL& url) {
    const BookmarkNode* mobile_node = bookmark_model_->mobile_node();
    ASSERT_TRUE(mobile_node);
    ASSERT_TRUE(bookmark_model_->AddURL(mobile_node, 0, base::string16(), url));
  }

  bool GetAndroidURLsRows(std::vector<AndroidURLRow>* rows,
                          AndroidProviderBackend* backend) {
    sql::Statement statement(backend->db_->GetCachedStatement(SQL_FROM_HERE,
        "SELECT id, raw_url, url_id FROM android_urls ORDER BY url_id ASC"));

    while (statement.Step()) {
      AndroidURLRow row;
      row.id = statement.ColumnInt64(0);
      row.raw_url = statement.ColumnString(1);
      row.url_id = statement.ColumnInt64(2);
      rows->push_back(row);
    }
    return true;
  }

  bool GetBookmarkCacheRows(std::vector<BookmarkCacheRow>* rows,
                            AndroidProviderBackend* backend) {
    sql::Statement statement(backend->db_->GetCachedStatement(SQL_FROM_HERE,
        "SELECT created_time, last_visit_time, url_id, bookmark, favicon_id "
        "FROM android_cache_db.bookmark_cache ORDER BY url_id ASC"));

    while (statement.Step()) {
      BookmarkCacheRow row;
      row.create_time_ = FromDatabaseTime(statement.ColumnInt64(0));
      row.last_visit_time_ = FromDatabaseTime(statement.ColumnInt64(1));
      row.url_id_ = statement.ColumnInt64(2);
      row.bookmark_ = statement.ColumnBool(3);
      row.favicon_id_ = statement.ColumnInt64(4);
      rows->push_back(row);
    }
    return true;
  }

  content::BrowserTaskEnvironment task_environment_;

  AndroidProviderBackendNotifier notifier_;
  scoped_refptr<HistoryBackend> history_backend_;
  TestHistoryDatabase history_db_;
  ThumbnailDatabase thumbnail_db_;
  base::ScopedTempDir temp_dir_;
  base::FilePath android_cache_db_name_;
  base::FilePath history_db_name_;
  base::FilePath thumbnail_db_name_;

  TestingProfileManager profile_manager_;
  BookmarkModel* bookmark_model_;
  std::unique_ptr<history::HistoryClient> history_client_;
  std::unique_ptr<history::HistoryBackendClient> history_backend_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidProviderBackendTest);
};

TEST_F(AndroidProviderBackendTest, IgnoreAboutBlank) {
  GURL url("about:blank");
  ASSERT_FALSE(history_client_->CanAddURL(url));
}

TEST_F(AndroidProviderBackendTest, UpdateTables) {
  GURL url1("http://www.cnn.com");
  URLID url_id1 = 0;
  std::vector<VisitInfo> visits1;
  Time last_visited1 = Time::Now() - TimeDelta::FromDays(1);
  Time created1 = last_visited1 - TimeDelta::FromDays(20);
  visits1.push_back(VisitInfo(created1, ui::PAGE_TRANSITION_LINK));
  visits1.push_back(VisitInfo(last_visited1 - TimeDelta::FromDays(1),
                              ui::PAGE_TRANSITION_LINK));
  visits1.push_back(VisitInfo(last_visited1, ui::PAGE_TRANSITION_LINK));

  GURL url2("http://www.example.com");
  URLID url_id2 = 0;
  std::vector<VisitInfo> visits2;
  Time last_visited2 = Time::Now();
  Time created2 = last_visited2 - TimeDelta::FromDays(10);
  visits2.push_back(VisitInfo(created2, ui::PAGE_TRANSITION_LINK));
  visits2.push_back(VisitInfo(last_visited2 - TimeDelta::FromDays(5),
                              ui::PAGE_TRANSITION_LINK));
  visits2.push_back(VisitInfo(last_visited2, ui::PAGE_TRANSITION_LINK));

  // Add a bookmark which is not in the history.
  GURL url3("http://www.bookmark.com");
  base::string16 title3(UTF8ToUTF16("bookmark"));
  ASSERT_TRUE(bookmark_model_->AddURL(bookmark_model_->bookmark_bar_node(), 0,
                                      title3, url3));
  // Only use the HistoryBackend to generate the test data.
  // HistoryBackend will shutdown after that.
  {
  scoped_refptr<HistoryBackend> history_backend;
  history_backend = base::MakeRefCounted<HistoryBackend>(
      std::make_unique<AndroidProviderBackendDelegate>(),
      history_client_->CreateBackendClient(),
      base::ThreadTaskRunnerHandle::Get());
  history_backend->Init(false,
                        TestHistoryDatabaseParamsForPath(temp_dir_.GetPath()));
  history_backend->AddVisits(url1, visits1, history::SOURCE_SYNCED);
  history_backend->AddVisits(url2, visits2, history::SOURCE_SYNCED);
  URLRow url_row;

  ASSERT_TRUE(history_backend->GetURL(url1, &url_row));
  url_id1 = url_row.id();
  ASSERT_TRUE(history_backend->GetURL(url2, &url_row));
  url_id2 = url_row.id();

  // Set favicon to url2.
  std::vector<SkBitmap> bitmaps(1u, CreateBitmap());
  history_backend->SetFavicons({url2}, favicon_base::IconType::kFavicon, GURL(),
                               bitmaps);
  history_backend->Closing();
  }

  // The history_db_name and thumbnail_db_name files should be created by
  // HistoryBackend. We need to open the same database files.
  ASSERT_TRUE(base::PathExists(history_db_name_));
  ASSERT_TRUE(base::PathExists(thumbnail_db_name_));

  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  ASSERT_EQ(sql::INIT_OK, thumbnail_db_.Init(thumbnail_db_name_));
  // Set url1 as bookmark.
  AddBookmark(url1);
  std::unique_ptr<AndroidProviderBackend> backend(new AndroidProviderBackend(
      android_cache_db_name_, &history_db_, &thumbnail_db_,
      history_backend_client_.get(), &notifier_));

  ASSERT_TRUE(backend->EnsureInitializedAndUpdated());

  // First verify that the bookmark which was not in the history has been added
  // to history database.
  URLRow url_row;
  ASSERT_TRUE(history_db_.GetRowForURL(url3, &url_row));
  URLID url_id3 = url_row.id();
  ASSERT_EQ(url3, url_row.url());
  ASSERT_EQ(title3, url_row.title());

  std::vector<AndroidURLRow> android_url_rows;
  ASSERT_TRUE(GetAndroidURLsRows(&android_url_rows, backend.get()));
  ASSERT_EQ(3u, android_url_rows.size());
  std::vector<AndroidURLRow>::iterator i = android_url_rows.begin();
  EXPECT_EQ(url_id1, i->url_id);
  EXPECT_EQ(url1.spec(), i->raw_url);
  i++;
  EXPECT_EQ(url_id2, i->url_id);
  EXPECT_EQ(url2.spec(), i->raw_url);
  i++;
  EXPECT_EQ(url_id3, i->url_id);
  EXPECT_EQ(url3.spec(), i->raw_url);

  std::vector<BookmarkCacheRow> bookmark_cache_rows;
  ASSERT_TRUE(GetBookmarkCacheRows(&bookmark_cache_rows, backend.get()));
  ASSERT_EQ(3u, bookmark_cache_rows.size());
  std::vector<BookmarkCacheRow>::const_iterator j = bookmark_cache_rows.begin();
  EXPECT_EQ(url_id1, j->url_id_);
  EXPECT_EQ(ToDatabaseTime(last_visited1), ToDatabaseTime(j->last_visit_time_));
  EXPECT_EQ(ToDatabaseTime(created1), ToDatabaseTime(j->create_time_));
  EXPECT_EQ(0, j->favicon_id_);
  EXPECT_TRUE(j->bookmark_);
  j++;
  EXPECT_EQ(url_id2, j->url_id_);
  EXPECT_EQ(ToDatabaseTime(last_visited2), ToDatabaseTime(j->last_visit_time_));
  EXPECT_EQ(ToDatabaseTime(created2), ToDatabaseTime(j->create_time_));
  EXPECT_NE(0, j->favicon_id_);
  EXPECT_FALSE(j->bookmark_);

  // Delete url2 from database.
  ASSERT_TRUE(history_db_.DeleteURLRow(url_id2));
  VisitVector visit_rows;
  ASSERT_TRUE(history_db_.GetMostRecentVisitsForURL(url_id2, 10,
                                                    &visit_rows));
  ASSERT_EQ(3u, visit_rows.size());
  for (VisitVector::const_iterator v = visit_rows.begin();
       v != visit_rows.end(); v++)
    history_db_.DeleteVisit(*v);

  backend->UpdateTables();

  android_url_rows.clear();
  ASSERT_TRUE(GetAndroidURLsRows(&android_url_rows, backend.get()));
  ASSERT_EQ(2u, android_url_rows.size());
  i = android_url_rows.begin();
  EXPECT_EQ(url_id1, i->url_id);
  EXPECT_EQ(url1.spec(), i->raw_url);
  ++i;
  EXPECT_EQ(url_id3, i->url_id);
  EXPECT_EQ(url3.spec(), i->raw_url);

  bookmark_cache_rows.clear();
  ASSERT_TRUE(GetBookmarkCacheRows(&bookmark_cache_rows, backend.get()));
  ASSERT_EQ(2u, bookmark_cache_rows.size());
  j = bookmark_cache_rows.begin();
  EXPECT_EQ(url_id1, j->url_id_);
  EXPECT_EQ(ToDatabaseTime(last_visited1), ToDatabaseTime(j->last_visit_time_));
  EXPECT_EQ(ToDatabaseTime(created1), ToDatabaseTime(j->create_time_));
  EXPECT_EQ(0, j->favicon_id_);
  EXPECT_TRUE(j->bookmark_);
  ++j;
  EXPECT_EQ(url_id3, j->url_id_);
  EXPECT_EQ(base::Time::UnixEpoch(), j->last_visit_time_);
  EXPECT_EQ(base::Time::UnixEpoch(), j->create_time_);
  EXPECT_EQ(0, j->favicon_id_);
  EXPECT_TRUE(j->bookmark_);
}

TEST_F(AndroidProviderBackendTest, QueryHistoryAndBookmarks) {
  GURL url1("http://www.cnn.com");
  const base::string16 title1(UTF8ToUTF16("cnn"));
  std::vector<VisitInfo> visits1;
  Time last_visited1 = Time::Now() - TimeDelta::FromDays(1);
  Time created1 = last_visited1 - TimeDelta::FromDays(20);
  visits1.push_back(VisitInfo(created1, ui::PAGE_TRANSITION_LINK));
  visits1.push_back(VisitInfo(last_visited1 - TimeDelta::FromDays(1),
                              ui::PAGE_TRANSITION_LINK));
  visits1.push_back(VisitInfo(last_visited1, ui::PAGE_TRANSITION_LINK));

  GURL url2("http://www.example.com");
  std::vector<VisitInfo> visits2;
  const base::string16 title2(UTF8ToUTF16("example"));
  Time last_visited2 = Time::Now();
  Time created2 = last_visited2 - TimeDelta::FromDays(10);
  visits2.push_back(VisitInfo(created2, ui::PAGE_TRANSITION_LINK));
  visits2.push_back(VisitInfo(last_visited2 - TimeDelta::FromDays(5),
                              ui::PAGE_TRANSITION_LINK));
  visits2.push_back(VisitInfo(last_visited2, ui::PAGE_TRANSITION_LINK));

  // Only use the HistoryBackend to generate the test data.
  // HistoryBackend will shutdown after that.
  {
  scoped_refptr<HistoryBackend> history_backend;
  history_backend = base::MakeRefCounted<HistoryBackend>(
      std::make_unique<AndroidProviderBackendDelegate>(),
      history_client_->CreateBackendClient(),
      base::ThreadTaskRunnerHandle::Get());
  history_backend->Init(false,
                        TestHistoryDatabaseParamsForPath(temp_dir_.GetPath()));
  history_backend->AddVisits(url1, visits1, history::SOURCE_SYNCED);
  history_backend->AddVisits(url2, visits2, history::SOURCE_SYNCED);

  history::URLRows url_rows(2u);
  ASSERT_TRUE(history_backend->GetURL(url1, &url_rows[0]));
  ASSERT_TRUE(history_backend->GetURL(url2, &url_rows[1]));
  url_rows[0].set_title(title1);
  url_rows[1].set_title(title2);
  ASSERT_EQ(2u, history_backend->UpdateURLs(url_rows));

  // Set favicon to url2.
  std::vector<SkBitmap> bitmaps(1u, CreateBitmap());
  history_backend->SetFavicons({url2}, favicon_base::IconType::kFavicon, GURL(),
                               bitmaps);
  history_backend->Closing();
  }

  // The history_db_name and thumbnail_db_name files should be created by
  // HistoryBackend. We need to open the same database files.
  ASSERT_TRUE(base::PathExists(history_db_name_));
  ASSERT_TRUE(base::PathExists(thumbnail_db_name_));

  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  ASSERT_EQ(sql::INIT_OK, thumbnail_db_.Init(thumbnail_db_name_));
  // Set url1 as bookmark.
  AddBookmark(url1);

  std::unique_ptr<AndroidProviderBackend> backend(new AndroidProviderBackend(
      android_cache_db_name_, &history_db_, &thumbnail_db_,
      history_backend_client_.get(), &notifier_));

  std::vector<HistoryAndBookmarkRow::ColumnID> projections;

  projections.push_back(HistoryAndBookmarkRow::ID);
  projections.push_back(HistoryAndBookmarkRow::URL);
  projections.push_back(HistoryAndBookmarkRow::TITLE);
  projections.push_back(HistoryAndBookmarkRow::CREATED);
  projections.push_back(HistoryAndBookmarkRow::LAST_VISIT_TIME);
  projections.push_back(HistoryAndBookmarkRow::VISIT_COUNT);
  projections.push_back(HistoryAndBookmarkRow::FAVICON);
  projections.push_back(HistoryAndBookmarkRow::BOOKMARK);

  std::unique_ptr<AndroidStatement> statement(backend->QueryHistoryAndBookmarks(
      projections, std::string(), std::vector<base::string16>(),
      std::string("url ASC")));
  ASSERT_TRUE(statement->statement()->Step());
  ASSERT_EQ(url1, GURL(statement->statement()->ColumnString(1)));
  EXPECT_EQ(title1, statement->statement()->ColumnString16(2));
  EXPECT_EQ(ToDatabaseTime(created1),
            statement->statement()->ColumnInt64(3));
  EXPECT_EQ(ToDatabaseTime(last_visited1),
            statement->statement()->ColumnInt64(4));
  EXPECT_EQ(3, statement->statement()->ColumnInt(5));
  EXPECT_EQ(6, statement->favicon_index());
  // No favicon.
  EXPECT_EQ(0, statement->statement()->ColumnByteLength(6));
  EXPECT_TRUE(statement->statement()->ColumnBool(7));

  ASSERT_TRUE(statement->statement()->Step());
  EXPECT_EQ(title2, statement->statement()->ColumnString16(2));
  ASSERT_EQ(url2, GURL(statement->statement()->ColumnString(1)));
  EXPECT_EQ(ToDatabaseTime(created2),
            statement->statement()->ColumnInt64(3));
  EXPECT_EQ(ToDatabaseTime(last_visited2),
            statement->statement()->ColumnInt64(4));
  EXPECT_EQ(3, statement->statement()->ColumnInt(5));
  std::vector<unsigned char> favicon2;
  EXPECT_EQ(6, statement->favicon_index());
  // Has favicon.
  EXPECT_NE(0, statement->statement()->ColumnByteLength(6));
  EXPECT_FALSE(statement->statement()->ColumnBool(7));

  // No more row.
  EXPECT_FALSE(statement->statement()->Step());

  // Query by bookmark
  statement.reset(backend->QueryHistoryAndBookmarks(projections, "bookmark=1",
      std::vector<base::string16>(), std::string("url ASC")));
  // Only URL1 is returned.
  ASSERT_TRUE(statement->statement()->Step());
  ASSERT_EQ(url1, GURL(statement->statement()->ColumnString(1)));
  EXPECT_FALSE(statement->statement()->Step());

  statement.reset(backend->QueryHistoryAndBookmarks(projections, "bookmark=0",
      std::vector<base::string16>(), std::string("url ASC")));
  // Only URL2 is returned.
  ASSERT_TRUE(statement->statement()->Step());
  ASSERT_EQ(url2, GURL(statement->statement()->ColumnString(1)));
  EXPECT_FALSE(statement->statement()->Step());
}

TEST_F(AndroidProviderBackendTest, InsertHistoryAndBookmark) {
  HistoryAndBookmarkRow row1;
  row1.set_raw_url("cnn.com");
  row1.set_url(GURL("http://cnn.com"));
  row1.set_last_visit_time(Time::Now() - TimeDelta::FromDays(1));
  row1.set_created(Time::Now() - TimeDelta::FromDays(20));
  row1.set_visit_count(10);
  row1.set_is_bookmark(true);
  row1.set_title(UTF8ToUTF16("cnn"));

  HistoryAndBookmarkRow row2;
  row2.set_raw_url("http://www.example.com");
  row2.set_url(GURL("http://www.example.com"));
  row2.set_last_visit_time(Time::Now() - TimeDelta::FromDays(10));
  row2.set_is_bookmark(false);
  row2.set_title(UTF8ToUTF16("example"));
  std::vector<unsigned char> data;
  data.push_back('1');
  row2.set_favicon(base::RefCountedBytes::TakeVector(&data));

  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  ASSERT_EQ(sql::INIT_OK, thumbnail_db_.Init(thumbnail_db_name_));
  std::unique_ptr<AndroidProviderBackend> backend(new AndroidProviderBackend(
      android_cache_db_name_, &history_db_, &thumbnail_db_,
      history_backend_client_.get(), &notifier_));

  ASSERT_TRUE(backend->InsertHistoryAndBookmark(row1));
  EXPECT_FALSE(notifier_.deleted_details());
  ASSERT_TRUE(notifier_.modified_details());
  ASSERT_EQ(1u, notifier_.modified_details()->size());
  EXPECT_EQ(row1.url(), (*notifier_.modified_details())[0].url());
  EXPECT_EQ(row1.last_visit_time(),
            (*notifier_.modified_details())[0].last_visit());
  EXPECT_EQ(row1.visit_count(),
            (*notifier_.modified_details())[0].visit_count());
  EXPECT_EQ(row1.title(),
            (*notifier_.modified_details())[0].title());
  EXPECT_FALSE(notifier_.favicon_changed());
  content::RunAllPendingInMessageLoop();
  ASSERT_EQ(1u, bookmark_model_->mobile_node()->children().size());
  const BookmarkNode* child =
      bookmark_model_->mobile_node()->children().front().get();
  ASSERT_TRUE(child);
  EXPECT_EQ(row1.title(), child->GetTitle());
  EXPECT_EQ(row1.url(), child->url());

  notifier_.ResetDetails();
  ASSERT_TRUE(backend->InsertHistoryAndBookmark(row2));
  EXPECT_FALSE(notifier_.deleted_details());
  ASSERT_TRUE(notifier_.modified_details());
  ASSERT_EQ(1u, notifier_.modified_details()->size());
  EXPECT_EQ(row2.url(), (*notifier_.modified_details())[0].url());
  EXPECT_EQ(row2.last_visit_time(),
            (*notifier_.modified_details())[0].last_visit());
  EXPECT_EQ(row2.title(),
            (*notifier_.modified_details())[0].title());
  ASSERT_TRUE(notifier_.favicon_changed());
  ASSERT_EQ(1u, notifier_.favicon_changed()->size());
  ASSERT_TRUE(notifier_.favicon_changed()->end() !=
              notifier_.favicon_changed()->find(row2.url()));

  std::vector<HistoryAndBookmarkRow::ColumnID> projections;
  projections.push_back(HistoryAndBookmarkRow::ID);
  projections.push_back(HistoryAndBookmarkRow::URL);
  projections.push_back(HistoryAndBookmarkRow::TITLE);
  projections.push_back(HistoryAndBookmarkRow::CREATED);
  projections.push_back(HistoryAndBookmarkRow::LAST_VISIT_TIME);
  projections.push_back(HistoryAndBookmarkRow::VISIT_COUNT);
  projections.push_back(HistoryAndBookmarkRow::FAVICON);
  projections.push_back(HistoryAndBookmarkRow::BOOKMARK);

  std::unique_ptr<AndroidStatement> statement(backend->QueryHistoryAndBookmarks(
      projections, std::string(), std::vector<base::string16>(),
      std::string("url ASC")));
  ASSERT_TRUE(statement->statement()->Step());
  ASSERT_EQ(row1.raw_url(), statement->statement()->ColumnString(1));
  EXPECT_EQ(row1.title(), statement->statement()->ColumnString16(2));
  EXPECT_EQ(ToDatabaseTime(row1.created()),
            statement->statement()->ColumnInt64(3));
  EXPECT_EQ(ToDatabaseTime(row1.last_visit_time()),
            statement->statement()->ColumnInt64(4));
  EXPECT_EQ(row1.visit_count(), statement->statement()->ColumnInt(5));
  EXPECT_EQ(6, statement->favicon_index());
  // No favicon.
  EXPECT_EQ(0, statement->statement()->ColumnByteLength(6));

  // TODO: Find a way to test the bookmark was added in BookmarkModel.
  // The bookmark was added in UI thread, there is no good way to test it.
  EXPECT_TRUE(statement->statement()->ColumnBool(7));

  ASSERT_TRUE(statement->statement()->Step());
  EXPECT_EQ(row2.title(), statement->statement()->ColumnString16(2));
  EXPECT_EQ(row2.url(), GURL(statement->statement()->ColumnString(1)));
  EXPECT_EQ(ToDatabaseTime(row2.last_visit_time()),
            statement->statement()->ColumnInt64(3));
  EXPECT_EQ(ToDatabaseTime(row2.last_visit_time()),
            statement->statement()->ColumnInt64(4));
  EXPECT_EQ(1, statement->statement()->ColumnInt(5));
  EXPECT_EQ(6, statement->favicon_index());
  // Has favicon.
  EXPECT_NE(0, statement->statement()->ColumnByteLength(6));
  // TODO: Find a way to test the bookmark was added in BookmarkModel.
  // The bookmark was added in UI thread, there is no good way to test it.
  EXPECT_FALSE(statement->statement()->ColumnBool(7));

  // No more row.
  EXPECT_FALSE(statement->statement()->Step());
}

TEST_F(AndroidProviderBackendTest, DeleteHistoryAndBookmarks) {
  HistoryAndBookmarkRow row1;
  row1.set_raw_url("cnn.com");
  row1.set_url(GURL("http://cnn.com"));
  row1.set_last_visit_time(Time::Now() - TimeDelta::FromDays(1));
  row1.set_created(Time::Now() - TimeDelta::FromDays(20));
  row1.set_visit_count(10);
  row1.set_is_bookmark(true);
  row1.set_title(UTF8ToUTF16("cnn"));

  HistoryAndBookmarkRow row2;
  row2.set_raw_url("http://www.example.com");
  row2.set_url(GURL("http://www.example.com"));
  row2.set_last_visit_time(Time::Now() - TimeDelta::FromDays(10));
  row2.set_is_bookmark(false);
  row2.set_title(UTF8ToUTF16("example"));
  std::vector<unsigned char> data;
  data.push_back('1');
  row2.set_favicon(base::RefCountedBytes::TakeVector(&data));

  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  ASSERT_EQ(sql::INIT_OK, thumbnail_db_.Init(thumbnail_db_name_));

  std::unique_ptr<AndroidProviderBackend> backend(new AndroidProviderBackend(
      android_cache_db_name_, &history_db_, &thumbnail_db_,
      history_backend_client_.get(), &notifier_));

  ASSERT_TRUE(backend->InsertHistoryAndBookmark(row1));
  ASSERT_TRUE(backend->InsertHistoryAndBookmark(row2));
  // Verify the row1 has been added in bookmark model.
  content::RunAllPendingInMessageLoop();
  ASSERT_EQ(1u, bookmark_model_->mobile_node()->children().size());
  const BookmarkNode* child =
      bookmark_model_->mobile_node()->children().front().get();
  ASSERT_TRUE(child);
  EXPECT_EQ(row1.title(), child->GetTitle());
  EXPECT_EQ(row1.url(), child->url());

  // Delete the row1.
  std::vector<base::string16> args;
  int deleted_count = 0;
  notifier_.ResetDetails();
  ASSERT_TRUE(backend->DeleteHistoryAndBookmarks("Favicon IS NULL", args,
                                                 &deleted_count));
  EXPECT_EQ(1, deleted_count);
  // Verify the row1 was removed from bookmark model.
  content::RunAllPendingInMessageLoop();
  ASSERT_EQ(0u, bookmark_model_->mobile_node()->children().size());

  // Verify notifications
  ASSERT_TRUE(notifier_.deleted_details());
  EXPECT_FALSE(notifier_.modified_details());
  EXPECT_EQ(1u, notifier_.deleted_details()->size());
  EXPECT_EQ(row1.url(), (*notifier_.deleted_details())[0].url());
  EXPECT_EQ(row1.last_visit_time(),
            (*notifier_.deleted_details())[0].last_visit());
  EXPECT_EQ(row1.title(),
            (*notifier_.deleted_details())[0].title());
  EXPECT_FALSE(notifier_.favicon_changed());

  std::vector<HistoryAndBookmarkRow::ColumnID> projections;
  projections.push_back(HistoryAndBookmarkRow::ID);
  projections.push_back(HistoryAndBookmarkRow::URL);
  projections.push_back(HistoryAndBookmarkRow::TITLE);
  projections.push_back(HistoryAndBookmarkRow::CREATED);
  projections.push_back(HistoryAndBookmarkRow::LAST_VISIT_TIME);
  projections.push_back(HistoryAndBookmarkRow::VISIT_COUNT);
  projections.push_back(HistoryAndBookmarkRow::FAVICON);
  projections.push_back(HistoryAndBookmarkRow::BOOKMARK);

  std::unique_ptr<AndroidStatement> statement(backend->QueryHistoryAndBookmarks(
      projections, std::string(), std::vector<base::string16>(),
      std::string("url ASC")));
  ASSERT_TRUE(statement->statement()->Step());

  EXPECT_EQ(row2.title(), statement->statement()->ColumnString16(2));
  EXPECT_EQ(row2.url(), GURL(statement->statement()->ColumnString(1)));
  EXPECT_EQ(ToDatabaseTime(row2.last_visit_time()),
            statement->statement()->ColumnInt64(3));
  EXPECT_EQ(ToDatabaseTime(row2.last_visit_time()),
            statement->statement()->ColumnInt64(4));
  EXPECT_EQ(1, statement->statement()->ColumnInt(5));
  EXPECT_EQ(6, statement->favicon_index());
  // Has favicon.
  EXPECT_NE(0, statement->statement()->ColumnByteLength(6));
  // TODO: Find a way to test the bookmark was added in BookmarkModel.
  // The bookmark was added in UI thread, there is no good way to test it.
  EXPECT_FALSE(statement->statement()->ColumnBool(7));
  // No more row.
  EXPECT_FALSE(statement->statement()->Step());

  deleted_count = 0;
  // Delete row2.
  notifier_.ResetDetails();
  ASSERT_TRUE(backend->DeleteHistoryAndBookmarks("bookmark = 0",
                  std::vector<base::string16>(), &deleted_count));
  // Verify notifications
  ASSERT_TRUE(notifier_.deleted_details());
  EXPECT_FALSE(notifier_.modified_details());
  EXPECT_EQ(1u, notifier_.deleted_details()->size());
  EXPECT_EQ(row2.url(), (*notifier_.deleted_details())[0].url());
  EXPECT_EQ(row2.last_visit_time(),
            (*notifier_.deleted_details())[0].last_visit());
  EXPECT_EQ(row2.title(),
            (*notifier_.deleted_details())[0].title());
  ASSERT_TRUE(notifier_.favicon_changed());
  ASSERT_EQ(1u, notifier_.favicon_changed()->size());
  ASSERT_TRUE(notifier_.favicon_changed()->end() !=
              notifier_.favicon_changed()->find(row2.url()));

  ASSERT_EQ(1, deleted_count);
  std::unique_ptr<AndroidStatement> statement1(
      backend->QueryHistoryAndBookmarks(projections, std::string(),
                                        std::vector<base::string16>(),
                                        std::string("url ASC")));
  ASSERT_FALSE(statement1->statement()->Step());
}

TEST_F(AndroidProviderBackendTest, IsValidHistoryAndBookmarkRow) {
  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  ASSERT_EQ(sql::INIT_OK, thumbnail_db_.Init(thumbnail_db_name_));
  std::unique_ptr<AndroidProviderBackend> backend(new AndroidProviderBackend(
      android_cache_db_name_, &history_db_, &thumbnail_db_,
      history_backend_client_.get(), &notifier_));

  // The created time and last visit time are too close to have required visit
  // count.
  HistoryAndBookmarkRow row1;
  row1.set_raw_url("cnn.com");
  row1.set_url(GURL("http://cnn.com"));
  row1.set_last_visit_time(Time::Now() - TimeDelta::FromDays(1));
  row1.set_created(Time::FromInternalValue(
      row1.last_visit_time().ToInternalValue() - 1));
  row1.set_visit_count(10);
  row1.set_is_bookmark(true);
  row1.set_title(UTF8ToUTF16("cnn"));
  EXPECT_FALSE(backend->InsertHistoryAndBookmark(row1));

  // Have different created time and last visit time, but only have 1 visit
  // count.
  HistoryAndBookmarkRow row2;
  row2.set_raw_url("http://www.example.com");
  row2.set_url(GURL("http://www.example.com"));
  row2.set_last_visit_time(Time::Now() - TimeDelta::FromDays(10));
  row2.set_created(Time::Now() - TimeDelta::FromDays(11));
  row2.set_visit_count(1);
  EXPECT_FALSE(backend->InsertHistoryAndBookmark(row2));

  // Have created time in the future.
  HistoryAndBookmarkRow row3;
  row3.set_raw_url("http://www.example.com");
  row3.set_url(GURL("http://www.example.com"));
  row3.set_created(Time::Now() + TimeDelta::FromDays(11));
  EXPECT_FALSE(backend->InsertHistoryAndBookmark(row3));

  // Have last vist time in the future.
  HistoryAndBookmarkRow row4;
  row4.set_raw_url("http://www.example.com");
  row4.set_url(GURL("http://www.example.com"));
  row4.set_last_visit_time(Time::Now() + TimeDelta::FromDays(11));
  EXPECT_FALSE(backend->InsertHistoryAndBookmark(row4));

  // Created time is larger than last visit time.
  HistoryAndBookmarkRow row5;
  row5.set_raw_url("http://www.example.com");
  row5.set_url(GURL("http://www.example.com"));
  row5.set_last_visit_time(Time::Now());
  row5.set_created(Time::Now() + TimeDelta::FromDays(11));
  EXPECT_FALSE(backend->InsertHistoryAndBookmark(row5));

  // Visit count is zero, and last visit time is not zero.
  HistoryAndBookmarkRow row6;
  row6.set_raw_url("http://www.example.com");
  row6.set_url(GURL("http://www.example.com"));
  row6.set_visit_count(0);
  row6.set_last_visit_time(Time::Now());
  row6.set_created(Time::Now() - TimeDelta::FromDays(1));
  EXPECT_FALSE(backend->InsertHistoryAndBookmark(row6));

  // Visit count is zero, and create time is not zero.
  HistoryAndBookmarkRow row7;
  row7.set_raw_url("http://www.example.com");
  row7.set_url(GURL("http://www.example.com"));
  row7.set_visit_count(0);
  row7.set_last_visit_time(Time::Now());
  row7.set_created(Time::UnixEpoch());
  EXPECT_TRUE(backend->InsertHistoryAndBookmark(row7));
}

TEST_F(AndroidProviderBackendTest, UpdateURL) {
  HistoryAndBookmarkRow row1;
  row1.set_raw_url("cnn.com");
  row1.set_url(GURL("http://cnn.com"));
  row1.set_last_visit_time(Time::Now() - TimeDelta::FromDays(1));
  row1.set_created(Time::Now() - TimeDelta::FromDays(20));
  row1.set_visit_count(10);
  row1.set_is_bookmark(true);
  row1.set_title(UTF8ToUTF16("cnn"));

  HistoryAndBookmarkRow row2;
  row2.set_raw_url("http://www.example.com");
  row2.set_url(GURL("http://www.example.com"));
  row2.set_last_visit_time(Time::Now() - TimeDelta::FromDays(10));
  row2.set_is_bookmark(false);
  row2.set_title(UTF8ToUTF16("example"));
  std::vector<unsigned char> data;
  data.push_back('1');
  row2.set_favicon(base::RefCountedBytes::TakeVector(&data));

  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  ASSERT_EQ(sql::INIT_OK, thumbnail_db_.Init(thumbnail_db_name_));
  std::unique_ptr<AndroidProviderBackend> backend(new AndroidProviderBackend(
      android_cache_db_name_, &history_db_, &thumbnail_db_,
      history_backend_client_.get(), &notifier_));

  AndroidURLID id1 = backend->InsertHistoryAndBookmark(row1);
  ASSERT_TRUE(id1);
  AndroidURLID id2 = backend->InsertHistoryAndBookmark(row2);
  ASSERT_TRUE(id2);

  // Verify the row1 has been added in bookmark model.
  content::RunAllPendingInMessageLoop();
  ASSERT_EQ(1u, bookmark_model_->mobile_node()->children().size());
  const BookmarkNode* child =
      bookmark_model_->mobile_node()->children().front().get();
  ASSERT_TRUE(child);
  EXPECT_EQ(row1.title(), child->GetTitle());
  EXPECT_EQ(row1.url(), child->url());

  // Make sure the url has correctly insertted.
  URLID url_id1 = history_db_.GetRowForURL(row1.url(), NULL);
  ASSERT_TRUE(url_id1);
  URLID url_id2 = history_db_.GetRowForURL(row2.url(), NULL);
  ASSERT_TRUE(url_id2);

  // Make sure we have the correct visit rows in visit table.
  VisitVector visits;
  ASSERT_TRUE(history_db_.GetVisitsForURL(url_id1, &visits));
  ASSERT_EQ(10u, visits.size());
  visits.clear();
  ASSERT_TRUE(history_db_.GetVisitsForURL(url_id2, &visits));
  ASSERT_EQ(1u, visits.size());

  int update_count;
  std::vector<base::string16> update_args;
  // Try to update the mutiple rows with the same URL, this should failed.
  HistoryAndBookmarkRow update_row1;
  update_row1.set_raw_url("newwebiste.com");
  update_row1.set_url(GURL("http://newwebsite.com"));
  update_args.clear();
  ASSERT_FALSE(backend->UpdateHistoryAndBookmarks(update_row1, std::string(),
                                                  update_args, &update_count));

  // Only update one URL.
  update_args.clear();
  update_args.push_back(UTF8ToUTF16(row1.raw_url()));
  notifier_.ResetDetails();
  ASSERT_TRUE(backend->UpdateHistoryAndBookmarks(update_row1, "url = ?",
                                                 update_args, &update_count));
  // Verify notifications, Update involves insert and delete URLS.
  ASSERT_TRUE(notifier_.deleted_details());
  EXPECT_EQ(1u, notifier_.deleted_details()->size());
  EXPECT_EQ(row1.url(), (*notifier_.deleted_details())[0].url());
  EXPECT_EQ(row1.last_visit_time(),
            (*notifier_.deleted_details())[0].last_visit());
  EXPECT_EQ(row1.title(),
            (*notifier_.deleted_details())[0].title());
  ASSERT_TRUE(notifier_.modified_details());
  ASSERT_EQ(1u, notifier_.modified_details()->size());
  EXPECT_EQ(update_row1.url(),
            (*notifier_.modified_details())[0].url());
  EXPECT_EQ(ToDatabaseTime(row1.last_visit_time()),
            ToDatabaseTime(
                (*notifier_.modified_details())[0].last_visit()));
  EXPECT_EQ(row1.title(),
            (*notifier_.modified_details())[0].title());
  EXPECT_FALSE(notifier_.favicon_changed());

  EXPECT_EQ(1, update_count);
  // We shouldn't find orignal url anymore.
  EXPECT_FALSE(history_db_.GetRowForURL(row1.url(), NULL));
  visits.clear();
  EXPECT_TRUE(history_db_.GetVisitsForURL(url_id1, &visits));
  EXPECT_EQ(0u, visits.size());
  // Verify new URL.
  URLRow new_row;
  EXPECT_TRUE(history_db_.GetRowForURL(update_row1.url(), &new_row));
  EXPECT_EQ(10, new_row.visit_count());
  EXPECT_EQ(ToDatabaseTime(row1.last_visit_time()),
            ToDatabaseTime(new_row.last_visit()));
  visits.clear();
  EXPECT_TRUE(history_db_.GetVisitsForURL(new_row.id(), &visits));
  EXPECT_EQ(10u, visits.size());
  AndroidURLRow android_url_row1;
  ASSERT_TRUE(history_db_.GetAndroidURLRow(new_row.id(), &android_url_row1));
  // Android URL ID shouldn't change.
  EXPECT_EQ(id1, android_url_row1.id);

  // Verify the bookmark model was updated.
  content::RunAllPendingInMessageLoop();
  ASSERT_EQ(1u, bookmark_model_->mobile_node()->children().size());
  const BookmarkNode* child1 =
      bookmark_model_->mobile_node()->children().front().get();
  ASSERT_TRUE(child1);
  EXPECT_EQ(row1.title(), child1->GetTitle());
  EXPECT_EQ(update_row1.url(), child1->url());

  // Update the URL with visit count, created time, and last visit time.
  HistoryAndBookmarkRow update_row2;
  update_row2.set_raw_url("somethingelse.com");
  update_row2.set_url(GURL("http://somethingelse.com"));
  update_row2.set_last_visit_time(Time::Now());
  update_row2.set_created(Time::Now() - TimeDelta::FromDays(20));
  update_row2.set_visit_count(10);

  update_args.clear();
  update_args.push_back(UTF8ToUTF16(row2.raw_url()));
  notifier_.ResetDetails();
  ASSERT_TRUE(backend->UpdateHistoryAndBookmarks(update_row2, "url = ?",
                                                 update_args, &update_count));
  // Verify notifications, Update involves insert and delete URLS.
  ASSERT_TRUE(notifier_.deleted_details());
  EXPECT_EQ(1u, notifier_.deleted_details()->size());
  EXPECT_EQ(row2.url(), (*notifier_.deleted_details())[0].url());
  EXPECT_EQ(row2.last_visit_time(),
            (*notifier_.deleted_details())[0].last_visit());
  EXPECT_EQ(row2.title(),
            (*notifier_.deleted_details())[0].title());
  ASSERT_TRUE(notifier_.modified_details());
  ASSERT_EQ(1u, notifier_.modified_details()->size());
  EXPECT_EQ(update_row2.url(),
            (*notifier_.modified_details())[0].url());
  EXPECT_EQ(ToDatabaseTime(update_row2.last_visit_time()),
            ToDatabaseTime(
                (*notifier_.modified_details())[0].last_visit()));
  EXPECT_EQ(update_row2.visit_count(),
            (*notifier_.modified_details())[0].visit_count());
  ASSERT_TRUE(notifier_.favicon_changed());
  ASSERT_EQ(2u, notifier_.favicon_changed()->size());
  ASSERT_TRUE(notifier_.favicon_changed()->end() !=
              notifier_.favicon_changed()->find(row2.url()));
  ASSERT_TRUE(notifier_.favicon_changed()->end() !=
              notifier_.favicon_changed()->find(update_row2.url()));

  EXPECT_EQ(1, update_count);
  // We shouldn't find orignal url anymore.
  EXPECT_FALSE(history_db_.GetRowForURL(row2.url(), NULL));
  visits.clear();
  EXPECT_TRUE(history_db_.GetVisitsForURL(url_id2, &visits));
  EXPECT_EQ(0u, visits.size());

  // Verify new URL.
  URLRow new_row2;
  ASSERT_TRUE(history_db_.GetRowForURL(update_row2.url(), &new_row2));
  EXPECT_EQ(10, new_row2.visit_count());
  EXPECT_EQ(update_row2.last_visit_time(), new_row2.last_visit());
  visits.clear();
  EXPECT_TRUE(history_db_.GetVisitsForURL(new_row2.id(), &visits));
  EXPECT_EQ(10u, visits.size());
  AndroidURLRow android_url_row2;
  ASSERT_TRUE(history_db_.GetAndroidURLRow(new_row2.id(), &android_url_row2));
  // Android URL ID shouldn't change.
  EXPECT_EQ(id2, android_url_row2.id);

  ASSERT_TRUE(history_db_.GetVisitsForURL(new_row2.id(), &visits));
  ASSERT_EQ(10u, visits.size());
  EXPECT_EQ(update_row2.created(), visits[0].visit_time);
  EXPECT_EQ(update_row2.last_visit_time(), visits[9].visit_time);
}

TEST_F(AndroidProviderBackendTest, UpdateVisitCount) {
  HistoryAndBookmarkRow row1;
  row1.set_raw_url("cnn.com");
  row1.set_url(GURL("http://cnn.com"));
  row1.set_last_visit_time(Time::Now() - TimeDelta::FromDays(1));
  row1.set_created(Time::Now() - TimeDelta::FromDays(20));
  row1.set_visit_count(10);
  row1.set_is_bookmark(true);
  row1.set_title(UTF8ToUTF16("cnn"));

  HistoryAndBookmarkRow row2;
  row2.set_raw_url("http://www.example.com");
  row2.set_url(GURL("http://www.example.com"));
  row2.set_last_visit_time(Time::Now() - TimeDelta::FromDays(10));
  row2.set_is_bookmark(false);
  row2.set_title(UTF8ToUTF16("example"));
  std::vector<unsigned char> data;
  data.push_back('1');
  row2.set_favicon(base::RefCountedBytes::TakeVector(&data));

  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  ASSERT_EQ(sql::INIT_OK, thumbnail_db_.Init(thumbnail_db_name_));
  std::unique_ptr<AndroidProviderBackend> backend(new AndroidProviderBackend(
      android_cache_db_name_, &history_db_, &thumbnail_db_,
      history_backend_client_.get(), &notifier_));

  AndroidURLID id1 = backend->InsertHistoryAndBookmark(row1);
  ASSERT_TRUE(id1);
  AndroidURLID id2 = backend->InsertHistoryAndBookmark(row2);
  ASSERT_TRUE(id2);

  int update_count;
  std::vector<base::string16> update_args;
  // Update the visit_count to a value less than current one.
  HistoryAndBookmarkRow update_row1;
  update_row1.set_visit_count(5);
  update_args.push_back(UTF8ToUTF16(row1.raw_url()));
  notifier_.ResetDetails();
  ASSERT_TRUE(backend->UpdateHistoryAndBookmarks(update_row1, "url = ?",
                                                 update_args, &update_count));
  // Verify notifications, Update involves modified URL.
  EXPECT_FALSE(notifier_.deleted_details());
  ASSERT_TRUE(notifier_.modified_details());
  ASSERT_EQ(1u, notifier_.modified_details()->size());
  EXPECT_EQ(row1.url(),
            (*notifier_.modified_details())[0].url());
  EXPECT_EQ(ToDatabaseTime(row1.last_visit_time()),
            ToDatabaseTime(
                (*notifier_.modified_details())[0].last_visit()));
  EXPECT_EQ(update_row1.visit_count(),
            (*notifier_.modified_details())[0].visit_count());
  EXPECT_FALSE(notifier_.favicon_changed());

  // All visits should be removed, and 5 new visit insertted.
  URLRow new_row1;
  ASSERT_TRUE(history_db_.GetRowForURL(row1.url(), &new_row1));
  EXPECT_EQ(5, new_row1.visit_count());
  VisitVector visits;
  ASSERT_TRUE(history_db_.GetVisitsForURL(new_row1.id(), &visits));
  ASSERT_EQ(5u, visits.size());
  EXPECT_EQ(row1.last_visit_time(), visits[4].visit_time);
  EXPECT_GT(row1.last_visit_time(), visits[0].visit_time);

  // Update the visit_count to a value equal to current one.
  HistoryAndBookmarkRow update_row2;
  update_row2.set_visit_count(1);
  update_args.clear();
  update_args.push_back(UTF8ToUTF16(row2.raw_url()));
  ASSERT_TRUE(backend->UpdateHistoryAndBookmarks(update_row2, "url = ?",
                                                 update_args, &update_count));
  // All shouldn't have any change.
  URLRow new_row2;
  ASSERT_TRUE(history_db_.GetRowForURL(row2.url(), &new_row2));
  EXPECT_EQ(1, new_row2.visit_count());

  ASSERT_TRUE(history_db_.GetVisitsForURL(new_row2.id(), &visits));
  ASSERT_EQ(1u, visits.size());
  EXPECT_EQ(row2.last_visit_time(), visits[0].visit_time);
}

TEST_F(AndroidProviderBackendTest, UpdateLastVisitTime) {
  HistoryAndBookmarkRow row1;
  row1.set_raw_url("cnn.com");
  row1.set_url(GURL("http://cnn.com"));
  row1.set_last_visit_time(Time::Now() - TimeDelta::FromDays(1));
  row1.set_created(Time::Now() - TimeDelta::FromDays(20));
  row1.set_visit_count(10);
  row1.set_is_bookmark(true);
  row1.set_title(UTF8ToUTF16("cnn"));

  HistoryAndBookmarkRow row2;
  row2.set_raw_url("http://www.example.com");
  row2.set_url(GURL("http://www.example.com"));
  row2.set_last_visit_time(Time::Now() - TimeDelta::FromDays(10));
  row2.set_is_bookmark(false);
  row2.set_title(UTF8ToUTF16("example"));
  std::vector<unsigned char> data;
  data.push_back('1');
  row2.set_favicon(base::RefCountedBytes::TakeVector(&data));

  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  ASSERT_EQ(sql::INIT_OK, thumbnail_db_.Init(thumbnail_db_name_));
  std::unique_ptr<AndroidProviderBackend> backend(new AndroidProviderBackend(
      android_cache_db_name_, &history_db_, &thumbnail_db_,
      history_backend_client_.get(), &notifier_));

  AndroidURLID id1 = backend->InsertHistoryAndBookmark(row1);
  ASSERT_TRUE(id1);
  AndroidURLID id2 = backend->InsertHistoryAndBookmark(row2);
  ASSERT_TRUE(id2);

  int update_count;
  std::vector<base::string16> update_args;
  // Update the last visit time to a value greater than current one.
  HistoryAndBookmarkRow update_row1;
  update_row1.set_last_visit_time(Time::Now());
  update_args.push_back(UTF8ToUTF16(row1.raw_url()));
  notifier_.ResetDetails();
  ASSERT_TRUE(backend->UpdateHistoryAndBookmarks(update_row1, "url = ?",
                                                 update_args, &update_count));
  // Verify notifications, Update involves modified URL.
  EXPECT_FALSE(notifier_.deleted_details());
  ASSERT_TRUE(notifier_.modified_details());
  ASSERT_EQ(1u, notifier_.modified_details()->size());
  EXPECT_EQ(row1.url(),
            (*notifier_.modified_details())[0].url());
  EXPECT_EQ(ToDatabaseTime(update_row1.last_visit_time()),
            ToDatabaseTime(
                (*notifier_.modified_details())[0].last_visit()));
  EXPECT_FALSE(notifier_.favicon_changed());

  URLRow new_row1;
  ASSERT_TRUE(history_db_.GetRowForURL(row1.url(), &new_row1));
  EXPECT_EQ(11, new_row1.visit_count());
  EXPECT_EQ(update_row1.last_visit_time(), new_row1.last_visit());
  VisitVector visits;
  ASSERT_TRUE(history_db_.GetVisitsForURL(new_row1.id(), &visits));
  // 1 new visit insertted.
  ASSERT_EQ(11u, visits.size());
  EXPECT_EQ(update_row1.last_visit_time(), visits[10].visit_time);
  EXPECT_EQ(row1.last_visit_time(), visits[9].visit_time);

  // Update the visit_tim to a value less than to current one.
  HistoryAndBookmarkRow update_row2;
  update_row2.set_last_visit_time(Time::Now() - TimeDelta::FromDays(1));
  update_args.clear();
  update_args.push_back(UTF8ToUTF16(row1.raw_url()));
  ASSERT_FALSE(backend->UpdateHistoryAndBookmarks(update_row2, "url = ?",
                                                  update_args, &update_count));
}

TEST_F(AndroidProviderBackendTest, UpdateFavicon) {
  HistoryAndBookmarkRow row1;
  row1.set_raw_url("cnn.com");
  row1.set_url(GURL("http://cnn.com"));
  row1.set_last_visit_time(Time::Now() - TimeDelta::FromDays(1));
  row1.set_created(Time::Now() - TimeDelta::FromDays(20));
  row1.set_visit_count(10);
  row1.set_is_bookmark(true);
  row1.set_title(UTF8ToUTF16("cnn"));

  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  ASSERT_EQ(sql::INIT_OK, thumbnail_db_.Init(thumbnail_db_name_));
  std::unique_ptr<AndroidProviderBackend> backend(new AndroidProviderBackend(
      android_cache_db_name_, &history_db_, &thumbnail_db_,
      history_backend_client_.get(), &notifier_));

  AndroidURLID id1 = backend->InsertHistoryAndBookmark(row1);
  ASSERT_TRUE(id1);

  int update_count;
  std::vector<base::string16> update_args;
  // Update the last visit time to a value greater than current one.
  HistoryAndBookmarkRow update_row1;

  // Set favicon.
  std::vector<unsigned char> data;
  data.push_back('1');
  update_row1.set_favicon(base::RefCountedBytes::TakeVector(&data));
  update_args.push_back(UTF8ToUTF16(row1.raw_url()));
  notifier_.ResetDetails();
  ASSERT_TRUE(backend->UpdateHistoryAndBookmarks(update_row1, "url = ?",
                                                 update_args, &update_count));
  // Verify notifications.
  EXPECT_FALSE(notifier_.deleted_details());
  EXPECT_FALSE(notifier_.modified_details());
  ASSERT_TRUE(notifier_.favicon_changed());
  ASSERT_EQ(1u, notifier_.favicon_changed()->size());
  ASSERT_TRUE(notifier_.favicon_changed()->end() !=
              notifier_.favicon_changed()->find(row1.url()));

  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(thumbnail_db_.GetIconMappingsForPageURL(
      row1.url(), {favicon_base::IconType::kFavicon}, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  std::vector<FaviconBitmap> favicon_bitmaps;
  EXPECT_TRUE(thumbnail_db_.GetFaviconBitmaps(icon_mappings[0].icon_id,
                                              &favicon_bitmaps));
  EXPECT_EQ(1u, favicon_bitmaps.size());
  EXPECT_TRUE(favicon_bitmaps[0].bitmap_data.get());
  EXPECT_EQ(1u, favicon_bitmaps[0].bitmap_data->size());
  EXPECT_EQ('1', *favicon_bitmaps[0].bitmap_data->front());

  // Remove favicon.
  HistoryAndBookmarkRow update_row2;

  // Set favicon.
  update_row1.set_favicon(new base::RefCountedBytes());
  update_args.clear();
  update_args.push_back(UTF8ToUTF16(row1.raw_url()));
  notifier_.ResetDetails();
  ASSERT_TRUE(backend->UpdateHistoryAndBookmarks(update_row1, "url = ?",
                                                 update_args, &update_count));
  // Verify notifications.
  EXPECT_FALSE(notifier_.deleted_details());
  EXPECT_FALSE(notifier_.modified_details());
  ASSERT_TRUE(notifier_.favicon_changed());
  ASSERT_EQ(1u, notifier_.favicon_changed()->size());
  ASSERT_TRUE(notifier_.favicon_changed()->end() !=
              notifier_.favicon_changed()->find(row1.url()));

  EXPECT_FALSE(thumbnail_db_.GetIconMappingsForPageURL(
      row1.url(), {favicon_base::IconType::kFavicon}, NULL));
}

TEST_F(AndroidProviderBackendTest, UpdateSearchTermTable) {
  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  ASSERT_EQ(sql::INIT_OK, thumbnail_db_.Init(thumbnail_db_name_));
  std::unique_ptr<AndroidProviderBackend> backend(new AndroidProviderBackend(
      android_cache_db_name_, &history_db_, &thumbnail_db_,
      history_backend_client_.get(), &notifier_));
  // Insert a keyword search item to verify if the update succeeds.
  HistoryAndBookmarkRow row1;
  row1.set_raw_url("cnn.com");
  row1.set_url(GURL("http://cnn.com"));
  row1.set_last_visit_time(Time::Now() - TimeDelta::FromDays(1));
  row1.set_title(UTF8ToUTF16("cnn"));
  ASSERT_TRUE(backend->InsertHistoryAndBookmark(row1));
  base::string16 term = UTF8ToUTF16("Search term 1");
  URLID url_id = history_db_.GetRowForURL(row1.url(), NULL);
  ASSERT_TRUE(url_id);
  ASSERT_TRUE(history_db_.SetKeywordSearchTermsForURL(url_id, 1, term));
  ASSERT_TRUE(backend->UpdateSearchTermTable());
  SearchTermRow keyword_cache;
  SearchTermID id = history_db_.GetSearchTerm(term, &keyword_cache);
  ASSERT_TRUE(id);
  EXPECT_EQ(term, keyword_cache.term);
  EXPECT_EQ(ToDatabaseTime(row1.last_visit_time()),
            ToDatabaseTime(keyword_cache.last_visit_time));

  // Add another row.
  HistoryAndBookmarkRow row2;
  row2.set_raw_url("google.com");
  row2.set_url(GURL("http://google.com"));
  row2.set_last_visit_time(Time::Now() - TimeDelta::FromDays(2));
  row2.set_title(UTF8ToUTF16("cnn"));
  ASSERT_TRUE(backend->InsertHistoryAndBookmark(row2));
  url_id = history_db_.GetRowForURL(row2.url(), NULL);
  ASSERT_TRUE(url_id);
  base::string16 term2 = UTF8ToUTF16("Search term 2");
  ASSERT_TRUE(history_db_.SetKeywordSearchTermsForURL(url_id, 1, term2));
  ASSERT_TRUE(backend->UpdateSearchTermTable());
  SearchTermID search_id1 = history_db_.GetSearchTerm(term,
                                                           &keyword_cache);
  // The id shouldn't changed.
  ASSERT_EQ(id, search_id1);
  EXPECT_EQ(term, keyword_cache.term);
  EXPECT_EQ(ToDatabaseTime(row1.last_visit_time()),
            ToDatabaseTime(keyword_cache.last_visit_time));
  // Verify the row just inserted.
  SearchTermID id2 = history_db_.GetSearchTerm(term2, &keyword_cache);
  ASSERT_TRUE(id2);
  EXPECT_EQ(term2, keyword_cache.term);
  EXPECT_EQ(ToDatabaseTime(row2.last_visit_time()),
            ToDatabaseTime(keyword_cache.last_visit_time));

  // Add 3rd row and associate it with term.
  HistoryAndBookmarkRow row3;
  row3.set_raw_url("search.com");
  row3.set_url(GURL("http://search.com"));
  row3.set_last_visit_time(Time::Now());
  row3.set_title(UTF8ToUTF16("search"));
  ASSERT_TRUE(backend->InsertHistoryAndBookmark(row3));
  url_id = history_db_.GetRowForURL(row3.url(), NULL);
  ASSERT_TRUE(url_id);
  ASSERT_TRUE(history_db_.SetKeywordSearchTermsForURL(url_id, 1, term));
  ASSERT_TRUE(backend->UpdateSearchTermTable());
  // Verify id not changed and last_visit_time updated.
  ASSERT_EQ(search_id1, history_db_.GetSearchTerm(term, &keyword_cache));
  EXPECT_EQ(ToDatabaseTime(row3.last_visit_time()),
            ToDatabaseTime(keyword_cache.last_visit_time));
  // The id of term2 wasn't changed.
  EXPECT_EQ(id2, history_db_.GetSearchTerm(term2, NULL));

  // Remove the term.
  ASSERT_TRUE(history_db_.DeleteKeywordSearchTerm(term));
  ASSERT_TRUE(backend->UpdateSearchTermTable());
  // The cache of term should removed.
  ASSERT_FALSE(history_db_.GetSearchTerm(term, NULL));
  // The id of term2 wasn't changed.
  EXPECT_EQ(id2, history_db_.GetSearchTerm(term2, NULL));
}

TEST_F(AndroidProviderBackendTest, QuerySearchTerms) {
  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  ASSERT_EQ(sql::INIT_OK, thumbnail_db_.Init(thumbnail_db_name_));
  std::unique_ptr<AndroidProviderBackend> backend(new AndroidProviderBackend(
      android_cache_db_name_, &history_db_, &thumbnail_db_,
      history_backend_client_.get(), &notifier_));
  // Insert a keyword search item to verify if we can find it.
  HistoryAndBookmarkRow row1;
  row1.set_raw_url("cnn.com");
  row1.set_url(GURL("http://cnn.com"));
  row1.set_last_visit_time(Time::Now() - TimeDelta::FromDays(1));
  row1.set_title(UTF8ToUTF16("cnn"));
  ASSERT_TRUE(backend->InsertHistoryAndBookmark(row1));
  base::string16 term = UTF8ToUTF16("Search term 1");
  URLID url_id = history_db_.GetRowForURL(row1.url(), NULL);
  ASSERT_TRUE(url_id);
  ASSERT_TRUE(history_db_.SetKeywordSearchTermsForURL(url_id, 1, term));

  std::vector<SearchRow::ColumnID> projections;
  projections.push_back(SearchRow::ID);
  projections.push_back(SearchRow::SEARCH_TERM);
  projections.push_back(SearchRow::SEARCH_TIME);
  std::unique_ptr<AndroidStatement> statement(
      backend->QuerySearchTerms(projections, std::string(),
                                std::vector<base::string16>(), std::string()));
  ASSERT_TRUE(statement.get());
  ASSERT_TRUE(statement->statement()->Step());
  EXPECT_TRUE(statement->statement()->ColumnInt64(0));
  EXPECT_EQ(term, statement->statement()->ColumnString16(1));
  EXPECT_EQ(ToDatabaseTime(row1.last_visit_time()),
            statement->statement()->ColumnInt64(2));
  EXPECT_FALSE(statement->statement()->Step());
}

TEST_F(AndroidProviderBackendTest, UpdateSearchTerms) {
  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  ASSERT_EQ(sql::INIT_OK, thumbnail_db_.Init(thumbnail_db_name_));
  std::unique_ptr<AndroidProviderBackend> backend(new AndroidProviderBackend(
      android_cache_db_name_, &history_db_, &thumbnail_db_,
      history_backend_client_.get(), &notifier_));
  // Insert a keyword.
  HistoryAndBookmarkRow row1;
  row1.set_raw_url("cnn.com");
  row1.set_url(GURL("http://cnn.com"));
  row1.set_last_visit_time(Time::Now() - TimeDelta::FromDays(1));
  row1.set_title(UTF8ToUTF16("cnn"));
  ASSERT_TRUE(backend->InsertHistoryAndBookmark(row1));
  base::string16 term = UTF8ToUTF16("Search term 1");
  URLID url_id = history_db_.GetRowForURL(row1.url(), NULL);
  ASSERT_TRUE(url_id);
  ASSERT_TRUE(history_db_.SetKeywordSearchTermsForURL(url_id, 1, term));

  // Get the SearchTermID of the row we just inserted.
  std::vector<SearchRow::ColumnID> projections;
  projections.push_back(SearchRow::ID);
  projections.push_back(SearchRow::SEARCH_TIME);
  projections.push_back(SearchRow::SEARCH_TERM);
  std::vector<base::string16> args;
  args.push_back(term);
  std::unique_ptr<AndroidStatement> statement(backend->QuerySearchTerms(
      projections, "search = ?", args, std::string()));
  ASSERT_TRUE(statement.get());
  ASSERT_TRUE(statement->statement()->Step());
  SearchTermID id = statement->statement()->ColumnInt64(0);
  ASSERT_TRUE(id);
  EXPECT_FALSE(statement->statement()->Step());

  // Update the search term and time.
  base::string16 update_term = UTF8ToUTF16("Update search term");
  args.clear();
  args.push_back(term);
  SearchRow search_row;
  search_row.set_search_term(update_term);
  search_row.set_url(GURL("http://google.com"));
  search_row.set_keyword_id(1);
  search_row.set_search_time(Time::Now() - TimeDelta::FromHours(1));
  int update_count = 0;
  ASSERT_TRUE(backend->UpdateSearchTerms(search_row, "search = ?", args,
                                         &update_count));
  EXPECT_EQ(1, update_count);

  // Verify if the search term updated.
  // The origin term should be removed.
  std::vector<KeywordSearchTermRow> rows;
  ASSERT_TRUE(history_db_.GetKeywordSearchTermRows(term, &rows));
  EXPECT_TRUE(rows.empty());
  // The new term should be inserted.
  ASSERT_TRUE(history_db_.GetKeywordSearchTermRows(update_term, &rows));
  ASSERT_EQ(1u, rows.size());
  // The history of urls shouldn't be removed.
  ASSERT_TRUE(history_db_.GetRowForURL(row1.url(), NULL));
  // The new URL is inserted.
  ASSERT_TRUE(history_db_.GetRowForURL(search_row.url(), NULL));

  // Verfiy the AndoridSearchID isn't changed.
  args.clear();
  args.push_back(update_term);
  statement.reset(backend->QuerySearchTerms(projections, "search = ?", args,
                                            std::string()));
  ASSERT_TRUE(statement.get());
  ASSERT_TRUE(statement->statement()->Step());
  // The id didn't change.
  EXPECT_EQ(id, statement->statement()->ColumnInt64(0));
  // The search time was updated.
  EXPECT_EQ(ToDatabaseTime(search_row.search_time()),
            statement->statement()->ColumnInt64(1));
  // The search term was updated.
  EXPECT_EQ(update_term, statement->statement()->ColumnString16(2));
  EXPECT_FALSE(statement->statement()->Step());

  // Only update the search time.
  SearchRow update_time;
  update_time.set_search_time(Time::Now());
  // Update it by id.
  args.clear();
  std::ostringstream oss;
  oss << id;
  args.push_back(UTF8ToUTF16(oss.str()));
  update_count = 0;
  ASSERT_TRUE(backend->UpdateSearchTerms(update_time, "_id = ?", args,
                                         &update_count));
  EXPECT_EQ(1, update_count);

  // Verify the update.
  statement.reset(backend->QuerySearchTerms(projections, "_id = ?", args,
                                            std::string()));
  ASSERT_TRUE(statement.get());
  ASSERT_TRUE(statement->statement()->Step());
  // The id didn't change.
  EXPECT_EQ(id, statement->statement()->ColumnInt64(0));
  // The search time was updated.
  EXPECT_EQ(ToDatabaseTime(update_time.search_time()),
            statement->statement()->ColumnInt64(1));
  // The search term didn't change.
  EXPECT_EQ(update_term, statement->statement()->ColumnString16(2));
  EXPECT_FALSE(statement->statement()->Step());
}

TEST_F(AndroidProviderBackendTest, DeleteSearchTerms) {
  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  ASSERT_EQ(sql::INIT_OK, thumbnail_db_.Init(thumbnail_db_name_));
  std::unique_ptr<AndroidProviderBackend> backend(new AndroidProviderBackend(
      android_cache_db_name_, &history_db_, &thumbnail_db_,
      history_backend_client_.get(), &notifier_));
  // Insert a keyword.
  HistoryAndBookmarkRow row1;
  row1.set_raw_url("cnn.com");
  row1.set_url(GURL("http://cnn.com"));
  row1.set_last_visit_time(Time::Now() - TimeDelta::FromDays(1));
  row1.set_title(UTF8ToUTF16("cnn"));
  ASSERT_TRUE(backend->InsertHistoryAndBookmark(row1));
  base::string16 term = UTF8ToUTF16("Search term 1");
  URLID url_id = history_db_.GetRowForURL(row1.url(), NULL);
  ASSERT_TRUE(url_id);
  ASSERT_TRUE(history_db_.SetKeywordSearchTermsForURL(url_id, 1, term));

  // Get the SearchTermID of the row we just inserted.
  std::vector<SearchRow::ColumnID> projections;
  projections.push_back(SearchRow::ID);
  projections.push_back(SearchRow::SEARCH_TIME);
  projections.push_back(SearchRow::SEARCH_TERM);
  std::vector<base::string16> args;
  args.push_back(term);
  std::unique_ptr<AndroidStatement> statement(backend->QuerySearchTerms(
      projections, "search = ?", args, std::string()));
  ASSERT_TRUE(statement.get());
  ASSERT_TRUE(statement->statement()->Step());
  SearchTermID id1 = statement->statement()->ColumnInt64(0);
  ASSERT_TRUE(id1);
  EXPECT_FALSE(statement->statement()->Step());

  // Insert a keyword.
  HistoryAndBookmarkRow row2;
  row2.set_raw_url("google.com");
  row2.set_url(GURL("http://google.com"));
  row2.set_last_visit_time(Time::Now() - TimeDelta::FromDays(1));
  row2.set_title(UTF8ToUTF16("google"));
  ASSERT_TRUE(backend->InsertHistoryAndBookmark(row2));
  base::string16 term2 = UTF8ToUTF16("Search term 2");
  URLID url_id2 = history_db_.GetRowForURL(row2.url(), NULL);
  ASSERT_TRUE(url_id2);
  ASSERT_TRUE(history_db_.SetKeywordSearchTermsForURL(url_id2, 1, term2));

  // Get the SearchTermID of the row we just inserted.
  projections.clear();
  projections.push_back(SearchRow::ID);
  projections.push_back(SearchRow::SEARCH_TIME);
  projections.push_back(SearchRow::SEARCH_TERM);
  args.clear();
  args.push_back(term2);
  statement.reset(backend->QuerySearchTerms(projections, "search = ?", args,
                                            std::string()));
  ASSERT_TRUE(statement.get());
  ASSERT_TRUE(statement->statement()->Step());
  SearchTermID id2 = statement->statement()->ColumnInt64(0);
  ASSERT_TRUE(id2);
  EXPECT_FALSE(statement->statement()->Step());

  // Delete the first one.
  args.clear();
  args.push_back(term);
  int deleted_count = 0;
  ASSERT_TRUE(backend->DeleteSearchTerms("search = ?", args, &deleted_count));
  EXPECT_EQ(1, deleted_count);
  std::vector<KeywordSearchTermRow> rows;
  ASSERT_TRUE(history_db_.GetKeywordSearchTermRows(term, &rows));
  EXPECT_TRUE(rows.empty());
  // Verify we can't get the first term.
  args.clear();
  std::ostringstream oss;
  oss << id1;
  args.push_back(UTF8ToUTF16(oss.str()));
  statement.reset(backend->QuerySearchTerms(projections, "_id = ?", args,
                                            std::string()));
  ASSERT_TRUE(statement.get());
  EXPECT_FALSE(statement->statement()->Step());

  // The second one is still there.
  args.clear();
  std::ostringstream oss1;
  oss1 << id2;
  args.push_back(UTF8ToUTF16(oss1.str()));
  statement.reset(backend->QuerySearchTerms(projections, "_id = ?", args,
                                            std::string()));
  ASSERT_TRUE(statement.get());
  EXPECT_TRUE(statement->statement()->Step());
  EXPECT_EQ(id2, statement->statement()->ColumnInt64(0));
  EXPECT_FALSE(statement->statement()->Step());

  // Remove all search terms in no condition.
  deleted_count = 0;
  args.clear();
  ASSERT_TRUE(backend->DeleteSearchTerms(std::string(), args, &deleted_count));
  EXPECT_EQ(1, deleted_count);

  // Verify the second one was removed.
  args.clear();
  args.push_back(UTF8ToUTF16(oss1.str()));
  statement.reset(backend->QuerySearchTerms(projections, "_id = ?", args,
                                            std::string()));
  ASSERT_TRUE(statement.get());
  EXPECT_FALSE(statement->statement()->Step());
}

TEST_F(AndroidProviderBackendTest, InsertSearchTerm) {
  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  ASSERT_EQ(sql::INIT_OK, thumbnail_db_.Init(thumbnail_db_name_));
  std::unique_ptr<AndroidProviderBackend> backend(new AndroidProviderBackend(
      android_cache_db_name_, &history_db_, &thumbnail_db_,
      history_backend_client_.get(), &notifier_));
  SearchRow search_row;
  search_row.set_search_term(UTF8ToUTF16("google"));
  search_row.set_url(GURL("http://google.com"));
  search_row.set_keyword_id(1);
  search_row.set_search_time(Time::Now() - TimeDelta::FromHours(1));

  SearchTermID id = backend->InsertSearchTerm(search_row);
  ASSERT_TRUE(id);

  std::vector<SearchRow::ColumnID> projections;
  projections.push_back(SearchRow::ID);
  projections.push_back(SearchRow::SEARCH_TIME);
  projections.push_back(SearchRow::SEARCH_TERM);
  std::vector<base::string16> args;
  std::ostringstream oss;
  oss << id;
  args.push_back(UTF8ToUTF16(oss.str()));
  std::unique_ptr<AndroidStatement> statement(
      backend->QuerySearchTerms(projections, "_id = ?", args, std::string()));
  ASSERT_TRUE(statement.get());
  ASSERT_TRUE(statement->statement()->Step());
  EXPECT_EQ(id, statement->statement()->ColumnInt64(0));
  EXPECT_EQ(ToDatabaseTime(search_row.search_time()),
            statement->statement()->ColumnInt64(1));
  EXPECT_EQ(search_row.search_term(),
            statement->statement()->ColumnString16(2));
  EXPECT_FALSE(statement->statement()->Step());
}

TEST_F(AndroidProviderBackendTest, DeleteHistory) {
  HistoryAndBookmarkRow row1;
  row1.set_raw_url("cnn.com");
  row1.set_url(GURL("http://cnn.com"));
  row1.set_last_visit_time(Time::Now() - TimeDelta::FromDays(1));
  row1.set_created(Time::Now() - TimeDelta::FromDays(20));
  row1.set_visit_count(10);
  row1.set_is_bookmark(true);
  row1.set_title(UTF8ToUTF16("cnn"));

  HistoryAndBookmarkRow row2;
  row2.set_raw_url("http://www.example.com");
  row2.set_url(GURL("http://www.example.com"));
  row2.set_last_visit_time(Time::Now() - TimeDelta::FromDays(10));
  row2.set_is_bookmark(false);
  row2.set_title(UTF8ToUTF16("example"));
  std::vector<unsigned char> data;
  data.push_back('1');
  row2.set_favicon(base::RefCountedBytes::TakeVector(&data));

  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  ASSERT_EQ(sql::INIT_OK, thumbnail_db_.Init(thumbnail_db_name_));
  std::unique_ptr<AndroidProviderBackend> backend(new AndroidProviderBackend(
      android_cache_db_name_, &history_db_, &thumbnail_db_,
      history_backend_client_.get(), &notifier_));

  AndroidURLID id1 = backend->InsertHistoryAndBookmark(row1);
  ASSERT_TRUE(id1);
  AndroidURLID id2 = backend->InsertHistoryAndBookmark(row2);
  ASSERT_TRUE(id2);

  // Verify the row1 has been added in bookmark model.
  content::RunAllPendingInMessageLoop();
  ASSERT_EQ(1u, bookmark_model_->mobile_node()->children().size());
  const BookmarkNode* child =
      bookmark_model_->mobile_node()->children().front().get();
  ASSERT_TRUE(child);
  EXPECT_EQ(row1.title(), child->GetTitle());
  EXPECT_EQ(row1.url(), child->url());

  // Delete history
  int deleted_count = 0;
  ASSERT_TRUE(backend->DeleteHistory(std::string(),
                                     std::vector<base::string16>(),
                                     &deleted_count));
  EXPECT_EQ(2, deleted_count);
  // The row2 was deleted.
  EXPECT_FALSE(history_db_.GetRowForURL(row2.url(), NULL));
  // Still find the row1.
  URLRow url_row;
  ASSERT_TRUE(history_db_.GetRowForURL(row1.url(), &url_row));
  // The visit_count was reset.
  EXPECT_EQ(0, url_row.visit_count());
  EXPECT_EQ(Time::UnixEpoch(), url_row.last_visit());

  // Verify the row1 is still in bookmark model.
  content::RunAllPendingInMessageLoop();
  ASSERT_EQ(1u, bookmark_model_->mobile_node()->children().size());
  const BookmarkNode* child1 =
      bookmark_model_->mobile_node()->children().front().get();
  ASSERT_TRUE(child1);
  EXPECT_EQ(row1.title(), child1->GetTitle());
  EXPECT_EQ(row1.url(), child1->url());

  // Verify notification
  ASSERT_TRUE(notifier_.deleted_details());
  ASSERT_EQ(2u, notifier_.deleted_details()->size());
  EXPECT_EQ(row1.url(),
            (*notifier_.modified_details())[0].url());
  EXPECT_EQ(Time::UnixEpoch(),
            (*notifier_.modified_details())[0].last_visit());
  EXPECT_EQ(1u, notifier_.favicon_changed()->size());
}

TEST_F(AndroidProviderBackendTest, TestMultipleNestingTransaction) {
  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  ASSERT_EQ(sql::INIT_OK, thumbnail_db_.Init(thumbnail_db_name_));
  std::unique_ptr<AndroidProviderBackend> backend(new AndroidProviderBackend(
      android_cache_db_name_, &history_db_, &thumbnail_db_,
      history_backend_client_.get(), &notifier_));

  // Create the nested transactions.
  history_db_.BeginTransaction();
  history_db_.BeginTransaction();
  history_db_.BeginTransaction();
  thumbnail_db_.BeginTransaction();
  thumbnail_db_.BeginTransaction();
  int history_transaction = history_db_.transaction_nesting();
  int thumbnail_transaction = thumbnail_db_.transaction_nesting();

  // Insert a row to verify the transaction number are not changed
  // after a transaction commit.
  HistoryAndBookmarkRow row1;
  row1.set_raw_url("cnn.com");
  row1.set_url(GURL("http://cnn.com"));
  row1.set_last_visit_time(Time::Now() - TimeDelta::FromDays(1));
  row1.set_created(Time::Now() - TimeDelta::FromDays(20));
  row1.set_visit_count(10);
  row1.set_title(UTF8ToUTF16("cnn"));
  ASSERT_TRUE(backend->InsertHistoryAndBookmark(row1));
  EXPECT_EQ(history_transaction, history_db_.transaction_nesting());
  EXPECT_EQ(thumbnail_transaction, thumbnail_db_.transaction_nesting());

  // Insert the same URL, it should failed. The transaction are still same
  // after a rollback.
  ASSERT_FALSE(backend->InsertHistoryAndBookmark(row1));
  EXPECT_EQ(history_transaction, history_db_.transaction_nesting());
  EXPECT_EQ(thumbnail_transaction, thumbnail_db_.transaction_nesting());

  // Insert another row to verify we are still fine after the previous
  // rollback.
  HistoryAndBookmarkRow row2;
  row2.set_raw_url("http://www.example.com");
  row2.set_url(GURL("http://www.example.com"));
  row2.set_last_visit_time(Time::Now() - TimeDelta::FromDays(10));
  row2.set_is_bookmark(false);
  row2.set_title(UTF8ToUTF16("example"));
  ASSERT_TRUE(backend->InsertHistoryAndBookmark(row2));
  EXPECT_EQ(history_transaction, history_db_.transaction_nesting());
  EXPECT_EQ(thumbnail_transaction, thumbnail_db_.transaction_nesting());
}

TEST_F(AndroidProviderBackendTest, TestAndroidCTSComplianceForZeroVisitCount) {
  // This is to verify the last visit time and created time are same when visit
  // count is 0.
  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  ASSERT_EQ(sql::INIT_OK, thumbnail_db_.Init(thumbnail_db_name_));
  std::unique_ptr<AndroidProviderBackend> backend(new AndroidProviderBackend(
      android_cache_db_name_, &history_db_, &thumbnail_db_,
      history_backend_client_.get(), &notifier_));
  URLRow url_row(GURL("http://www.google.com"));
  url_row.set_last_visit(Time::Now());
  url_row.set_visit_count(0);
  history_db_.AddURL(url_row);

  std::vector<HistoryAndBookmarkRow::ColumnID> projections;

  projections.push_back(HistoryAndBookmarkRow::ID);
  projections.push_back(HistoryAndBookmarkRow::URL);
  projections.push_back(HistoryAndBookmarkRow::TITLE);
  projections.push_back(HistoryAndBookmarkRow::CREATED);
  projections.push_back(HistoryAndBookmarkRow::LAST_VISIT_TIME);
  projections.push_back(HistoryAndBookmarkRow::VISIT_COUNT);
  projections.push_back(HistoryAndBookmarkRow::FAVICON);
  projections.push_back(HistoryAndBookmarkRow::BOOKMARK);

  std::unique_ptr<AndroidStatement> statement(backend->QueryHistoryAndBookmarks(
      projections, std::string(), std::vector<base::string16>(),
      std::string("url ASC")));

  ASSERT_TRUE(statement->statement()->Step());
  EXPECT_EQ(ToDatabaseTime(url_row.last_visit()),
            statement->statement()->ColumnInt64(3));
  EXPECT_EQ(ToDatabaseTime(url_row.last_visit()),
            statement->statement()->ColumnInt64(4));
  EXPECT_EQ(url_row.visit_count(), statement->statement()->ColumnInt(5));
}

TEST_F(AndroidProviderBackendTest, AndroidCTSComplianceFolderColumnExists) {
  // This is test is used to verify the 'folder' column exists, all bookmarks
  // returned when folder is 0 and the non bookmark rows returned when folder
  // is 1.
  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  ASSERT_EQ(sql::INIT_OK, thumbnail_db_.Init(thumbnail_db_name_));
  std::unique_ptr<AndroidProviderBackend> backend(new AndroidProviderBackend(
      android_cache_db_name_, &history_db_, &thumbnail_db_,
      history_backend_client_.get(), &notifier_));
  HistoryAndBookmarkRow row1;
  row1.set_raw_url("cnn.com");
  row1.set_url(GURL("http://cnn.com"));
  row1.set_last_visit_time(Time::Now() - TimeDelta::FromDays(1));
  row1.set_created(Time::Now() - TimeDelta::FromDays(20));
  row1.set_visit_count(10);
  row1.set_is_bookmark(true);
  row1.set_title(UTF8ToUTF16("cnn"));

  HistoryAndBookmarkRow row2;
  row2.set_raw_url("http://www.example.com");
  row2.set_url(GURL("http://www.example.com"));
  row2.set_last_visit_time(Time::Now() - TimeDelta::FromDays(10));
  row2.set_is_bookmark(false);
  row2.set_title(UTF8ToUTF16("example"));
  std::vector<unsigned char> data;
  data.push_back('1');
  row2.set_favicon(base::RefCountedBytes::TakeVector(&data));

  AndroidURLID id1 = backend->InsertHistoryAndBookmark(row1);
  ASSERT_TRUE(id1);
  AndroidURLID id2 = backend->InsertHistoryAndBookmark(row2);
  ASSERT_TRUE(id2);
  content::RunAllPendingInMessageLoop();

  // Query by folder=0, the row1 should returned.
  std::vector<HistoryAndBookmarkRow::ColumnID> projections;

  projections.push_back(HistoryAndBookmarkRow::URL);

  std::unique_ptr<AndroidStatement> statement(backend->QueryHistoryAndBookmarks(
      projections, std::string("folder=0"), std::vector<base::string16>(),
      std::string("url ASC")));
  ASSERT_TRUE(statement->statement()->Step());
  EXPECT_EQ(row1.raw_url(), statement->statement()->ColumnString(0));
  EXPECT_FALSE(statement->statement()->Step());

  // Query by folder=1, the row2 should returned.
  statement.reset(backend->QueryHistoryAndBookmarks(
      projections, std::string("folder=1"), std::vector<base::string16>(),
      std::string("url ASC")));
  ASSERT_TRUE(statement->statement()->Step());
  EXPECT_EQ(row2.url(), GURL(statement->statement()->ColumnString(0)));
  EXPECT_FALSE(statement->statement()->Step());
}

TEST_F(AndroidProviderBackendTest, QueryWithoutThumbnailDB) {
  GURL url1("http://www.cnn.com");
  const base::string16 title1(UTF8ToUTF16("cnn"));
  std::vector<VisitInfo> visits1;
  Time last_visited1 = Time::Now() - TimeDelta::FromDays(1);
  Time created1 = last_visited1 - TimeDelta::FromDays(20);
  visits1.push_back(VisitInfo(created1, ui::PAGE_TRANSITION_LINK));
  visits1.push_back(VisitInfo(last_visited1 - TimeDelta::FromDays(1),
                              ui::PAGE_TRANSITION_LINK));
  visits1.push_back(VisitInfo(last_visited1, ui::PAGE_TRANSITION_LINK));

  GURL url2("http://www.example.com");
  std::vector<VisitInfo> visits2;
  const base::string16 title2(UTF8ToUTF16("example"));
  Time last_visited2 = Time::Now();
  Time created2 = last_visited2 - TimeDelta::FromDays(10);
  visits2.push_back(VisitInfo(created2, ui::PAGE_TRANSITION_LINK));
  visits2.push_back(VisitInfo(last_visited2 - TimeDelta::FromDays(5),
                              ui::PAGE_TRANSITION_LINK));
  visits2.push_back(VisitInfo(last_visited2, ui::PAGE_TRANSITION_LINK));

  // Only use the HistoryBackend to generate the test data.
  // HistoryBackend will shutdown after that.
  {
  scoped_refptr<HistoryBackend> history_backend;
  history_backend = base::MakeRefCounted<HistoryBackend>(
      std::make_unique<AndroidProviderBackendDelegate>(),
      history_client_->CreateBackendClient(),
      base::ThreadTaskRunnerHandle::Get());
  history_backend->Init(false,
                        TestHistoryDatabaseParamsForPath(temp_dir_.GetPath()));
  history_backend->AddVisits(url1, visits1, history::SOURCE_SYNCED);
  history_backend->AddVisits(url2, visits2, history::SOURCE_SYNCED);
  URLRow url_row;

  history::URLRows url_rows(2u);
  ASSERT_TRUE(history_backend->GetURL(url1, &url_rows[0]));
  ASSERT_TRUE(history_backend->GetURL(url2, &url_rows[1]));
  url_rows[0].set_title(title1);
  url_rows[1].set_title(title2);
  ASSERT_EQ(2u, history_backend->UpdateURLs(url_rows));

  // Set favicon to url2.
  std::vector<SkBitmap> bitmaps(1u, CreateBitmap());
  history_backend->SetFavicons({url2}, favicon_base::IconType::kFavicon, GURL(),
                               bitmaps);
  history_backend->Closing();
  }

  // The history_db_name and thumbnail_db_name files should be created by
  // HistoryBackend. We need to open the same database files.
  ASSERT_TRUE(base::PathExists(history_db_name_));
  ASSERT_TRUE(base::PathExists(thumbnail_db_name_));

  // Only creates the history database
  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));

  // Set url1 as bookmark.
  AddBookmark(url1);

  std::unique_ptr<AndroidProviderBackend> backend(
      new AndroidProviderBackend(android_cache_db_name_, &history_db_, NULL,
                                 history_backend_client_.get(), &notifier_));

  std::vector<HistoryAndBookmarkRow::ColumnID> projections;

  projections.push_back(HistoryAndBookmarkRow::ID);
  projections.push_back(HistoryAndBookmarkRow::URL);
  projections.push_back(HistoryAndBookmarkRow::TITLE);
  projections.push_back(HistoryAndBookmarkRow::CREATED);
  projections.push_back(HistoryAndBookmarkRow::LAST_VISIT_TIME);
  projections.push_back(HistoryAndBookmarkRow::VISIT_COUNT);
  projections.push_back(HistoryAndBookmarkRow::FAVICON);
  projections.push_back(HistoryAndBookmarkRow::BOOKMARK);

  std::unique_ptr<AndroidStatement> statement(backend->QueryHistoryAndBookmarks(
      projections, std::string(), std::vector<base::string16>(),
      std::string("url ASC")));
  ASSERT_TRUE(statement->statement()->Step());
  ASSERT_EQ(url1, GURL(statement->statement()->ColumnString(1)));
  EXPECT_EQ(title1, statement->statement()->ColumnString16(2));
  EXPECT_EQ(ToDatabaseTime(created1),
            statement->statement()->ColumnInt64(3));
  EXPECT_EQ(ToDatabaseTime(last_visited1),
            statement->statement()->ColumnInt64(4));
  EXPECT_EQ(3, statement->statement()->ColumnInt(5));
  EXPECT_EQ(6, statement->favicon_index());
  // No favicon.
  EXPECT_EQ(0, statement->statement()->ColumnByteLength(6));
  EXPECT_TRUE(statement->statement()->ColumnBool(7));

  ASSERT_TRUE(statement->statement()->Step());
  EXPECT_EQ(title2, statement->statement()->ColumnString16(2));
  ASSERT_EQ(url2, GURL(statement->statement()->ColumnString(1)));
  EXPECT_EQ(ToDatabaseTime(created2),
            statement->statement()->ColumnInt64(3));
  EXPECT_EQ(ToDatabaseTime(last_visited2),
            statement->statement()->ColumnInt64(4));
  EXPECT_EQ(3, statement->statement()->ColumnInt(5));
  std::vector<unsigned char> favicon2;
  EXPECT_EQ(6, statement->favicon_index());
  // No favicon because thumbnail database wasn't initialized.
  EXPECT_EQ(0, statement->statement()->ColumnByteLength(6));
  EXPECT_FALSE(statement->statement()->ColumnBool(7));

  // No more row.
  EXPECT_FALSE(statement->statement()->Step());
}

TEST_F(AndroidProviderBackendTest, InsertWithoutThumbnailDB) {
  HistoryAndBookmarkRow row1;
  row1.set_raw_url("cnn.com");
  row1.set_url(GURL("http://cnn.com"));
  row1.set_last_visit_time(Time::Now() - TimeDelta::FromDays(1));
  row1.set_created(Time::Now() - TimeDelta::FromDays(20));
  row1.set_visit_count(10);
  row1.set_is_bookmark(true);
  row1.set_title(UTF8ToUTF16("cnn"));

  HistoryAndBookmarkRow row2;
  row2.set_raw_url("http://www.example.com");
  row2.set_url(GURL("http://www.example.com"));
  row2.set_last_visit_time(Time::Now() - TimeDelta::FromDays(10));
  row2.set_is_bookmark(false);
  row2.set_title(UTF8ToUTF16("example"));
  std::vector<unsigned char> data;
  data.push_back('1');
  row2.set_favicon(base::RefCountedBytes::TakeVector(&data));

  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  std::unique_ptr<AndroidProviderBackend> backend(
      new AndroidProviderBackend(android_cache_db_name_, &history_db_, NULL,
                                 history_backend_client_.get(), &notifier_));

  ASSERT_TRUE(backend->InsertHistoryAndBookmark(row1));
  EXPECT_FALSE(notifier_.deleted_details());
  ASSERT_TRUE(notifier_.modified_details());
  ASSERT_EQ(1u, notifier_.modified_details()->size());
  EXPECT_EQ(row1.url(), (*notifier_.modified_details())[0].url());
  EXPECT_EQ(row1.last_visit_time(),
            (*notifier_.modified_details())[0].last_visit());
  EXPECT_EQ(row1.visit_count(),
            (*notifier_.modified_details())[0].visit_count());
  EXPECT_EQ(row1.title(),
            (*notifier_.modified_details())[0].title());
  EXPECT_FALSE(notifier_.favicon_changed());
  content::RunAllPendingInMessageLoop();
  ASSERT_EQ(1u, bookmark_model_->mobile_node()->children().size());
  const BookmarkNode* child =
      bookmark_model_->mobile_node()->children().front().get();
  ASSERT_TRUE(child);
  EXPECT_EQ(row1.title(), child->GetTitle());
  EXPECT_EQ(row1.url(), child->url());

  notifier_.ResetDetails();
  ASSERT_TRUE(backend->InsertHistoryAndBookmark(row2));
  EXPECT_FALSE(notifier_.deleted_details());
  ASSERT_TRUE(notifier_.modified_details());
  ASSERT_EQ(1u, notifier_.modified_details()->size());
  EXPECT_EQ(row2.url(), (*notifier_.modified_details())[0].url());
  EXPECT_EQ(row2.last_visit_time(),
            (*notifier_.modified_details())[0].last_visit());
  EXPECT_EQ(row2.title(),
            (*notifier_.modified_details())[0].title());
  // Favicon details is still false because thumbnail database wasn't
  // initialized, we ignore any changes of favicon.
  ASSERT_FALSE(notifier_.favicon_changed());
}

TEST_F(AndroidProviderBackendTest, DeleteWithoutThumbnailDB) {
  HistoryAndBookmarkRow row1;
  row1.set_raw_url("cnn.com");
  row1.set_url(GURL("http://cnn.com"));
  row1.set_last_visit_time(Time::Now() - TimeDelta::FromDays(1));
  row1.set_created(Time::Now() - TimeDelta::FromDays(20));
  row1.set_visit_count(10);
  row1.set_is_bookmark(true);
  row1.set_title(UTF8ToUTF16("cnn"));

  HistoryAndBookmarkRow row2;
  row2.set_raw_url("http://www.example.com");
  row2.set_url(GURL("http://www.example.com"));
  row2.set_last_visit_time(Time::Now() - TimeDelta::FromDays(10));
  row2.set_is_bookmark(false);
  row2.set_title(UTF8ToUTF16("example"));
  std::vector<unsigned char> data;
  data.push_back('1');
  row2.set_favicon(base::RefCountedBytes::TakeVector(&data));

  {
    TestHistoryDatabase history_db;
    ThumbnailDatabase thumbnail_db(NULL);
    ASSERT_EQ(sql::INIT_OK, history_db.Init(history_db_name_));
    ASSERT_EQ(sql::INIT_OK, thumbnail_db.Init(thumbnail_db_name_));

    std::unique_ptr<AndroidProviderBackend> backend(new AndroidProviderBackend(
        android_cache_db_name_, &history_db, &thumbnail_db,
        history_backend_client_.get(), &notifier_));

    ASSERT_TRUE(backend->InsertHistoryAndBookmark(row1));
    ASSERT_TRUE(backend->InsertHistoryAndBookmark(row2));
    // Verify the row1 has been added in bookmark model.
    content::RunAllPendingInMessageLoop();
    ASSERT_EQ(1u, bookmark_model_->mobile_node()->children().size());
    const BookmarkNode* child =
        bookmark_model_->mobile_node()->children().front().get();
    ASSERT_TRUE(child);
    EXPECT_EQ(row1.title(), child->GetTitle());
    EXPECT_EQ(row1.url(), child->url());
  }
  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  std::unique_ptr<AndroidProviderBackend> backend(
      new AndroidProviderBackend(android_cache_db_name_, &history_db_, NULL,
                                 history_backend_client_.get(), &notifier_));

  // Delete all rows.
  std::vector<base::string16> args;
  int deleted_count = 0;
  notifier_.ResetDetails();
  ASSERT_TRUE(backend->DeleteHistoryAndBookmarks("Favicon IS NULL", args,
                                                 &deleted_count));
  // All rows were deleted.
  EXPECT_EQ(2, deleted_count);
  // Verify the rows was removed from bookmark model.
  content::RunAllPendingInMessageLoop();
  ASSERT_EQ(0u, bookmark_model_->mobile_node()->children().size());

  // Verify notifications
  ASSERT_TRUE(notifier_.deleted_details());
  EXPECT_FALSE(notifier_.modified_details());
  EXPECT_EQ(2u, notifier_.deleted_details()->size());
  // No favicon has been deleted.
  EXPECT_FALSE(notifier_.favicon_changed());

  // No row exists.
  std::vector<HistoryAndBookmarkRow::ColumnID> projections;
  projections.push_back(HistoryAndBookmarkRow::ID);
  projections.push_back(HistoryAndBookmarkRow::URL);
  projections.push_back(HistoryAndBookmarkRow::TITLE);
  projections.push_back(HistoryAndBookmarkRow::CREATED);
  projections.push_back(HistoryAndBookmarkRow::LAST_VISIT_TIME);
  projections.push_back(HistoryAndBookmarkRow::VISIT_COUNT);
  projections.push_back(HistoryAndBookmarkRow::FAVICON);
  projections.push_back(HistoryAndBookmarkRow::BOOKMARK);

  std::unique_ptr<AndroidStatement> statement1(
      backend->QueryHistoryAndBookmarks(projections, std::string(),
                                        std::vector<base::string16>(),
                                        std::string("url ASC")));
  ASSERT_FALSE(statement1->statement()->Step());
}

TEST_F(AndroidProviderBackendTest, UpdateFaviconWithoutThumbnail) {
  HistoryAndBookmarkRow row1;
  row1.set_raw_url("cnn.com");
  row1.set_url(GURL("http://cnn.com"));
  row1.set_last_visit_time(Time::Now() - TimeDelta::FromDays(1));
  row1.set_created(Time::Now() - TimeDelta::FromDays(20));
  row1.set_visit_count(10);
  row1.set_is_bookmark(true);
  row1.set_title(UTF8ToUTF16("cnn"));

  {
    TestHistoryDatabase history_db;
    ThumbnailDatabase thumbnail_db(NULL);
    ASSERT_EQ(sql::INIT_OK, history_db.Init(history_db_name_));
    ASSERT_EQ(sql::INIT_OK, thumbnail_db.Init(thumbnail_db_name_));
    std::unique_ptr<AndroidProviderBackend> backend(new AndroidProviderBackend(
        android_cache_db_name_, &history_db, &thumbnail_db,
        history_backend_client_.get(), &notifier_));

    AndroidURLID id1 = backend->InsertHistoryAndBookmark(row1);
    ASSERT_TRUE(id1);
  }

  ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name_));
  std::unique_ptr<AndroidProviderBackend> backend(
      new AndroidProviderBackend(android_cache_db_name_, &history_db_, NULL,
                                 history_backend_client_.get(), &notifier_));

  int update_count;
  std::vector<base::string16> update_args;
  // Update the last visit time to a value greater than current one.
  HistoryAndBookmarkRow update_row1;

  // Set visit count.
  update_row1.set_visit_count(5);
  // Set favicon.
  std::vector<unsigned char> data;
  data.push_back('1');
  update_row1.set_favicon(base::RefCountedBytes::TakeVector(&data));
  update_args.push_back(UTF8ToUTF16(row1.raw_url()));
  notifier_.ResetDetails();
  ASSERT_TRUE(backend->UpdateHistoryAndBookmarks(update_row1, "url = ?",
                                                 update_args, &update_count));
  // Verify notifications.
  EXPECT_FALSE(notifier_.deleted_details());
  ASSERT_TRUE(notifier_.modified_details());
  ASSERT_EQ(1u, notifier_.modified_details()->size());
  // No favicon will be updated as thumbnail database is missing.
  EXPECT_FALSE(notifier_.favicon_changed());
}

}  // namespace history
