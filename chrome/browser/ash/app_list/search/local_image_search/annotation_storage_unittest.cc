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
#include "search_utils.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

constexpr double kDefaultScore = 0.7;
constexpr double kRelevanceWeight = 0.9;

double GetAdjustedRelevance(double relevance, double score = kDefaultScore) {
  return kRelevanceWeight * relevance + (1 - kRelevanceWeight) * score;
}

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

TEST_F(AnnotationStorageTest, InsertIca) {
  storage_->Initialize();
  auto time = base::Time::Now();

  // Inserts ICA annotation with full info.
  ImageInfo bar_image({}, test_directory_.AppendASCII("bar.jpg"), time,
                      /*file_size=*/1);
  AnnotationInfo info_1;
  info_1.score = 0.1f;
  info_1.x = 0.2f;
  info_1.y = 0.3f;
  info_1.area = 0.4f;
  bar_image.annotation_map["test1"] = info_1;

  storage_->Insert(bar_image, IndexingSource::kIca);
  EXPECT_THAT(storage_->GetAllAnnotationsForTest(),
              testing::ElementsAreArray({bar_image}));

  // Inserts ICA annotation with score only.
  ImageInfo bar_image1({}, test_directory_.AppendASCII("bar.png"),
                       std::move(time), 1);
  AnnotationInfo info_2;
  info_2.score = 0.5f;
  bar_image1.annotation_map["test1"] = info_2;

  storage_->Insert(bar_image1, IndexingSource::kIca);
  EXPECT_THAT(storage_->GetAllAnnotationsForTest(),
              testing::ElementsAreArray({bar_image, bar_image1}));

  // Inserts ICA result with multiple annotations.
  ImageInfo foo_image({}, test_directory_.AppendASCII("foo.png"),
                      base::Time::Now(), 2);

  ImageInfo foo_image_insert = foo_image;
  AnnotationInfo info_3;
  info_3.score = 0.6f;
  info_3.x = 0.7f;
  info_3.y = 0.8f;
  info_3.area = 0.1f;
  foo_image_insert.annotation_map["test3"] = info_3;
  AnnotationInfo info_4;
  info_4.score = 0.9f;
  foo_image_insert.annotation_map["test4"] = info_4;
  storage_->Insert(foo_image_insert, IndexingSource::kIca);

  // The expected results for multi-annotation insert.
  ImageInfo foo_image_expected1 = foo_image;
  foo_image_expected1.annotation_map["test3"] = info_3;
  ImageInfo foo_image_expected2 = foo_image;
  foo_image_expected2.annotation_map["test4"] = info_4;
  EXPECT_THAT(
      storage_->GetAllAnnotationsForTest(),
      testing::UnorderedElementsAreArray(
          {bar_image, bar_image1, foo_image_expected1, foo_image_expected2}));
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
      storage_->GetImageStatus(test_directory_.AppendASCII("bar.jpg"));
  EXPECT_THAT(expect_bar.last_modified, bar_image.last_modified);

  auto expect_foo =
      storage_->GetImageStatus(test_directory_.AppendASCII("foo.png"));
  EXPECT_THAT(expect_foo.last_modified, foo_image.last_modified);

  task_environment_.RunUntilIdle();
}

// Search quality test. Used to fine-tune the precision of search.
TEST_F(AnnotationStorageTest, SearchAnnotations) {
  storage_->Initialize();

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

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("bar tes")),
                               /*max_num_results=*/5),
              testing::ElementsAreArray({FileSearchResult(
                  document_image1.path, document_image1.last_modified,
                  GetAdjustedRelevance(1.8571 / 2))}));

  EXPECT_THAT(
      storage_->Search(base::UTF8ToUTF16(std::string("test")), 5),
      testing::ElementsAreArray(
          {FileSearchResult(document_image1.path, document_image1.last_modified,
                            GetAdjustedRelevance(1)),
           FileSearchResult(foo_image.path, foo_image.last_modified,
                            GetAdjustedRelevance(0.88888))}));

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("testi")), 5),
              testing::ElementsAreArray({FileSearchResult(
                  document_image2.path, document_image2.last_modified,
                  GetAdjustedRelevance(0.833333))}));

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("testin")), 5),
              testing::ElementsAreArray({FileSearchResult(
                  document_image2.path, document_image2.last_modified,
                  GetAdjustedRelevance(0.923077))}));

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("testing")), 5),
              testing::ElementsAreArray({FileSearchResult(
                  document_image2.path, document_image2.last_modified,
                  GetAdjustedRelevance(1))}));

  EXPECT_THAT(
      storage_->Search(base::UTF8ToUTF16(std::string("testing_")), 5),
      testing::UnorderedElementsAreArray(
          {FileSearchResult(document_image2.path, document_image2.last_modified,
                            GetAdjustedRelevance(0.8)),
           FileSearchResult(document_image3.path, document_image3.last_modified,
                            GetAdjustedRelevance(0.8))}));

  EXPECT_THAT(
      storage_->Search(base::UTF8ToUTF16(std::string("testing_l")), 5),
      testing::UnorderedElementsAreArray(
          {FileSearchResult(document_image2.path, document_image2.last_modified,
                            GetAdjustedRelevance(0.857143)),
           FileSearchResult(document_image3.path, document_image3.last_modified,
                            GetAdjustedRelevance(0.857143))}));

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("testing-")), 5),
              testing::ElementsAreArray({FileSearchResult(
                  document_image2.path, document_image2.last_modified,
                  GetAdjustedRelevance(1))}));

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("testing-l")), 5),
              testing::ElementsAreArray(std::vector<FileSearchResult>()));

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("est")), 5),
              testing::ElementsAreArray(std::vector<FileSearchResult>()));

  EXPECT_THAT(
      storage_->Search(base::UTF8ToUTF16(std::string("Test")), 5),
      testing::ElementsAreArray(
          {FileSearchResult(document_image1.path, document_image1.last_modified,
                            GetAdjustedRelevance(1)),
           FileSearchResult(foo_image.path, foo_image.last_modified,
                            GetAdjustedRelevance(0.88888))}));

  EXPECT_THAT(
      storage_->Search(base::UTF8ToUTF16(std::string("TEST")), 5),
      testing::ElementsAreArray(
          {FileSearchResult(document_image1.path, document_image1.last_modified,
                            GetAdjustedRelevance(1)),
           FileSearchResult(foo_image.path, foo_image.last_modified,
                            GetAdjustedRelevance(0.88888))}));
}

TEST_F(AnnotationStorageTest, SearchAnnotationsIca) {
  storage_->Initialize();

  // Inserts ICA results for images with the same annotations.
  ImageInfo bar_image1({}, test_directory_.AppendASCII("bar1.jpg"),
                       base::Time::Now(),
                       /*file_size=*/1);
  AnnotationInfo bar_annotation_info1;
  bar_annotation_info1.score = 0.5f;
  bar_image1.annotation_map["test"] = bar_annotation_info1;
  storage_->Insert(bar_image1, IndexingSource::kIca);

  ImageInfo bar_image2({}, test_directory_.AppendASCII("bar2.jpg"),
                       base::Time::Now(),
                       /*file_size=*/1);
  AnnotationInfo bar_annotation_info2;
  bar_annotation_info2.score = 0.6f;
  bar_image2.annotation_map["test"] = bar_annotation_info2;
  storage_->Insert(bar_image2, IndexingSource::kIca);

  ImageInfo bar_image3({}, test_directory_.AppendASCII("bar3.jpg"),
                       base::Time::Now(),
                       /*file_size=*/1);
  AnnotationInfo bar_annotation_info3;
  bar_annotation_info3.score = 0.7f;
  bar_image3.annotation_map["test"] = bar_annotation_info3;
  storage_->Insert(bar_image3, IndexingSource::kIca);

  // The images should return in the descending order of the annotation score.
  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("test")),
                               /*max_num_results=*/4),
              testing::ElementsAreArray(
                  {FileSearchResult(bar_image3.path, bar_image3.last_modified,
                                    GetAdjustedRelevance(1, 0.7)),
                   FileSearchResult(bar_image2.path, bar_image2.last_modified,
                                    GetAdjustedRelevance(1, 0.6)),
                   FileSearchResult(bar_image1.path, bar_image1.last_modified,
                                    GetAdjustedRelevance(1, 0.5))}));

  // Inserts ICA results for images with the same annotation scores.
  ImageInfo foo_image1({}, test_directory_.AppendASCII("foo1.jpg"),
                       base::Time::Now(),
                       /*file_size=*/1);
  AnnotationInfo foo_annotation_info1;
  foo_annotation_info1.score = 0.5f;
  foo_image1.annotation_map["bilibili"] = foo_annotation_info1;
  storage_->Insert(foo_image1, IndexingSource::kIca);

  ImageInfo foo_image2({}, test_directory_.AppendASCII("foo2.jpg"),
                       base::Time::Now(),
                       /*file_size=*/1);
  AnnotationInfo foo_annotation_info2;
  foo_annotation_info2.score = 0.5f;
  foo_image2.annotation_map["bilibil"] = foo_annotation_info2;
  storage_->Insert(foo_image2, IndexingSource::kIca);

  ImageInfo foo_image3({}, test_directory_.AppendASCII("foo3.jpg"),
                       base::Time::Now(),
                       /*file_size=*/1);
  AnnotationInfo foo_annotation_info3;
  foo_annotation_info3.score = 0.5f;
  foo_image3.annotation_map["bilibi"] = foo_annotation_info3;
  storage_->Insert(foo_image3, IndexingSource::kIca);

  // The images should return in the descending order of the annotation
  // relevance.
  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("bilibi")),
                               /*max_num_results=*/4),
              testing::ElementsAreArray(
                  {FileSearchResult(foo_image3.path, foo_image3.last_modified,
                                    GetAdjustedRelevance(1, 0.5)),
                   FileSearchResult(foo_image2.path, foo_image2.last_modified,
                                    GetAdjustedRelevance(0.923077, 0.5)),
                   FileSearchResult(foo_image1.path, foo_image1.last_modified,
                                    GetAdjustedRelevance(0.857143, 0.5))}));
}

TEST_F(AnnotationStorageTest, SearchAnnotationsMulti) {
  storage_->Initialize();

  auto time = base::Time::Now();
  ImageInfo bar_image({}, test_directory_.AppendASCII("bar.jpg"), time,
                      /*file_size=*/1);

  // Inserts OCR results.
  ImageInfo bar_image_ocr = bar_image;
  bar_image_ocr.annotations.insert("test");
  bar_image_ocr.annotations.insert("bar");
  storage_->Insert(bar_image_ocr, IndexingSource::kOcr);

  // Inserts ICA results with the same set of annotations.
  ImageInfo bar_image_ica = bar_image;

  AnnotationInfo test_annotation_info1;
  test_annotation_info1.score = 0.5f;
  bar_image_ica.annotation_map["test"] = test_annotation_info1;

  AnnotationInfo test_annotation_info2;
  test_annotation_info2.score = 0.9f;
  bar_image_ica.annotation_map["bar"] = test_annotation_info2;

  storage_->Insert(bar_image_ica, IndexingSource::kIca);

  // If the same annotation from ICA has a lower score than the default score of
  // OCR, we expect it to return the image only once with the relevance from
  // OCR.
  EXPECT_THAT(
      storage_->Search(base::UTF8ToUTF16(std::string("test")), 4),
      testing::ElementsAreArray({FileSearchResult(
          bar_image.path, bar_image.last_modified, GetAdjustedRelevance(1))}));

  // If the same annotation from ICA has a higher score than the default score
  // of OCR, we expect it to return the image only once with the relevance from
  // ICA.
  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("bar")), 4),
              testing::ElementsAreArray(
                  {FileSearchResult(bar_image.path, bar_image.last_modified,
                                    GetAdjustedRelevance(1, 0.9))}));
}

TEST_F(AnnotationStorageTest, MaxResult) {
  storage_->Initialize();

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
                            GetAdjustedRelevance(2 / 2)),
           FileSearchResult(document_image2.path, document_image2.last_modified,
                            GetAdjustedRelevance(1.888 / 2)),
           FileSearchResult(document_image3.path, document_image3.last_modified,
                            GetAdjustedRelevance(1.888 / 2))}));

  EXPECT_THAT(
      storage_->Search(base::UTF8ToUTF16(std::string("bar test")), 3),
      testing::ElementsAreArray(
          {FileSearchResult(document_image1.path, document_image1.last_modified,
                            GetAdjustedRelevance(2 / 2)),
           FileSearchResult(document_image2.path, document_image2.last_modified,
                            GetAdjustedRelevance(1.888 / 2)),
           FileSearchResult(document_image3.path, document_image3.last_modified,
                            GetAdjustedRelevance(1.888 / 2))}));

  EXPECT_THAT(
      storage_->Search(base::UTF8ToUTF16(std::string("bar test")), 2),
      testing::ElementsAreArray(
          {FileSearchResult(document_image1.path, document_image1.last_modified,
                            GetAdjustedRelevance(2 / 2)),
           FileSearchResult(document_image2.path, document_image2.last_modified,
                            GetAdjustedRelevance(1.888 / 2))}));

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("bar test")), 1),
              testing::ElementsAreArray({FileSearchResult(
                  document_image1.path, document_image1.last_modified,
                  GetAdjustedRelevance(2 / 2))}));

  EXPECT_THAT(storage_->Search(base::UTF8ToUTF16(std::string("bar test")), 0),
              testing::ElementsAreArray(std::vector<FileSearchResult>()));
}

TEST_F(AnnotationStorageTest, QueryWithStopWords) {
  storage_->Initialize();

  ImageInfo document_image1({"test", "bar", "test1"},
                            test_directory_.AppendASCII("document1.jpg"),
                            base::Time::Now(), /*file_size=*/1);
  ImageInfo document_image2({"bar", "test1"},
                            test_directory_.AppendASCII("document2.jpg"),
                            base::Time::Now(), 2);

  storage_->Insert(document_image1);
  storage_->Insert(document_image2);

  EXPECT_THAT(
      storage_->Search(base::UTF8ToUTF16(std::string("a bar")),
                       /*max_num_results=*/4),
      testing::UnorderedElementsAreArray(
          {FileSearchResult(document_image1.path, document_image1.last_modified,
                            GetAdjustedRelevance(1)),
           FileSearchResult(document_image2.path, document_image2.last_modified,
                            GetAdjustedRelevance(1))}));

  EXPECT_THAT(
      storage_->Search(base::UTF8ToUTF16(std::string("an bar")), 4),
      testing::UnorderedElementsAreArray(
          {FileSearchResult(document_image1.path, document_image1.last_modified,
                            GetAdjustedRelevance(1)),
           FileSearchResult(document_image2.path, document_image2.last_modified,
                            GetAdjustedRelevance(1))}));

  EXPECT_THAT(
      storage_->Search(base::UTF8ToUTF16(std::string("bar a")), 4),
      testing::UnorderedElementsAreArray(
          {FileSearchResult(document_image1.path, document_image1.last_modified,
                            GetAdjustedRelevance(1)),
           FileSearchResult(document_image2.path, document_image2.last_modified,
                            GetAdjustedRelevance(1))}));
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

TEST_F(AnnotationStorageTest, MultiSource) {
  storage_->Initialize();
  // Initially, the image should not exist in the database.
  EXPECT_EQ(storage_->GetImageStatus(test_directory_.AppendASCII("bar.jpg")),
            ImageStatus());

  auto time = base::Time::Now();
  ImageInfo bar_image({}, test_directory_.AppendASCII("bar.jpg"), time,
                      /*file_size=*/1);

  ImageInfo bar_image_ocr = bar_image;
  bar_image_ocr.annotations.insert("test");
  storage_->Insert(bar_image_ocr);

  EXPECT_THAT(storage_->GetAllAnnotationsForTest(),
              testing::ElementsAreArray({bar_image_ocr}));
  // When receiving the ocr results, we should expect the `ocr_indexed` to be
  // updated.
  EXPECT_EQ(storage_->GetImageStatus(test_directory_.AppendASCII("bar.jpg")),
            ImageStatus(time, 1, 0));

  // Simulates a new result with same annotation from ICA.
  ImageInfo bar_image_ica = bar_image;
  AnnotationInfo test_annotation_info;
  test_annotation_info.score = 0.1f;
  test_annotation_info.x = 0.2f;
  test_annotation_info.y = 0.3f;
  test_annotation_info.area = 0.4f;
  bar_image_ica.annotation_map["test"] = test_annotation_info;

  storage_->Insert(bar_image_ica, IndexingSource::kIca);

  // Both results are kept in db.
  EXPECT_THAT(storage_->GetAllAnnotationsForTest(),
              testing::ElementsAreArray({bar_image_ocr, bar_image_ica}));
  // When receiving the ica results, we should expect the `ica_indexed` to be
  // updated.
  EXPECT_EQ(storage_->GetImageStatus(test_directory_.AppendASCII("bar.jpg")),
            ImageStatus(time, 1, 1));
}

}  // namespace
}  // namespace app_list
