// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/profile_writer.h"

#include <stddef.h>

#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/query_parser/query_parser.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using bookmarks::BookmarkModel;
using bookmarks::TitledUrlMatch;
using bookmarks::UrlAndTitle;
using password_manager::PasswordForm;
using password_manager::TestPasswordStore;

PasswordForm MakePasswordForm() {
  PasswordForm form;
  form.url = GURL("https://example.com/");
  form.signon_realm = form.url.DeprecatedGetOriginAsURL().spec();
  form.username_value = u"user@gmail.com";
  form.password_value = u"s3cre3t";
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

}  // namespace

class TestProfileWriter : public ProfileWriter {
 public:
  explicit TestProfileWriter(Profile* profile) : ProfileWriter(profile) {}
 protected:
  ~TestProfileWriter() override {}
};

class ProfileWriterTest : public testing::Test {
 public:
  ProfileWriterTest() {}

  ProfileWriterTest(const ProfileWriterTest&) = delete;
  ProfileWriterTest& operator=(const ProfileWriterTest&) = delete;

  ~ProfileWriterTest() override {}

  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();

    TestingProfile::Builder second_profile_builder;
    second_profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    second_profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    second_profile_ = second_profile_builder.Build();
  }

  TestingProfile* profile() { return profile_.get(); }

  TestingProfile* second_profile() { return second_profile_.get(); }

  // Create test bookmark entries to be added to ProfileWriter to
  // simulate bookmark importing.
  void CreateImportedBookmarksEntries() {
    AddImportedBookmarkEntry(GURL("http://www.google.com"), u"Google");
    AddImportedBookmarkEntry(GURL("http://www.yahoo.com"), u"Yahoo");
  }

  // Helper function to create history entries.
  history::URLRow MakeURLRow(const char* url,
                             std::u16string title,
                             int visit_count,
                             int days_since_last_visit,
                             int typed_count) {
    history::URLRow row(GURL(url), 0);
    row.set_title(title);
    row.set_visit_count(visit_count);
    row.set_typed_count(typed_count);
    row.set_last_visit(base::Time::NowFromSystemTime() -
                       base::Days(days_since_last_visit));
    return row;
  }

  // Create test history entries to be added to ProfileWriter to
  // simulate history importing.
  void CreateHistoryPageEntries() {
    history::URLRow row1(
        MakeURLRow("http://www.google.com", u"Google", 3, 10, 1));
    history::URLRow row2(
        MakeURLRow("http://www.yahoo.com", u"Yahoo", 3, 30, 10));
    pages_.push_back(row1);
    pages_.push_back(row2);
  }

  void VerifyBookmarksCount(const std::vector<UrlAndTitle>& bookmarks_record,
                            BookmarkModel* bookmark_model,
                            size_t expected) {
    for (auto bookmark : bookmarks_record) {
      std::vector<TitledUrlMatch> matches =
          bookmark_model->GetBookmarksMatching(
              bookmark.title, 10, query_parser::MatchingAlgorithm::DEFAULT);
      EXPECT_EQ(expected, matches.size());
    }
  }

  void VerifyHistoryCount(Profile* profile) {
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS);
    history::QueryOptions options;
    base::CancelableTaskTracker history_task_tracker;
    base::RunLoop loop;
    history_service->QueryHistory(
        std::u16string(), options,
        base::BindLambdaForTesting([&](history::QueryResults results) {
          history_count_ = results.size();
          loop.Quit();
        }),
        &history_task_tracker);
    loop.Run();
  }

  // Creates a TemplateURL from the provided data.
  std::unique_ptr<TemplateURL> CreateTemplateURL(const std::string& keyword,
                                                 const std::string& url,
                                                 const std::string& short_name);

 protected:
  std::vector<ImportedBookmarkEntry> bookmarks_;
  history::URLRows pages_;
  size_t history_count_;

 private:
  void AddImportedBookmarkEntry(const GURL& url, const std::u16string& title) {
    base::Time date;
    ImportedBookmarkEntry entry;
    entry.creation_time = date;
    entry.url = url;
    entry.title = title;
    entry.in_toolbar = true;
    entry.is_folder = false;
    bookmarks_.push_back(entry);
  }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestingProfile> second_profile_;
};

// Add bookmarks via ProfileWriter to profile1 when profile2 also exists.
TEST_F(ProfileWriterTest, CheckBookmarksWithMultiProfile) {
  BookmarkModel* bookmark_model2 =
      BookmarkModelFactory::GetForBrowserContext(second_profile());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model2);
  bookmarks::AddIfNotBookmarked(bookmark_model2, GURL("http://www.bing.com"),
                                u"Bing");

  CreateImportedBookmarksEntries();
  BookmarkModel* bookmark_model1 =
      BookmarkModelFactory::GetForBrowserContext(profile());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model1);

  scoped_refptr<TestProfileWriter> profile_writer(
      new TestProfileWriter(profile()));
  profile_writer->AddBookmarks(bookmarks_, u"Imported from Firefox");

  std::vector<UrlAndTitle> url_record1 = bookmark_model1->GetUniqueUrls();
  EXPECT_EQ(2u, url_record1.size());

  std::vector<UrlAndTitle> url_record2 = bookmark_model2->GetUniqueUrls();
  EXPECT_EQ(1u, url_record2.size());
}

// Verify that bookmarks are duplicated when added twice.
TEST_F(ProfileWriterTest, CheckBookmarksAfterWritingDataTwice) {
  CreateImportedBookmarksEntries();
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

  scoped_refptr<TestProfileWriter> profile_writer(
      new TestProfileWriter(profile()));
  profile_writer->AddBookmarks(bookmarks_, u"Imported from Firefox");
  std::vector<UrlAndTitle> bookmarks_record = bookmark_model->GetUniqueUrls();
  EXPECT_EQ(2u, bookmarks_record.size());

  VerifyBookmarksCount(bookmarks_record, bookmark_model, 1);

  profile_writer->AddBookmarks(bookmarks_, u"Imported from Firefox");
  // Verify that duplicate bookmarks exist.
  VerifyBookmarksCount(bookmarks_record, bookmark_model, 2);
}

std::unique_ptr<TemplateURL> ProfileWriterTest::CreateTemplateURL(
    const std::string& keyword,
    const std::string& url,
    const std::string& short_name) {
  TemplateURLData data;
  data.SetKeyword(base::ASCIIToUTF16(keyword));
  data.SetShortName(base::ASCIIToUTF16(short_name));
  data.SetURL(TemplateURLRef::DisplayURLToURLRef(base::ASCIIToUTF16(url)));
  return std::make_unique<TemplateURL>(data);
}

// Verify that history entires are not duplicated when added twice.
TEST_F(ProfileWriterTest, CheckHistoryAfterWritingDataTwice) {
  profile()->BlockUntilHistoryProcessesPendingRequests();

  CreateHistoryPageEntries();
  scoped_refptr<TestProfileWriter> profile_writer(
      new TestProfileWriter(profile()));
  profile_writer->AddHistoryPage(pages_, history::SOURCE_FIREFOX_IMPORTED);
  VerifyHistoryCount(profile());
  size_t original_history_count = history_count_;
  history_count_ = 0;

  profile_writer->AddHistoryPage(pages_, history::SOURCE_FIREFOX_IMPORTED);
  VerifyHistoryCount(profile());
  EXPECT_EQ(original_history_count, history_count_);
}

TEST_F(ProfileWriterTest, AddKeywords) {
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(),
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));

  TemplateURLService::OwnedTemplateURLVector keywords;
  keywords.push_back(CreateTemplateURL("key1", "http://key1.com", "n1"));
  // This entry will not be added since it has the same key as an existing
  // keyword.
  keywords.push_back(CreateTemplateURL("key1", "http://key1_1.com", "n1_1"));
  keywords.push_back(CreateTemplateURL("key2", "http://key2.com", "n2"));

  auto profile_writer = base::MakeRefCounted<TestProfileWriter>(profile());
  profile_writer->AddKeywords(std::move(keywords), false);

  TemplateURLService* turl_model =
      TemplateURLServiceFactory::GetForProfile(profile());
  auto turls = turl_model->GetTemplateURLs();
  EXPECT_EQ(turls.size(), 2u);

  EXPECT_EQ(turls[0]->keyword(), u"key1");
  EXPECT_EQ(turls[0]->url(), "http://key1.com");
  EXPECT_EQ(turls[0]->short_name(), u"n1");

  EXPECT_EQ(turls[1]->keyword(), u"key2");
  EXPECT_EQ(turls[1]->url(), "http://key2.com");
  EXPECT_EQ(turls[1]->short_name(), u"n2");
}

TEST_F(ProfileWriterTest, AddPassword) {
  scoped_refptr<TestPasswordStore> store =
      CreateAndUseTestPasswordStore(profile());
  PasswordForm form = MakePasswordForm();

  auto profile_writer = base::MakeRefCounted<TestProfileWriter>(profile());
  profile_writer->AddPasswordForm(form);

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(store->stored_passwords().at(form.signon_realm),
              testing::ElementsAre(form));
}

TEST_F(ProfileWriterTest, AddPasswordDisabled) {
  scoped_refptr<TestPasswordStore> store =
      CreateAndUseTestPasswordStore(profile());
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableService, false);
  PasswordForm form = MakePasswordForm();

  auto profile_writer = base::MakeRefCounted<TestProfileWriter>(profile());
  profile_writer->AddPasswordForm(form);

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(store->stored_passwords(), testing::IsEmpty());
}
