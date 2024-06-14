// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_image_search/annotation_storage.h"

#include <iostream>
#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/app_list/search/local_image_search/image_annotation_worker.h"
#include "chrome/browser/ash/app_list/search/local_image_search/local_image_search_test_util.h"
#include "chrome/browser/ash/app_list/search/local_image_search/sql_database.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

int CreateOldTestSchema(SqlDatabase* db) {
  static constexpr char kQuery[] =
      // clang-format off
            "CREATE TABLE test("
              "key TEXT NOT NULL)";
  // clang-format on
  db->GetStatementForQuery(SQL_FROM_HERE, kQuery)->Run();
  return 2;
}

class AnnotationStorageTest : public testing::Test {
 protected:
  // testing::Test overrides:
  void SetUp() override {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    test_directory_ = temp_dir.GetPath();
    base::FilePath test_db = test_directory_.AppendASCII("test.db");
    storage_ =
        std::make_unique<AnnotationStorage>(std::move(test_db),
                                            /*annotation_worker=*/nullptr);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<AnnotationStorage> storage_;
  base::FilePath test_directory_;
};

TEST_F(AnnotationStorageTest, EmptyStorage) {
  storage_->Initialize();
  task_environment_.RunUntilIdle();

  auto annotations = storage_->GetAllAnnotationsForTest();
  EXPECT_TRUE(annotations.empty());

  task_environment_.RunUntilIdle();
}

TEST_F(AnnotationStorageTest, Insert) {
  storage_->Initialize();
  task_environment_.RunUntilIdle();

  auto time = base::Time::Now();
  ImageInfo bar_image({"test"}, test_directory_.AppendASCII("bar.jpg"), time,
                      /*file_size=*/1);

  storage_->Insert(bar_image);

  EXPECT_THAT(storage_->GetAllAnnotationsForTest(),
              testing::ElementsAreArray({bar_image}));
  task_environment_.RunUntilIdle();

  ImageInfo bar_image1({"test1"}, test_directory_.AppendASCII("bar.jpg"),
                       std::move(time), 1);

  storage_->Insert(bar_image1);

  EXPECT_THAT(storage_->GetAllAnnotationsForTest(),
              testing::ElementsAreArray({bar_image, bar_image1}));
  task_environment_.RunUntilIdle();

  ImageInfo foo_image({"test1"}, test_directory_.AppendASCII("foo.png"),
                      base::Time::Now(), 2);

  storage_->Insert(foo_image);

  EXPECT_THAT(
      storage_->GetAllAnnotationsForTest(),
      testing::UnorderedElementsAreArray({bar_image, bar_image1, foo_image}));
  task_environment_.RunUntilIdle();
}

TEST_F(AnnotationStorageTest, Remove) {
  storage_->Initialize();
  task_environment_.RunUntilIdle();

  auto time = base::Time::Now();
  ImageInfo bar_image({"test"}, test_directory_.AppendASCII("bar.jpg"),
                      base::Time::Now(), /*file_size=*/1);
  ImageInfo foo_image({"test"}, test_directory_.AppendASCII("foo.png"), time,
                      2);
  ImageInfo foo_image1({"test1"}, test_directory_.AppendASCII("foo.png"),
                       std::move(time), 2);
  storage_->Insert(bar_image);
  storage_->Insert(foo_image);
  storage_->Insert(foo_image1);

  storage_->Remove(test_directory_.AppendASCII("bar.jpg"));

  EXPECT_THAT(storage_->GetAllAnnotationsForTest(),
              testing::ElementsAreArray({foo_image, foo_image1}));

  storage_->Remove(test_directory_.AppendASCII("bar.jpg"));

  EXPECT_THAT(storage_->GetAllAnnotationsForTest(),
              testing::ElementsAreArray({foo_image, foo_image1}));

  storage_->Remove(test_directory_.AppendASCII("foo.png"));
  EXPECT_TRUE(storage_->GetAllAnnotationsForTest().empty());

  storage_->Remove(test_directory_.AppendASCII("foo.png"));
  EXPECT_TRUE(storage_->GetAllAnnotationsForTest().empty());

  task_environment_.RunUntilIdle();
}

TEST_F(AnnotationStorageTest, FindImagePath) {
  storage_->Initialize();
  task_environment_.RunUntilIdle();

  ImageInfo bar_image({"test"}, test_directory_.AppendASCII("bar.jpg"),
                      base::Time::Now(), /*file_size=*/1);
  ImageInfo foo_image({"test1"}, test_directory_.AppendASCII("foo.png"),
                      base::Time::Now(), 2);
  storage_->Insert(bar_image);
  storage_->Insert(foo_image);

  auto expect_bar =
      storage_->FindImagePath(test_directory_.AppendASCII("bar.jpg"));
  EXPECT_THAT(expect_bar, testing::ElementsAreArray({bar_image}));

  auto expect_foo =
      storage_->FindImagePath(test_directory_.AppendASCII("foo.png"));
  EXPECT_THAT(expect_foo, testing::ElementsAreArray({foo_image}));

  task_environment_.RunUntilIdle();
}

TEST_F(AnnotationStorageTest, GetLastModifiedTime) {
  storage_->Initialize();
  task_environment_.RunUntilIdle();

  ImageInfo bar_image({"test"}, test_directory_.AppendASCII("bar.jpg"),
                      base::Time::Now(), /*file_size=*/1);
  ImageInfo foo_image({"test1"}, test_directory_.AppendASCII("foo.png"),
                      base::Time::Now(), /*file_size=*/2);
  // Ensures they are not both the default time.
  EXPECT_NE(foo_image.last_modified, bar_image.last_modified);

  storage_->Insert(bar_image);
  storage_->Insert(foo_image);

  auto expect_bar =
      storage_->GetLastModifiedTime(test_directory_.AppendASCII("bar.jpg"));
  EXPECT_THAT(expect_bar, bar_image.last_modified);

  auto expect_foo =
      storage_->GetLastModifiedTime(test_directory_.AppendASCII("foo.png"));
  EXPECT_THAT(expect_foo, foo_image.last_modified);

  task_environment_.RunUntilIdle();
}

// Search quality test. Used to fine-tune the precision of search.
TEST_F(AnnotationStorageTest, SearchAnnotations) {
  storage_->Initialize();
  task_environment_.RunUntilIdle();

  ImageInfo document_image1({"test", "bar", "test1"},
                            test_directory_.AppendASCII("document1.jpg"),
                            base::Time::Now(), /*file_size=*/1);
  ImageInfo document_image2({"testing", "testing_long"},
                            test_directory_.AppendASCII("document2.jpg"),
                            base::Time::Now(), 2);
  ImageInfo document_image3({"testing_long"},
                            test_directory_.AppendASCII("document3.jpg"),
                            base::Time::Now(), 3);
  ImageInfo foo_image({"test1"}, test_directory_.AppendASCII("foo.png"),
                      base::Time::Now(), 4);
  storage_->Insert(document_image1);
  storage_->Insert(document_image2);
  storage_->Insert(document_image3);
  storage_->Insert(foo_image);

  EXPECT_THAT(
      storage_->Search(base::UTF8ToUTF16(std::string("bar tes")),
                       /*max_num_results=*/5),
      testing::ElementsAreArray({FileSearchResult(
          document_image1.path, document_image1.last_modified, 1.8571 / 2)}));

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("test")), 5),
              testing::ElementsAreArray(
                  {FileSearchResult(document_image1.path,
                                    document_image1.last_modified, 1),
                   FileSearchResult(foo_image.path, foo_image.last_modified,
                                    0.88888)}));

  EXPECT_THAT(
      storage_->Search(base::UTF8ToUTF16(std::string("testi")), 5),
      testing::ElementsAreArray({FileSearchResult(
          document_image2.path, document_image2.last_modified, 0.833333)}));

  EXPECT_THAT(
      storage_->Search(base::UTF8ToUTF16(std::string("testin")), 5),
      testing::ElementsAreArray({FileSearchResult(
          document_image2.path, document_image2.last_modified, 0.923077)}));

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("testing")), 5),
              testing::ElementsAreArray({FileSearchResult(
                  document_image2.path, document_image2.last_modified, 1)}));

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("testing_")), 5),
              testing::UnorderedElementsAreArray(
                  {FileSearchResult(document_image2.path,
                                    document_image2.last_modified, 0.8),
                   FileSearchResult(document_image3.path,
                                    document_image3.last_modified, 0.8)}));

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("testing_l")), 5),
              testing::UnorderedElementsAreArray(
                  {FileSearchResult(document_image2.path,
                                    document_image2.last_modified, 0.857143),
                   FileSearchResult(document_image3.path,
                                    document_image3.last_modified, 0.857143)}));

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("testing-")), 5),
              testing::ElementsAreArray({FileSearchResult(
                  document_image2.path, document_image2.last_modified, 1)}));

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("testing-l")), 5),
              testing::ElementsAreArray(std::vector<FileSearchResult>()));

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("est")), 5),
              testing::ElementsAreArray(std::vector<FileSearchResult>()));

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("Test")), 5),
              testing::ElementsAreArray(
                  {FileSearchResult(document_image1.path,
                                    document_image1.last_modified, 1),
                   FileSearchResult(foo_image.path, foo_image.last_modified,
                                    0.88888)}));

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("TEST")), 5),
              testing::ElementsAreArray(
                  {FileSearchResult(document_image1.path,
                                    document_image1.last_modified, 1),
                   FileSearchResult(foo_image.path, foo_image.last_modified,
                                    0.88888)}));

  task_environment_.RunUntilIdle();
}

TEST_F(AnnotationStorageTest, MaxResult) {
  storage_->Initialize();
  task_environment_.RunUntilIdle();

  auto time = base::Time::Now();
  ImageInfo document_image1({"test", "bar", "test1"},
                            test_directory_.AppendASCII("document1.jpg"), time,
                            /*file_size=*/1);
  ImageInfo document_image2(
      {"bar", "test1"}, test_directory_.AppendASCII("document2.jpg"), time, 2);
  ImageInfo document_image3(
      {"bar", "test1"}, test_directory_.AppendASCII("document3.jpg"), time, 3);
  ImageInfo foo_image({"test1"}, test_directory_.AppendASCII("foo.png"), time,
                      4);

  storage_->Insert(document_image1);
  storage_->Insert(document_image2);
  storage_->Insert(document_image3);
  storage_->Insert(foo_image);

  EXPECT_THAT(
      storage_->Search(base::UTF8ToUTF16(std::string("bar test")),
                       /*max_num_results=*/4),
      testing::ElementsAreArray(
          {FileSearchResult(document_image1.path, document_image1.last_modified,
                            2 / 2),
           FileSearchResult(document_image2.path, document_image2.last_modified,
                            1.888 / 2),
           FileSearchResult(document_image3.path, document_image3.last_modified,
                            1.888 / 2)}));

  EXPECT_THAT(
      storage_->Search(base::UTF8ToUTF16(std::string("bar test")), 3),
      testing::ElementsAreArray(
          {FileSearchResult(document_image1.path, document_image1.last_modified,
                            2 / 2),
           FileSearchResult(document_image2.path, document_image2.last_modified,
                            1.888 / 2),
           FileSearchResult(document_image3.path, document_image3.last_modified,
                            1.888 / 2)}));

  EXPECT_THAT(
      storage_->Search(base::UTF8ToUTF16(std::string("bar test")), 2),
      testing::ElementsAreArray(
          {FileSearchResult(document_image1.path, document_image1.last_modified,
                            2 / 2),
           FileSearchResult(document_image2.path, document_image2.last_modified,
                            1.888 / 2)}));

  EXPECT_THAT(
      storage_->Search(base::UTF8ToUTF16(std::string("bar test")), 1),
      testing::ElementsAreArray({FileSearchResult(
          document_image1.path, document_image1.last_modified, 2 / 2)}));

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("bar test")), 0),
              testing::ElementsAreArray(std::vector<FileSearchResult>()));

  task_environment_.RunUntilIdle();
}

TEST_F(AnnotationStorageTest, QueryWithStopWords) {
  storage_->Initialize();
  task_environment_.RunUntilIdle();

  ImageInfo document_image1({"test", "bar", "test1"},
                            test_directory_.AppendASCII("document1.jpg"),
                            base::Time::Now(), /*file_size=*/1);
  ImageInfo document_image2({"bar", "test1"},
                            test_directory_.AppendASCII("document2.jpg"),
                            base::Time::Now(), 2);

  storage_->Insert(document_image1);
  storage_->Insert(document_image2);

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("a bar")),
                               /*max_num_results=*/4),
              testing::UnorderedElementsAreArray(
                  {FileSearchResult(document_image1.path,
                                    document_image1.last_modified, 1),
                   FileSearchResult(document_image2.path,
                                    document_image2.last_modified, 1)}));

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("an bar")), 4),
              testing::UnorderedElementsAreArray(
                  {FileSearchResult(document_image1.path,
                                    document_image1.last_modified, 1),
                   FileSearchResult(document_image2.path,
                                    document_image2.last_modified, 1)}));

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("bar a")), 4),
              testing::UnorderedElementsAreArray(
                  {FileSearchResult(document_image1.path,
                                    document_image1.last_modified, 1),
                   FileSearchResult(document_image2.path,
                                    document_image2.last_modified, 1)}));

  task_environment_.RunUntilIdle();
}

TEST_F(AnnotationStorageTest, SchemaMigration) {
  storage_.reset();

  auto sql_database = std::make_unique<SqlDatabase>(
      test_directory_.AppendASCII("test.db"), /*histogram_tag=*/"test",
      /*current_version_number=*/2, base::BindRepeating(CreateOldTestSchema),
      base::BindRepeating([](SqlDatabase* db, int current_version_number) {
        return current_version_number;
      }));

  ASSERT_TRUE(sql_database->Initialize());
  task_environment_.RunUntilIdle();
  sql_database->Close();

  storage_ = std::make_unique<AnnotationStorage>(
      test_directory_.AppendASCII("test.db"),
      /*annotation_worker=*/nullptr);
  storage_->Initialize();
  task_environment_.RunUntilIdle();

  ImageInfo bar_image({"test"}, test_directory_.AppendASCII("bar.jpg"),
                      base::Time::Now(), /*file_size=*/1);

  storage_->Insert(bar_image);
  EXPECT_THAT(storage_->GetAllAnnotationsForTest(),
              testing::ElementsAreArray({bar_image}));
  task_environment_.RunUntilIdle();
}

TEST_F(AnnotationStorageTest, SearchByDirectory) {
  storage_->Initialize();
  task_environment_.RunUntilIdle();

  ImageInfo document_image1({"test", "bar", "test1"},
                            test_directory_.AppendASCII("document1.jpg"),
                            base::Time::Now(), /*file_size=*/1);
  ImageInfo foo_image(
      {"test1"},
      test_directory_.AppendASCII("New Folder").AppendASCII("foo.png"),
      base::Time::Now(), 4);

  storage_->Insert(document_image1);
  storage_->Insert(foo_image);

  EXPECT_THAT(
      storage_->SearchByDirectory(test_directory_),
      testing::ElementsAreArray({document_image1.path, foo_image.path}));

  EXPECT_THAT(
      storage_->SearchByDirectory(test_directory_.AppendASCII("New Folder")),
      testing::ElementsAreArray({foo_image.path}));

  task_environment_.RunUntilIdle();
}

TEST_F(AnnotationStorageTest, GetAllFiles) {
  storage_->Initialize();
  task_environment_.RunUntilIdle();

  ImageInfo document_image1({"test", "bar", "test1"},
                            test_directory_.AppendASCII("document1.jpg"),
                            base::Time::Now(), /*file_size=*/1);
  ImageInfo foo_image(
      {"test1"},
      test_directory_.AppendASCII("New Folder").AppendASCII("foo.png"),
      base::Time::Now(), 4);

  storage_->Insert(document_image1);
  storage_->Insert(foo_image);

  EXPECT_THAT(
      storage_->GetAllFiles(),
      testing::ElementsAreArray({document_image1.path, foo_image.path}));

  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace app_list
