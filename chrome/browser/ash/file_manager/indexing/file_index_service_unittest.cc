// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/file_index_service.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_manager/indexing/file_info.h"
#include "chrome/browser/ash/file_manager/indexing/query.h"
#include "chrome/browser/ash/file_manager/indexing/term.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace file_manager {
namespace {

GURL MakeLocalURL(const std::string& file_name) {
  return GURL(base::StrCat(
      {"filesystem:chrome://file-manager/external/Downloads-user123/",
       file_name}));
}

GURL MakeDriveURL(const std::string& file_name) {
  return GURL(base::StrCat(
      {"filesystem:chrome://file-manager/external/drivefs-987654321/",
       file_name}));
}

MATCHER_P(ContainsFiles, expected_files, "") {
  std::set<FileInfo> result_set;
  for (const Match& match : arg.matches) {
    result_set.emplace(match.file_info);
  }
  std::set<FileInfo> expected_set;
  for (const FileInfo& info : expected_files) {
    expected_set.emplace(info);
  }
  return expected_set == result_set;
}

class FileIndexServiceTest : public testing::Test {
 public:
  FileIndexServiceTest()
      : pinned_("label", u"pinned"),
        downloaded_("label", u"downloaded"),
        starred_("label", u"starred"),
        foo_url_(MakeLocalURL("foo.txt")),
        bar_url_(MakeLocalURL("bar.txt")),
        task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override { CreateIndex(); }

  void TearDown() override { DestroyIndex(); }

  // Convenience methods that convert asynchronous results to synchronous.
  void CreateIndex() {
    index_service_ = std::make_unique<FileIndexService>(&profile_);
    ASSERT_EQ(Init(), OpResults::kSuccess);
  }

  void DestroyIndex() { index_service_.reset(); }

  OpResults Init() {
    base::RunLoop run_loop;
    OpResults outcome;
    index_service_->Init(base::BindLambdaForTesting([&](OpResults results) {
      outcome = results;
      run_loop.Quit();
    }));
    run_loop.Run();
    return outcome;
  }

  SearchResults Search(const Query& query) {
    base::RunLoop run_loop;
    SearchResults outcome;
    index_service_->Search(
        query, base::BindLambdaForTesting([&](SearchResults results) {
          outcome.total_matches = results.total_matches;
          outcome.matches = results.matches;
          run_loop.Quit();
        }));
    run_loop.Run();
    return outcome;
  }

  OpResults PutFileInfo(const FileInfo& file_info) {
    base::RunLoop run_loop;
    OpResults outcome;
    index_service_->PutFileInfo(
        file_info, base::BindLambdaForTesting([&](OpResults results) {
          outcome = results;
          run_loop.Quit();
        }));
    run_loop.Run();
    return outcome;
  }

  OpResults UpdateTerms(const std::vector<Term> terms, const GURL& url) {
    base::RunLoop run_loop;
    OpResults outcome;
    index_service_->UpdateTerms(
        terms, url, base::BindLambdaForTesting([&](OpResults results) {
          outcome = results;
          run_loop.Quit();
        }));
    run_loop.Run();
    return outcome;
  }

  OpResults AugmentTerms(const std::vector<Term> terms, const GURL& url) {
    base::RunLoop run_loop;
    OpResults outcome;
    index_service_->AugmentTerms(
        terms, url, base::BindLambdaForTesting([&](OpResults results) {
          outcome = results;
          run_loop.Quit();
        }));
    run_loop.Run();
    return outcome;
  }

  OpResults RemoveTerms(const std::vector<Term> terms, const GURL& url) {
    base::RunLoop run_loop;
    OpResults outcome;
    index_service_->RemoveTerms(
        terms, url, base::BindLambdaForTesting([&](OpResults results) {
          outcome = results;
          run_loop.Quit();
        }));
    run_loop.Run();
    return outcome;
  }

  OpResults RemoveFile(const GURL& url) {
    base::RunLoop run_loop;
    OpResults outcome;
    index_service_->RemoveFile(
        url, base::BindLambdaForTesting([&](OpResults results) {
          outcome = results;
          run_loop.Quit();
        }));
    run_loop.Run();
    return outcome;
  }

  std::unique_ptr<FileIndexService> index_service_;
  Term pinned_;
  Term downloaded_;
  Term starred_;
  GURL foo_url_;
  GURL bar_url_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

typedef std::vector<FileInfo> FileInfoList;

TEST_F(FileIndexServiceTest, InitializeTwice) {
  ASSERT_EQ(Init(), OpResults::kSuccess);
}

TEST_F(FileIndexServiceTest, CreateDestroyCreate) {
  // Index is already created by SetUp(). Thus just destroy it and create.
  // it again.
  DestroyIndex();
  // TODO(b:327534824): Remove the sleep statement.
  base::PlatformThread::Sleep(base::Milliseconds(250));
  CreateIndex();
}

TEST_F(FileIndexServiceTest, EmptySearch) {
  // Empty query on an empty index.
  EXPECT_THAT(Search(Query({})), ContainsFiles(FileInfoList{}));

  FileInfo file_info(foo_url_, 1024, base::Time());
  EXPECT_EQ(PutFileInfo(file_info), OpResults::kSuccess);
  EXPECT_EQ(UpdateTerms({pinned_}, file_info.file_url), OpResults::kSuccess);

  // Empty query on an non-empty index.
  EXPECT_THAT(Search(Query({})), ContainsFiles(FileInfoList{}));
}

TEST_F(FileIndexServiceTest, SimpleMatch) {
  FileInfo file_info(foo_url_, 1024, base::Time());

  EXPECT_EQ(PutFileInfo(file_info), OpResults::kSuccess);
  EXPECT_EQ(UpdateTerms({pinned_}, file_info.file_url), OpResults::kSuccess);
  EXPECT_THAT(Search(Query({pinned_})), ContainsFiles(FileInfoList{file_info}));
}

TEST_F(FileIndexServiceTest, MultiTermMatch) {
  FileInfo file_info(foo_url_, 1024, base::Time());

  // Label file_info as pinned and starred.
  EXPECT_EQ(PutFileInfo(file_info), OpResults::kSuccess);
  EXPECT_EQ(UpdateTerms({pinned_, starred_}, file_info.file_url),
            OpResults::kSuccess);

  EXPECT_THAT(Search(Query({pinned_})), ContainsFiles(FileInfoList{file_info}));

  EXPECT_THAT(Search(Query({starred_})),
              ContainsFiles(FileInfoList{file_info}));

  EXPECT_THAT(Search(Query({pinned_, starred_})),
              ContainsFiles(FileInfoList{file_info}));
}

TEST_F(FileIndexServiceTest, AugmentTerms) {
  FileInfo file_info(foo_url_, 1024, base::Time());

  EXPECT_EQ(PutFileInfo(file_info), OpResults::kSuccess);
  // Label file_info as pinned and starred.
  EXPECT_EQ(UpdateTerms({downloaded_}, file_info.file_url),
            OpResults::kSuccess);

  // Can find by downloaded.
  EXPECT_THAT(Search(Query({downloaded_})),
              ContainsFiles(FileInfoList{file_info}));
  // Cannot find by starred.
  EXPECT_THAT(Search(Query({starred_})), ContainsFiles(FileInfoList{}));

  EXPECT_EQ(AugmentTerms({starred_}, foo_url_), OpResults::kSuccess);
  // Can find by downloaded.
  EXPECT_THAT(Search(Query({downloaded_})),
              ContainsFiles(FileInfoList{file_info}));
  // And by starred.
  EXPECT_THAT(Search(Query({starred_})),
              ContainsFiles(FileInfoList{file_info}));
  // And by starred and downloaded.
  EXPECT_THAT(Search(Query({starred_, downloaded_})),
              ContainsFiles(FileInfoList{file_info}));
}

TEST_F(FileIndexServiceTest, ReplaceTerms) {
  FileInfo file_info(foo_url_, 1024, base::Time());

  EXPECT_EQ(PutFileInfo(file_info), OpResults::kSuccess);
  // Start with the single label: downloaded.
  EXPECT_EQ(UpdateTerms({downloaded_}, file_info.file_url),
            OpResults::kSuccess);
  EXPECT_THAT(Search(Query({downloaded_})),
              ContainsFiles(FileInfoList{file_info}));
  EXPECT_THAT(Search(Query({starred_})), ContainsFiles(FileInfoList{}));

  // Just adding more labels: both downloaded and starred.
  EXPECT_EQ(UpdateTerms({downloaded_, starred_}, file_info.file_url),
            OpResults::kSuccess);
  EXPECT_THAT(Search(Query({downloaded_})),
              ContainsFiles(FileInfoList{file_info}));
  EXPECT_THAT(Search(Query({starred_})),
              ContainsFiles(FileInfoList{file_info}));

  // Remove the original "downloaded" label.
  EXPECT_EQ(UpdateTerms({starred_}, file_info.file_url), OpResults::kSuccess);
  EXPECT_THAT(Search(Query({downloaded_})), ContainsFiles(FileInfoList{}));
  EXPECT_THAT(Search(Query({starred_})),
              ContainsFiles(FileInfoList{file_info}));

  // Remove the "starred" label and add back "downloaded".
  EXPECT_EQ(UpdateTerms({downloaded_}, file_info.file_url),
            OpResults::kSuccess);
  EXPECT_THAT(Search(Query({downloaded_})),
              ContainsFiles(FileInfoList{file_info}));
  EXPECT_THAT(Search(Query({starred_})), ContainsFiles(FileInfoList({})));
}

TEST_F(FileIndexServiceTest, SearchMultipleFiles) {
  FileInfo foo_file_info(foo_url_, 1024, base::Time());

  EXPECT_EQ(PutFileInfo(foo_file_info), OpResults::kSuccess);
  EXPECT_EQ(UpdateTerms({downloaded_}, foo_file_info.file_url),
            OpResults::kSuccess);

  GURL bar_drive_url = MakeDriveURL("bar.txt");
  FileInfo bar_file_info(bar_drive_url, 1024, base::Time());
  EXPECT_EQ(PutFileInfo(bar_file_info), OpResults::kSuccess);
  EXPECT_EQ(UpdateTerms({downloaded_}, bar_file_info.file_url),
            OpResults::kSuccess);

  EXPECT_THAT(Search(Query({downloaded_})),
              ContainsFiles(FileInfoList{foo_file_info, bar_file_info}));
}

TEST_F(FileIndexServiceTest, SearchByNonexistingTerms) {
  FileInfo file_info(foo_url_, 1024, base::Time());
  EXPECT_EQ(PutFileInfo(file_info), OpResults::kSuccess);

  EXPECT_EQ(UpdateTerms({pinned_}, file_info.file_url), OpResults::kSuccess);

  EXPECT_THAT(Search(Query({downloaded_})), ContainsFiles(FileInfoList{}));
}

TEST_F(FileIndexServiceTest, EmptyUpdateIsInvalid) {
  FileInfo file_info(foo_url_, 1024, base::Time());
  EXPECT_EQ(PutFileInfo(file_info), OpResults::kSuccess);

  // Insert into the index with pinned label.
  EXPECT_EQ(UpdateTerms({pinned_}, file_info.file_url), OpResults::kSuccess);
  // Verify that passing empty terms is disallowed.
  EXPECT_EQ(UpdateTerms({}, file_info.file_url), OpResults::kArgumentError);

  EXPECT_THAT(Search(Query({pinned_})), ContainsFiles(FileInfoList{file_info}));
}

TEST_F(FileIndexServiceTest, FieldSeparator) {
  Term colon_in_field("foo:", u"one");
  FileInfo foo_info(foo_url_, 1024, base::Time());
  EXPECT_EQ(PutFileInfo(foo_info), OpResults::kSuccess);

  EXPECT_EQ(UpdateTerms({colon_in_field}, foo_info.file_url),
            OpResults::kSuccess);

  Term colon_in_text("foo", u":one");
  FileInfo bar_info(bar_url_, 1024, base::Time());
  EXPECT_EQ(PutFileInfo(bar_info), OpResults::kSuccess);
  EXPECT_EQ(UpdateTerms({colon_in_text}, bar_info.file_url),
            OpResults::kSuccess);

  EXPECT_THAT(Search(Query({colon_in_field})),
              ContainsFiles(FileInfoList{foo_info}));
  EXPECT_THAT(Search(Query({colon_in_text})),
              ContainsFiles(FileInfoList{bar_info}));
}

TEST_F(FileIndexServiceTest, GlobalSearch) {
  // Setup: two files, one marked with the label:starred, the other with
  // content:starred. This simulates the case where identical tokens, "starred"
  // came from two different sources (labeling, and file content).
  const std::u16string text = u"starred";
  Term label_term("label", text);
  Term content_term("content", text);
  FileInfo labeled_info(foo_url_, 1024, base::Time());
  FileInfo content_info(bar_url_, 1024, base::Time());

  EXPECT_EQ(PutFileInfo(labeled_info), OpResults::kSuccess);
  EXPECT_EQ(PutFileInfo(content_info), OpResults::kSuccess);

  EXPECT_EQ(UpdateTerms({label_term}, labeled_info.file_url),
            OpResults::kSuccess);
  EXPECT_EQ(UpdateTerms({content_term}, content_info.file_url),
            OpResults::kSuccess);

  // Searching with empty field name means global space search.
  EXPECT_THAT(Search(Query({Term("", text)})),
              ContainsFiles(FileInfoList{labeled_info, content_info}));
  // Searching with field name, gives us unique results.
  EXPECT_THAT(Search(Query({label_term})),
              ContainsFiles(FileInfoList{labeled_info}));
  EXPECT_THAT(Search(Query({content_term})),
              ContainsFiles(FileInfoList{content_info}));
}

TEST_F(FileIndexServiceTest, MixedSearch) {
  // Setup: two files, both starred, one labeled "tax", one containing the word
  // "tax" in its content.
  const std::u16string tax_text = u"tax";
  Term tax_content_term("content", tax_text);
  Term tax_label_term("label", tax_text);
  FileInfo tax_label_info(foo_url_, 1024, base::Time());
  FileInfo tax_content_info(bar_url_, 1024, base::Time());

  EXPECT_EQ(PutFileInfo(tax_label_info), OpResults::kSuccess);
  EXPECT_EQ(PutFileInfo(tax_content_info), OpResults::kSuccess);

  EXPECT_EQ(
      UpdateTerms({starred_, tax_content_term}, tax_content_info.file_url),
      OpResults::kSuccess);
  EXPECT_EQ(UpdateTerms({starred_, tax_label_term}, tax_label_info.file_url),
            OpResults::kSuccess);

  // Searching with "starred tax" should return both files.
  EXPECT_THAT(Search(Query({Term("", tax_text), Term("", u"starred")})),
              ContainsFiles(FileInfoList{tax_content_info, tax_label_info}));
  // Searching with with "label:starred content:tax" gives us just the file that
  // has "tax" in content.
  EXPECT_THAT(Search(Query({starred_, tax_content_term})),
              ContainsFiles(FileInfoList{tax_content_info}));
  // Searching with with "label:starred label:tax" gives us just the file that
  // has "tax" as a label.
  EXPECT_THAT(Search(Query({starred_, tax_label_term})),
              ContainsFiles(FileInfoList{tax_label_info}));
}

TEST_F(FileIndexServiceTest, RemoveFile) {
  // Empty remove.
  FileInfo foo_info(foo_url_, 1024, base::Time());
  EXPECT_EQ(RemoveFile(foo_info.file_url), OpResults::kSuccess);
  // Add foo_info to the index.
  EXPECT_EQ(PutFileInfo(foo_info), OpResults::kSuccess);
  EXPECT_EQ(UpdateTerms({starred_}, foo_info.file_url), OpResults::kSuccess);
  EXPECT_THAT(Search(Query({starred_})), ContainsFiles(FileInfoList{foo_info}));
  EXPECT_EQ(RemoveFile(foo_info.file_url), OpResults::kSuccess);
  EXPECT_THAT(Search(Query({starred_})), ContainsFiles(FileInfoList{}));
}

TEST_F(FileIndexServiceTest, RemoveTerms) {
  FileInfo foo_info(foo_url_, 1024, base::Time());

  EXPECT_EQ(RemoveTerms({}, foo_url_), OpResults::kSuccess);

  // Add terms for foo_info.
  EXPECT_EQ(PutFileInfo(foo_info), OpResults::kSuccess);
  EXPECT_EQ(UpdateTerms({starred_, downloaded_}, foo_info.file_url),
            OpResults::kSuccess);
  EXPECT_THAT(Search(Query({starred_})), ContainsFiles(FileInfoList{foo_info}));
  EXPECT_THAT(Search(Query({downloaded_})),
              ContainsFiles(FileInfoList{foo_info}));

  EXPECT_EQ(RemoveTerms({starred_}, foo_info.file_url), OpResults::kSuccess);

  EXPECT_TRUE(Search(Query({starred_})).matches.empty());
  EXPECT_THAT(Search(Query({downloaded_})),
              ContainsFiles(FileInfoList{foo_info}));

  // Remove more terms, including one that is no longer there.
  EXPECT_EQ(RemoveTerms({starred_, downloaded_}, foo_info.file_url),
            OpResults::kSuccess);

  EXPECT_TRUE(Search(Query({starred_})).matches.empty());
  EXPECT_TRUE(Search(Query({downloaded_})).matches.empty());
}

TEST_F(FileIndexServiceTest, AddOrUpdateBeforePut) {
  EXPECT_EQ(UpdateTerms({starred_}, foo_url_), OpResults::kFileMissing);
  EXPECT_EQ(AugmentTerms({starred_}, foo_url_), OpResults::kFileMissing);
}

}  // namespace
}  // namespace file_manager
