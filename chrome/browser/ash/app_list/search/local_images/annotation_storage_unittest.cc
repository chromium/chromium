// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/app_list/search/local_images/annotation_storage.h"
#include "chrome/browser/ash/app_list/search/local_images/image_annotation_worker.h"
#include "chrome/browser/ash/app_list/search/local_images/local_image_search_test_util.h"
#include "chrome/browser/ash/app_list/search/local_images/sql_database.h"
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
  db->GetStatementForQuery(SQL_FROM_HERE, kQuery).Run();
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
    storage_ = std::make_unique<AnnotationStorage>(
        std::move(test_db), /*histogram_tag=*/"test",
        /*annotation_worker=*/nullptr);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<AnnotationStorage> storage_;
  base::FilePath test_directory_;
};

TEST_F(AnnotationStorageTest, EmptyStorage) {
  storage_->Initialize();
  task_environment_.RunUntilIdle();

  auto annotations = storage_->GetAllAnnotations();
  EXPECT_TRUE(annotations.empty());

  task_environment_.RunUntilIdle();
}

TEST_F(AnnotationStorageTest, InsertOrReplace) {
  storage_->Initialize();
  task_environment_.RunUntilIdle();

  ImageInfo bar_image({"test"}, test_directory_.AppendASCII("bar.jpg"),
                      base::Time::Now(), /*is_ignored=*/false);

  storage_->Insert(bar_image);

  EXPECT_THAT(storage_->GetAllAnnotations(),
              testing::ElementsAreArray({bar_image}));
  task_environment_.RunUntilIdle();

  ImageInfo foo_image({"test1"}, test_directory_.AppendASCII("foo.png"),
                      base::Time::Now(), /*is_ignored=*/false);

  storage_->Insert(foo_image);

  EXPECT_THAT(storage_->GetAllAnnotations(),
              testing::UnorderedElementsAreArray({bar_image, foo_image}));
  task_environment_.RunUntilIdle();
}

TEST_F(AnnotationStorageTest, Remove) {
  storage_->Initialize();
  task_environment_.RunUntilIdle();

  ImageInfo bar_image({"test"}, test_directory_.AppendASCII("bar.jpg"),
                      base::Time::Now(), /*is_ignored=*/false);
  ImageInfo foo_image({"test1"}, test_directory_.AppendASCII("foo.png"),
                      base::Time::Now(), /*is_ignored=*/false);
  storage_->Insert(bar_image);
  storage_->Insert(foo_image);

  storage_->Remove(test_directory_.AppendASCII("bar.jpg"));

  EXPECT_THAT(storage_->GetAllAnnotations(),
              testing::ElementsAreArray({foo_image}));

  storage_->Remove(test_directory_.AppendASCII("bar.jpg"));

  EXPECT_THAT(storage_->GetAllAnnotations(),
              testing::ElementsAreArray({foo_image}));

  storage_->Remove(test_directory_.AppendASCII("foo.png"));
  EXPECT_TRUE(storage_->GetAllAnnotations().empty());

  storage_->Remove(test_directory_.AppendASCII("foo.png"));
  EXPECT_TRUE(storage_->GetAllAnnotations().empty());

  task_environment_.RunUntilIdle();
}

TEST_F(AnnotationStorageTest, FindImagePath) {
  storage_->Initialize();
  task_environment_.RunUntilIdle();

  ImageInfo bar_image({"test"}, test_directory_.AppendASCII("bar.jpg"),
                      base::Time::Now(), /*is_ignored=*/false);
  ImageInfo foo_image({"test1"}, test_directory_.AppendASCII("foo.png"),
                      base::Time::Now(), /*is_ignored=*/false);
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

TEST_F(AnnotationStorageTest, LinearSearchAnnotations) {
  storage_->Initialize();
  task_environment_.RunUntilIdle();

  ImageInfo bar_image({"test", "bar"}, test_directory_.AppendASCII("bar.jpg"),
                      base::Time::Now(), /*is_ignored=*/false);
  ImageInfo foo_image({"test1"}, test_directory_.AppendASCII("foo.png"),
                      base::Time::Now(), /*is_ignored=*/false);
  ImageInfo ignore_image({"test2"}, test_directory_.AppendASCII("remove.png"),
                         base::Time::Now(), /*is_ignored=*/true);
  storage_->Insert(bar_image);
  storage_->Insert(foo_image);
  storage_->Insert(ignore_image);

  auto images =
      storage_->LinearSearchAnnotations(base::UTF8ToUTF16(std::string("test")));

  EXPECT_THAT(
      images,
      testing::UnorderedElementsAre(
          testing::Pair(bar_image.path,
                        FileSearchResult(bar_image.last_modified, 1)),
          testing::Pair(foo_image.path,
                        FileSearchResult(foo_image.last_modified, 0.909375))));

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

  sql_database->Initialize();
  task_environment_.RunUntilIdle();
  sql_database->Close();

  storage_ = std::make_unique<AnnotationStorage>(
      test_directory_.AppendASCII("test.db"), /*histogram_tag=*/"test",
      /*annotation_worker=*/nullptr);
  storage_->Initialize();
  task_environment_.RunUntilIdle();

  ImageInfo bar_image({"test"}, test_directory_.AppendASCII("bar.jpg"),
                      base::Time::Now(), /*is_ignored=*/false);

  storage_->Insert(bar_image);
  EXPECT_THAT(storage_->GetAllAnnotations(),
              testing::ElementsAreArray({bar_image}));
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace app_list
