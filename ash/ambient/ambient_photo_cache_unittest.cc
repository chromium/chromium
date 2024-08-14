// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_photo_cache.h"

#include <fstream>
#include <iostream>

#include "ash/ambient/ambient_access_token_controller.h"
#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_photo_cache_settings.h"
#include "ash/ambient/test/ambient_ash_test_helper.h"
#include "ash/ambient/test/test_ambient_client.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
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
  AmbientPhotoCacheTest() {
    ambient_photo_cache::SetFileTaskRunner(
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  }

  void SetUp() override {
    ambient_ash_test_helper_ = std::make_unique<AmbientAshTestHelper>();
    access_token_controller_ = std::make_unique<AmbientAccessTokenController>();

    auto test_path = GetTestPath();
    base::DeletePathRecursively(test_path);
    SetAmbientPhotoCacheRootDirForTesting(test_path);
  }

  void TearDown() override { base::DeletePathRecursively(GetTestPath()); }

  network::TestURLLoaderFactory& test_url_loader_factory() {
    return ambient_ash_test_helper_->ambient_client().test_url_loader_factory();
  }

  AmbientAccessTokenController& access_token_controller() {
    return *access_token_controller_;
  }

  bool IsAccessTokenRequestPending() {
    return ambient_ash_test_helper_->ambient_client()
        .IsAccessTokenRequestPending();
  }

  void IssueAccessToken() {
    ambient_ash_test_helper_->ambient_client().IssueAccessToken(
        /*is_empty=*/false);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<AmbientAshTestHelper> ambient_ash_test_helper_;
  std::unique_ptr<AmbientAccessTokenController> access_token_controller_;
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
    ambient_photo_cache::WritePhotoCache(ambient_photo_cache::Store::kPrimary,
                                         cache_index, cache,
                                         loop.QuitClosure());
    loop.Run();
  }

  {
    base::RunLoop loop;
    // Read the files back using photo cache.
    ambient::PhotoCacheEntry cache_read;
    ambient_photo_cache::ReadPhotoCache(
        ambient_photo_cache::Store::kPrimary, cache_index,
        base::BindLambdaForTesting(
            [&cache_read, &loop](ambient::PhotoCacheEntry cache_entry_in) {
              cache_read = std::move(cache_entry_in);
              loop.Quit();
            }));
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
    ambient_photo_cache::ReadPhotoCache(
        ambient_photo_cache::Store::kPrimary,
        /*cache_index=*/1,
        base::BindLambdaForTesting(
            [&cache_read, &loop](ambient::PhotoCacheEntry cache_entry_in) {
              cache_read = std::move(cache_entry_in);
              loop.Quit();
            }));
    loop.Run();

    EXPECT_TRUE(cache_read.primary_photo().image().empty());
    EXPECT_TRUE(cache_read.primary_photo().details().empty());
    EXPECT_FALSE(cache_read.primary_photo().is_portrait());
    EXPECT_TRUE(cache_read.related_photo().image().empty());
    EXPECT_TRUE(cache_read.related_photo().details().empty());
    EXPECT_FALSE(cache_read.related_photo().is_portrait());
  }
}

TEST_F(AmbientPhotoCacheTest, AttachTokenToDownloadRequest) {
  std::string fake_url = "https://faketesturl/";

  ambient_photo_cache::DownloadPhoto(fake_url, access_token_controller(),
                                     base::BindOnce([](std::string&&) {}));
  RunUntilIdle();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken();
  EXPECT_FALSE(IsAccessTokenRequestPending());

  auto* pending_requests = test_url_loader_factory().pending_requests();
  EXPECT_EQ(pending_requests->size(), std::size_t{1});
  EXPECT_EQ(pending_requests->at(0).request.url, fake_url);

  EXPECT_EQ(pending_requests->at(0).request.headers.GetHeader("Authorization"),
            std::string("Bearer ") + TestAmbientClient::kTestAccessToken);
}

TEST_F(AmbientPhotoCacheTest, AttachTokenToDownloadToTempFileRequest) {
  std::string fake_url = "https://faketesturl/";

  ambient_photo_cache::DownloadPhotoToTempFile(
      fake_url, access_token_controller(),
      base::BindOnce([](base::FilePath) {}));

  RunUntilIdle();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken();
  EXPECT_FALSE(IsAccessTokenRequestPending());

  auto* pending_requests = test_url_loader_factory().pending_requests();
  EXPECT_EQ(pending_requests->size(), std::size_t{1});
  EXPECT_EQ(pending_requests->at(0).request.url, fake_url);

  EXPECT_EQ(pending_requests->at(0).request.headers.GetHeader("Authorization"),
            std::string("Bearer ") + TestAmbientClient::kTestAccessToken);
}

}  // namespace ash
