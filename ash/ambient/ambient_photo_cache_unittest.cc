// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_photo_cache.h"

#include "ash/ambient/ambient_constants.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

base::FilePath GetTestPath() {
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_TEMP, &path));
  path = path.Append(FILE_PATH_LITERAL(kAmbientModeDirectoryName));
  return path;
}

}  // namespace

class AmbientPhotoCacheTest : public testing::Test {
 public:
  void SetUp() override {
    auto test_path = GetTestPath();
    base::DeletePathRecursively(test_path);
    photo_cache_ = AmbientPhotoCache::Create(test_path);
  }

  void TearDown() override { base::DeletePathRecursively(GetTestPath()); }

  AmbientPhotoCache* photo_cache() { return photo_cache_.get(); }

 protected:
  base::test::TaskEnvironment task_environment_;

 private:
  std::unique_ptr<AmbientPhotoCache> photo_cache_;
};

TEST_F(AmbientPhotoCacheTest, ReadsBackWrittenFiles) {
  int cache_index = 0;
  std::string image("image");
  std::string details("details");
  std::string related_image("related image");

  {
    base::RunLoop loop;
    photo_cache()->WriteFiles(cache_index, &image, &details, &related_image,
                              loop.QuitClosure());
    loop.Run();
  }

  {
    base::RunLoop loop;
    // Read the files back using photo cache.
    photo_cache()->ReadFiles(
        cache_index,
        base::BindOnce(
            [](base::OnceClosure done, PhotoCacheEntry cache_read) {
              EXPECT_EQ(*cache_read.image, "image");
              EXPECT_EQ(*cache_read.details, "details");
              EXPECT_EQ(*cache_read.related_image, "related image");
              std::move(done).Run();
            },
            loop.QuitClosure()));
    loop.Run();
  }
}

TEST_F(AmbientPhotoCacheTest, WritesFileToDisk) {
  base::FilePath test_path = GetTestPath();

  int cache_index = 5;
  std::string image("image 5");
  std::string details("details 5");
  std::string related_image("related image 5");

  // Make sure files are not on disk.
  EXPECT_FALSE(base::PathExists(test_path.Append(FILE_PATH_LITERAL("5.img"))));
  EXPECT_FALSE(base::PathExists(test_path.Append(FILE_PATH_LITERAL("5.txt"))));
  EXPECT_FALSE(
      base::PathExists(test_path.Append(FILE_PATH_LITERAL("5_r.img"))));

  // Write the data to the cache.
  {
    base::RunLoop loop;
    photo_cache()->WriteFiles(cache_index, &image, &details, &related_image,
                              loop.QuitClosure());
    loop.Run();
  }

  // Verify that expected files are written to disk.
  std::string actual_image;
  EXPECT_TRUE(base::ReadFileToString(
      test_path.Append(FILE_PATH_LITERAL("5.img")), &actual_image));
  EXPECT_EQ(actual_image, image);

  std::string actual_details;
  EXPECT_TRUE(base::ReadFileToString(
      test_path.Append(FILE_PATH_LITERAL("5.txt")), &actual_details));
  EXPECT_EQ(actual_details, details);

  std::string actual_related_image;
  EXPECT_TRUE(base::ReadFileToString(
      test_path.Append(FILE_PATH_LITERAL("5_r.img")), &actual_related_image));
  EXPECT_EQ(actual_related_image, related_image);
}

TEST_F(AmbientPhotoCacheTest, SetsDataToEmptyStringWhenFilesMissing) {
  base::FilePath test_path = GetTestPath();
  EXPECT_FALSE(base::DirectoryExists(test_path));
  {
    base::RunLoop loop;
    photo_cache()->ReadFiles(
        /*cache_index=*/1,
        base::BindOnce(
            [](base::OnceClosure done, PhotoCacheEntry cache_read) {
              EXPECT_TRUE(cache_read.image->empty());
              EXPECT_TRUE(cache_read.details->empty());
              EXPECT_TRUE(cache_read.related_image->empty());
              std::move(done).Run();
            },
            loop.QuitClosure()));
  }
}

}  // namespace ash
