// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/app_list/search/local_images/annotation_storage.h"
#include "chrome/browser/ash/app_list/search/local_images/image_annotation_worker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

class AnnotationStorageTest : public testing::Test {
 protected:
  // testing::Test overrides:
  void SetUp() override {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    test_directory_ = temp_dir.GetPath();
    base::FilePath test_db = test_directory_.AppendASCII("test.db");
    storage_ = base::MakeRefCounted<AnnotationStorage>(
        std::move(test_db), /*histogram_tag=*/"test",
        /*current_version_number=*/2, /*annotation_worker=*/nullptr);
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<AnnotationStorage> storage_;
  base::FilePath test_directory_;
};

bool Matcher(std::vector<ImageInfo> arg,
             std::vector<ImageInfo> expected_images) {
  for (const auto& expect_image : expected_images) {
    for (const auto& test_image : arg) {
      if (test_image.path == expect_image.path &&
          test_image.annotations == expect_image.annotations &&
          test_image.last_modified == expect_image.last_modified) {
        return true;
      }
    }
  }
  return false;
}

MATCHER_P(OneOfImages, image1, "") {
  return Matcher(arg, {image1});
}

MATCHER_P2(OneOfImages, image1, image2, "") {
  return Matcher(arg, {image1, image2});
}

MATCHER_P2(OneOfFileSearchResult, result1, result2, "") {
  for (const auto& expect_result : {result1, result2}) {
    for (const auto& test_result : arg) {
      if (test_result.path == expect_result.path &&
          test_result.last_modified == expect_result.last_modified &&
          std::abs(test_result.relevance - expect_result.relevance) <
              0.0000001) {
        return true;
      }
    }
  }
  return false;
}

TEST_F(AnnotationStorageTest, EmptyStorage) {
  storage_->InitializeAsync();
  task_environment_.RunUntilIdle();

  auto expect_empty = base::BindLambdaForTesting(
      [](std::vector<ImageInfo> images) { ASSERT_EQ(images.size(), 0u); });

  storage_->GetAllAnnotationsAsync(expect_empty);

  task_environment_.RunUntilIdle();
}

TEST_F(AnnotationStorageTest, InsertOrReplaceAsync) {
  storage_->InitializeAsync();
  task_environment_.RunUntilIdle();

  ImageInfo bar_image({"test"}, test_directory_.AppendASCII("bar.jpg"),
                      base::Time::Now());
  auto expect_one =
      base::BindLambdaForTesting([=](std::vector<ImageInfo> images) {
        EXPECT_THAT(images, OneOfImages(bar_image));
      });

  storage_->InsertOrReplaceAsync(bar_image);

  storage_->GetAllAnnotationsAsync(expect_one);
  task_environment_.RunUntilIdle();

  ImageInfo foo_image({"test1"}, test_directory_.AppendASCII("foo.png"),
                      base::Time::Now());
  auto expect_two =
      base::BindLambdaForTesting([=](std::vector<ImageInfo> images) {
        EXPECT_THAT(images, OneOfImages(bar_image, foo_image));
      });

  storage_->InsertOrReplaceAsync(foo_image);

  storage_->GetAllAnnotationsAsync(expect_two);
  task_environment_.RunUntilIdle();
}

TEST_F(AnnotationStorageTest, RemoveAsync) {
  storage_->InitializeAsync();
  task_environment_.RunUntilIdle();

  ImageInfo bar_image({"test"}, test_directory_.AppendASCII("bar.jpg"),
                      base::Time::Now());
  ImageInfo foo_image({"test1"}, test_directory_.AppendASCII("foo.png"),
                      base::Time::Now());
  storage_->InsertOrReplaceAsync(bar_image);
  storage_->InsertOrReplaceAsync(foo_image);

  storage_->RemoveAsync(test_directory_.AppendASCII("bar.jpg"));

  auto expect_callback =
      base::BindLambdaForTesting([=](std::vector<ImageInfo> images) {
        EXPECT_THAT(images, OneOfImages(foo_image));
      });
  storage_->GetAllAnnotationsAsync(expect_callback);

  storage_->RemoveAsync(test_directory_.AppendASCII("bar.jpg"));
  storage_->GetAllAnnotationsAsync(expect_callback);

  auto expect_empty = base::BindLambdaForTesting(
      [](std::vector<ImageInfo> images) { ASSERT_EQ(images.size(), 0u); });
  storage_->RemoveAsync(test_directory_.AppendASCII("foo.png"));
  storage_->GetAllAnnotationsAsync(expect_empty);

  storage_->RemoveAsync(test_directory_.AppendASCII("foo.png"));
  storage_->GetAllAnnotationsAsync(expect_empty);

  task_environment_.RunUntilIdle();
}

TEST_F(AnnotationStorageTest, FindImagePathAsync) {
  storage_->InitializeAsync();
  task_environment_.RunUntilIdle();

  ImageInfo bar_image({"test"}, test_directory_.AppendASCII("bar.jpg"),
                      base::Time::Now());
  ImageInfo foo_image({"test1"}, test_directory_.AppendASCII("foo.png"),
                      base::Time::Now());
  storage_->InsertOrReplaceAsync(bar_image);
  storage_->InsertOrReplaceAsync(foo_image);

  auto expect_bar =
      base::BindLambdaForTesting([=](std::vector<ImageInfo> images) {
        EXPECT_THAT(images, OneOfImages(bar_image));
      });
  storage_->FindImagePathAsync(test_directory_.AppendASCII("bar.jpg"),
                               expect_bar);

  auto expect_foo =
      base::BindLambdaForTesting([=](std::vector<ImageInfo> images) {
        EXPECT_THAT(images, OneOfImages(foo_image));
      });
  storage_->FindImagePathAsync(test_directory_.AppendASCII("foo.png"),
                               expect_foo);

  task_environment_.RunUntilIdle();
}

TEST_F(AnnotationStorageTest, LinearSearchAnnotationsAsync) {
  storage_->InitializeAsync();
  task_environment_.RunUntilIdle();

  ImageInfo bar_image({"test", "bar"}, test_directory_.AppendASCII("bar.jpg"),
                      base::Time::Now());
  ImageInfo foo_image({"test1"}, test_directory_.AppendASCII("foo.png"),
                      base::Time::Now());
  storage_->InsertOrReplaceAsync(bar_image);
  storage_->InsertOrReplaceAsync(foo_image);

  auto expect =
      base::BindLambdaForTesting([=](std::vector<FileSearchResult> images) {
        EXPECT_THAT(
            images,
            OneOfFileSearchResult(
                FileSearchResult(bar_image.path, bar_image.last_modified, 1),
                FileSearchResult(foo_image.path, foo_image.last_modified,
                                 0.909375)));
      });
  storage_->LinearSearchAnnotationsAsync(base::UTF8ToUTF16(std::string("test")),
                                         expect);

  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace app_list
