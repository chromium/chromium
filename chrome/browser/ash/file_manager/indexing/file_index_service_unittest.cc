// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/ash/file_manager/indexing/file_index_service.h"
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
  return GURL("filesystem:chrome://file-manager/external/Downloads-user123/" +
              file_name);
}

GURL MakeDriveURL(const std::string& file_name) {
  return GURL("filesystem:chrome://file-manager/external/drivefs-987654321/" +
              file_name);
}

class FileIndexServiceTest : public testing::Test {
 public:
  FileIndexServiceTest()
      : pinned_("label", u"pinned"),
        downloaded_("label", u"downloaded"),
        starred_("label", u"starred"),
        task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    index_service_ = std::make_unique<FileIndexService>(&profile_);
  }

  void TearDown() override { index_service_.reset(); }

  std::unique_ptr<FileIndexService> index_service_;
  Term pinned_;
  Term downloaded_;
  Term starred_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(FileIndexServiceTest, EmptySearch) {
  // Empty query on an empty index.
  EXPECT_THAT(index_service_->Search(Query({})), testing::ElementsAre());

  FileInfo file_info(MakeLocalURL("foo.txt"), 1024, base::Time());
  EXPECT_EQ(index_service_->UpdateFile({pinned_}, file_info),
            OpResults::kSuccess);

  // Empty query on an non-empty index.
  EXPECT_THAT(index_service_->Search(Query({})), testing::ElementsAre());
}

TEST_F(FileIndexServiceTest, SimpleMatch) {
  FileInfo file_info(MakeLocalURL("foo.txt"), 1024, base::Time());

  EXPECT_EQ(index_service_->UpdateFile({pinned_}, file_info),
            OpResults::kSuccess);
  EXPECT_THAT(index_service_->Search(Query({pinned_})),
              testing::ElementsAre(file_info));
}

TEST_F(FileIndexServiceTest, MultiTermMatch) {
  FileInfo file_info(MakeLocalURL("foot.txt"), 1024, base::Time());

  // Label file_info as pinned and starred.
  EXPECT_EQ(index_service_->UpdateFile({pinned_, starred_}, file_info),
            OpResults::kSuccess);

  std::vector<FileInfo> pinned_files = index_service_->Search(Query({pinned_}));
  EXPECT_THAT(index_service_->Search(Query({pinned_})),
              testing::ElementsAre(file_info));

  EXPECT_THAT(index_service_->Search(Query({starred_})),
              testing::ElementsAre(file_info));

  std::vector<FileInfo> starred_and_pinned_files =
      index_service_->Search(Query({pinned_, starred_}));
  EXPECT_THAT(starred_and_pinned_files, testing::ElementsAre(file_info));
}

TEST_F(FileIndexServiceTest, AugmentTerms) {
  FileInfo file_info(MakeLocalURL("foot.txt"), 1024, base::Time());

  // Label file_info as pinned and starred.
  EXPECT_EQ(index_service_->UpdateFile({downloaded_}, file_info),
            OpResults::kSuccess);

  // Can find by downloaded.
  EXPECT_THAT(index_service_->Search(Query({downloaded_})),
              testing::ElementsAre(file_info));
  // Cannot find by starred.
  EXPECT_THAT(index_service_->Search(Query({starred_})),
              testing::ElementsAre());

  EXPECT_EQ(index_service_->AugmentFile({starred_}, file_info),
            OpResults::kSuccess);
  // Can find by downloaded.
  EXPECT_THAT(index_service_->Search(Query({downloaded_})),
              testing::ElementsAre(file_info));
  // And by starred.
  EXPECT_THAT(index_service_->Search(Query({starred_})),
              testing::ElementsAre(file_info));
  // And by starred and downloaded.
  EXPECT_THAT(index_service_->Search(Query({starred_, downloaded_})),
              testing::ElementsAre(file_info));
}

TEST_F(FileIndexServiceTest, ReplaceTerms) {
  FileInfo file_info(MakeLocalURL("foo.txt"), 1024, base::Time());

  // Start with the single label: downloaded.
  EXPECT_EQ(index_service_->UpdateFile({downloaded_}, file_info),
            OpResults::kSuccess);
  EXPECT_THAT(index_service_->Search(Query({downloaded_})),
              testing::ElementsAre(file_info));
  EXPECT_THAT(index_service_->Search(Query({starred_})),
              testing::ElementsAre());

  // Just adding more labels: both downloaded and starred.
  EXPECT_EQ(index_service_->UpdateFile({downloaded_, starred_}, file_info),
            OpResults::kSuccess);
  EXPECT_THAT(index_service_->Search(Query({downloaded_})),
              testing::ElementsAre(file_info));
  EXPECT_THAT(index_service_->Search(Query({starred_})),
              testing::ElementsAre(file_info));

  // Remove the original "downloaded" label.
  EXPECT_EQ(index_service_->UpdateFile({starred_}, file_info),
            OpResults::kSuccess);
  EXPECT_THAT(index_service_->Search(Query({downloaded_})),
              testing::ElementsAre());
  EXPECT_THAT(index_service_->Search(Query({starred_})),
              testing::ElementsAre(file_info));

  // Remove the "starred" label and add back "downloaded".
  EXPECT_EQ(index_service_->UpdateFile({downloaded_}, file_info),
            OpResults::kSuccess);
  EXPECT_THAT(index_service_->Search(Query({downloaded_})),
              testing::ElementsAre(file_info));
  EXPECT_THAT(index_service_->Search(Query({starred_})),
              testing::ElementsAre());
}

TEST_F(FileIndexServiceTest, SearchMultipleFiles) {
  FileInfo foo_file_info(MakeLocalURL("foo.txt"), 1024, base::Time());
  EXPECT_EQ(index_service_->UpdateFile({downloaded_}, foo_file_info),
            OpResults::kSuccess);

  FileInfo bar_file_info(MakeDriveURL("bar.txt"), 1024, base::Time());
  EXPECT_EQ(index_service_->UpdateFile({downloaded_}, bar_file_info),
            OpResults::kSuccess);

  EXPECT_THAT(index_service_->Search(Query({downloaded_})),
              testing::ElementsAre(foo_file_info, bar_file_info));
}

TEST_F(FileIndexServiceTest, SearchByNonexistingTerms) {
  FileInfo file_info(MakeLocalURL("foo.txt"), 1024, base::Time());
  EXPECT_EQ(index_service_->UpdateFile({pinned_}, file_info),
            OpResults::kSuccess);

  EXPECT_THAT(index_service_->Search(Query({downloaded_})),
              testing::ElementsAre());
}

TEST_F(FileIndexServiceTest, EmptyUpdateIsInvalid) {
  FileInfo file_info(MakeLocalURL("foo.txt"), 1024, base::Time());
  // Insert into the index with pinned label.
  EXPECT_EQ(index_service_->UpdateFile({pinned_}, file_info),
            OpResults::kSuccess);
  // Verify that passing empty terms is disallowed.
  EXPECT_EQ(index_service_->UpdateFile({}, file_info),
            OpResults::kArgumentError);

  EXPECT_THAT(index_service_->Search(Query({pinned_})),
              testing::ElementsAre(file_info));
}

TEST_F(FileIndexServiceTest, FieldSeparator) {
  Term colon_in_field("foo:", u"one");
  FileInfo foo_info(MakeLocalURL("foo.txt"), 1024, base::Time());
  EXPECT_EQ(index_service_->UpdateFile({colon_in_field}, foo_info),
            OpResults::kSuccess);

  Term colon_in_text("foo", u":one");
  FileInfo bar_info(MakeLocalURL("bar.txt"), 1024, base::Time());
  EXPECT_EQ(index_service_->UpdateFile({colon_in_text}, bar_info),
            OpResults::kSuccess);

  EXPECT_THAT(index_service_->Search(Query({colon_in_field})),
              testing::ElementsAre(foo_info));
  EXPECT_THAT(index_service_->Search(Query({colon_in_text})),
              testing::ElementsAre(bar_info));
}

TEST_F(FileIndexServiceTest, GlobalSearch) {
  // Setup: two files, one marked with the label:starred, the other with
  // content:starred. This simulates the case where identical tokens, "starred"
  // came from two different sources (labeling, and file content).
  const std::u16string text = u"starred";
  Term label_term("label", text);
  Term content_term("content", text);
  FileInfo labeled_info(MakeLocalURL("foo.txt"), 1024, base::Time());
  FileInfo content_info(MakeLocalURL("bar.txt"), 1024, base::Time());

  EXPECT_EQ(index_service_->UpdateFile({label_term}, labeled_info),
            OpResults::kSuccess);
  EXPECT_EQ(index_service_->UpdateFile({content_term}, content_info),
            OpResults::kSuccess);

  // Searching with empty field name means global space search.
  EXPECT_THAT(index_service_->Search(Query({Term("", text)})),
              testing::ElementsAre(labeled_info, content_info));
  // Searching with field name, gives us unique results.
  EXPECT_THAT(index_service_->Search(Query({label_term})),
              testing::ElementsAre(labeled_info));
  EXPECT_THAT(index_service_->Search(Query({content_term})),
              testing::ElementsAre(content_info));
}

TEST_F(FileIndexServiceTest, MixedSearch) {
  // Setup: two files, both starred, one labeled "tax", one containing the word
  // "tax" in its content.
  const std::u16string tax_text = u"tax";
  Term tax_content_term("content", tax_text);
  Term tax_label_term("label", tax_text);
  FileInfo tax_label_info(MakeLocalURL("foo.txt"), 1024, base::Time());
  FileInfo tax_content_info(MakeLocalURL("bar.txt"), 1024, base::Time());

  EXPECT_EQ(index_service_->UpdateFile({starred_, tax_content_term},
                                       tax_content_info),
            OpResults::kSuccess);
  EXPECT_EQ(
      index_service_->UpdateFile({starred_, tax_label_term}, tax_label_info),
      OpResults::kSuccess);

  // Searching with "starred tax" should return both files.
  EXPECT_THAT(
      index_service_->Search(Query({Term("", tax_text), Term("", u"starred")})),
      testing::ElementsAre(tax_content_info, tax_label_info));
  // Searching with with "label:starred content:tax" gives us just the file that
  // has "tax" in content.
  EXPECT_THAT(index_service_->Search(Query({starred_, tax_content_term})),
              testing::ElementsAre(tax_content_info));
  // Searching with with "label:starred label:tax" gives us just the file that
  // has "tax" as a label.
  EXPECT_THAT(index_service_->Search(Query({starred_, tax_label_term})),
              testing::ElementsAre(tax_label_info));
}

TEST_F(FileIndexServiceTest, RemoveFile) {
  // Empty remove.
  FileInfo foo_info(MakeLocalURL("foo.txt"), 1024, base::Time());
  EXPECT_EQ(index_service_->RemoveFile(foo_info.file_url), OpResults::kSuccess);
  // Add foo_info to the index.
  EXPECT_EQ(index_service_->UpdateFile({starred_}, foo_info),
            OpResults::kSuccess);
  EXPECT_THAT(index_service_->Search(Query({starred_})),
              testing::ElementsAre(foo_info));
  EXPECT_EQ(index_service_->RemoveFile(foo_info.file_url), OpResults::kSuccess);
  EXPECT_THAT(index_service_->Search(Query({starred_})),
              testing::ElementsAre());
}

}  // namespace
}  // namespace file_manager
