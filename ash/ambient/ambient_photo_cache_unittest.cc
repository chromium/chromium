// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_photo_cache.h"

#include <fstream>
#include <iostream>

#include "ash/ambient/ambient_constants.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "base/bind.h"
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
  std::string related_details("related details");
  bool is_portrait = true;

  {
    ambient::PhotoCacheEntry cache;
    cache.mutable_primary_photo()->set_image(image);
    cache.mutable_primary_photo()->set_details(details);
    cache.mutable_primary_photo()->set_is_portrait(is_portrait);
    cache.mutable_related_photo()->set_image(related_image);
    cache.mutable_related_photo()->set_details(related_details);
    cache.mutable_related_photo()->set_is_portrait(is_portrait);

    base::RunLoop loop;
    photo_cache()->WritePhotoCache(cache_index, cache, loop.QuitClosure());
    loop.Run();
  }

  {
    base::RunLoop loop;
    // Read the files back using photo cache.
    ambient::PhotoCacheEntry cache_read;
    photo_cache()->ReadPhotoCache(
        cache_index, &cache_read,
        base::BindOnce([](base::OnceClosure done) { std::move(done).Run(); },
                       loop.QuitClosure()));
    loop.Run();

    EXPECT_EQ(cache_read.primary_photo().image(), "image");
    EXPECT_EQ(cache_read.primary_photo().details(), "details");
    EXPECT_TRUE(cache_read.primary_photo().is_portrait());
    EXPECT_EQ(cache_read.related_photo().image(), "related image");
    EXPECT_EQ(cache_read.related_photo().details(), "related details");
    EXPECT_TRUE(cache_read.related_photo().is_portrait());
  }
}

TEST_F(AmbientPhotoCacheTest, SetsDataToEmptyStringWhenFilesMissing) {
  base::FilePath test_path = GetTestPath();
  EXPECT_FALSE(base::DirectoryExists(test_path));
  {
    base::RunLoop loop;
    ambient::PhotoCacheEntry cache_read;
    photo_cache()->ReadPhotoCache(
        /*cache_index=*/1, &cache_read,
        base::BindOnce([](base::OnceClosure done) { std::move(done).Run(); },
                       loop.QuitClosure()));
    loop.Run();

    EXPECT_TRUE(cache_read.primary_photo().image().empty());
    EXPECT_TRUE(cache_read.primary_photo().details().empty());
    EXPECT_FALSE(cache_read.primary_photo().is_portrait());
    EXPECT_TRUE(cache_read.related_photo().image().empty());
    EXPECT_TRUE(cache_read.related_photo().details().empty());
    EXPECT_FALSE(cache_read.related_photo().is_portrait());
  }
}

}  // namespace ash
