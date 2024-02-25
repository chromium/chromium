// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/almanac_api_client/almanac_icon_cache.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/resources/grit/ui_resources.h"

namespace apps {
namespace {

using testing::_;

ACTION_P(PostFetchReply, p0) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(*arg2), p0, image_fetcher::RequestMetadata()));
}

class TestAlmanacIconCache : public AlmanacIconCache {
 public:
  TestAlmanacIconCache()
      : mock_image_fetcher_(
            std::make_unique<image_fetcher::MockImageFetcher>()) {}
  ~TestAlmanacIconCache() override = default;

  image_fetcher::MockImageFetcher* mock_image_fetcher() const {
    return mock_image_fetcher_.get();
  }

 private:
  image_fetcher::ImageFetcher* GetImageFetcher() override {
    return mock_image_fetcher_.get();
  }
  gfx::Image image_override_;

  std::unique_ptr<image_fetcher::MockImageFetcher> mock_image_fetcher_;
};

class AlmanacIconCacheTest : public testing::Test {
 public:
  AlmanacIconCacheTest()
      : almanac_image_fetcher_(std::make_unique<TestAlmanacIconCache>()) {}

  gfx::Image& GetTestImage(int resource_id) {
    return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
        resource_id);
  }

  image_fetcher::MockImageFetcher* mock_image_fetcher() {
    return almanac_image_fetcher_->mock_image_fetcher();
  }

  TestAlmanacIconCache* almanac_image_fetcher() {
    return almanac_image_fetcher_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestAlmanacIconCache> almanac_image_fetcher_;
};

TEST_F(AlmanacIconCacheTest, DownloadIconSuccess) {
  GURL fake_url = GURL("https://www.example.com/fake_image");
  gfx::Image expected_image = GetTestImage(IDR_DEFAULT_FAVICON);
  base::test::TestFuture<const gfx::Image&> received_image;

  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(fake_url, _, _, _))
      .WillOnce(PostFetchReply(expected_image));
  almanac_image_fetcher()->GetIcon(fake_url, received_image.GetCallback());

  EXPECT_TRUE(gfx::test::AreImagesEqual(expected_image, received_image.Get()));
}

TEST_F(AlmanacIconCacheTest, DownloadIconServerFailure) {
  GURL fake_url = GURL("https://www.example.com/fake_image");
  base::test::TestFuture<const gfx::Image&> received_image;

  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(fake_url, _, _, _))
      .WillOnce(PostFetchReply(gfx::Image()));
  almanac_image_fetcher()->GetIcon(fake_url, received_image.GetCallback());

  EXPECT_TRUE(received_image.Get().IsEmpty());
}
}  // namespace
}  // namespace apps
