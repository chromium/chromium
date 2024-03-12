// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
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
        starred_("label", u"started"),
        task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    index_service_ = std::make_unique<FileIndexService>(&profile_);
  }

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
  index_service_->UpdateFile({pinned_}, file_info);

  // Empty query on an non-empty index.
  EXPECT_THAT(index_service_->Search(Query({})), testing::ElementsAre());
}

TEST_F(FileIndexServiceTest, SimpleMatch) {
  FileInfo file_info(MakeLocalURL("foo.txt"), 1024, base::Time());

  index_service_->UpdateFile({pinned_}, file_info);
  EXPECT_THAT(index_service_->Search(Query({pinned_})),
              testing::ElementsAre(file_info));
}

TEST_F(FileIndexServiceTest, MultiTermMatch) {
  FileInfo file_info(MakeLocalURL("foot.txt"), 1024, base::Time());

  // Label file_info as pinned and starred.
  index_service_->UpdateFile({pinned_, starred_}, file_info);

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
  index_service_->UpdateFile({downloaded_}, file_info);

  // Can find by downloaded.
  EXPECT_THAT(index_service_->Search(Query({downloaded_})),
              testing::ElementsAre(file_info));
  // Cannot find by starred.
  EXPECT_THAT(index_service_->Search(Query({starred_})),
              testing::ElementsAre());

  index_service_->AugmentFile({starred_}, file_info);
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
  index_service_->UpdateFile({downloaded_}, file_info);
  EXPECT_THAT(index_service_->Search(Query({downloaded_})),
              testing::ElementsAre(file_info));
  EXPECT_THAT(index_service_->Search(Query({starred_})),
              testing::ElementsAre());

  // Just adding more labels: both downloaded and starred.
  index_service_->UpdateFile({downloaded_, starred_}, file_info);
  EXPECT_THAT(index_service_->Search(Query({downloaded_})),
              testing::ElementsAre(file_info));
  EXPECT_THAT(index_service_->Search(Query({starred_})),
              testing::ElementsAre(file_info));

  // Remove the original "downloaded" label.
  index_service_->UpdateFile({starred_}, file_info);
  EXPECT_THAT(index_service_->Search(Query({downloaded_})),
              testing::ElementsAre());
  EXPECT_THAT(index_service_->Search(Query({starred_})),
              testing::ElementsAre(file_info));

  // Remove the "starred" label and add back "downloaded".
  index_service_->UpdateFile({downloaded_}, file_info);
  EXPECT_THAT(index_service_->Search(Query({downloaded_})),
              testing::ElementsAre(file_info));
  EXPECT_THAT(index_service_->Search(Query({starred_})),
              testing::ElementsAre());
}

TEST_F(FileIndexServiceTest, SearchMultipleFiles) {
  FileInfo foo_file_info(MakeLocalURL("foo.txt"), 1024, base::Time());
  index_service_->UpdateFile({downloaded_}, foo_file_info);

  FileInfo bar_file_info(MakeDriveURL("bar.txt"), 1024, base::Time());
  index_service_->UpdateFile({downloaded_}, bar_file_info);

  EXPECT_THAT(index_service_->Search(Query({downloaded_})),
              testing::ElementsAre(foo_file_info, bar_file_info));
}

TEST_F(FileIndexServiceTest, SearchByNonexistingTerms) {
  FileInfo file_info(MakeLocalURL("foo.txt"), 1024, base::Time());
  index_service_->UpdateFile({pinned_}, file_info);

  EXPECT_THAT(index_service_->Search(Query({downloaded_})),
              testing::ElementsAre());
}

TEST_F(FileIndexServiceTest, EmptyUpdateDeletes) {
  FileInfo file_info(MakeLocalURL("foo.txt"), 1024, base::Time());
  // Insert into the index with pinned label.
  index_service_->UpdateFile({pinned_}, file_info);
  // Remove from index by inserting it with no labels.
  index_service_->UpdateFile({}, file_info);

  EXPECT_THAT(index_service_->Search(Query({pinned_})), testing::ElementsAre());
}

TEST_F(FileIndexServiceTest, EmptyUpdateForUnknownFile) {
  FileInfo file_info(MakeLocalURL("foo.txt"), 1024, base::Time());
  // Inserting with empty terms does nothing.
  index_service_->UpdateFile({}, file_info);

  EXPECT_THAT(index_service_->Search(Query({pinned_})), testing::ElementsAre());
}

}  // namespace
}  // namespace file_manager
