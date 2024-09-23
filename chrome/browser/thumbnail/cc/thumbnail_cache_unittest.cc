// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnail/cc/thumbnail_cache.h"

#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "cc/resources/ui_resource_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/ui_android_export.h"

namespace thumbnail {
namespace {

constexpr int kKiB = 1024;
constexpr int kN32PixelSize = 4;
constexpr int kDefaultCacheSize = 3;
constexpr int kCompressionQueueMaxSize = 2;
constexpr int kWriteQueueMaxSize = 2;

class MockUIResourceProvider : public ui::UIResourceProvider {
 public:
  MOCK_METHOD(cc::UIResourceId,
              CreateUIResource,
              (cc::UIResourceClient*),
              (override));
  MOCK_METHOD(void, DeleteUIResource, (cc::UIResourceId), (override));
  MOCK_METHOD(bool, SupportsETC1NonPowerOfTwo, (), (const, override));

  base::WeakPtr<UIResourceProvider> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockUIResourceProvider> weak_factory_{this};
};

}  // namespace

class ThumbnailCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    thumbnail_cache_ = std::make_unique<ThumbnailCache>(
        kDefaultCacheSize, kCompressionQueueMaxSize, kWriteQueueMaxSize,
        /*save_jpeg_thumbnails=*/true);
    thumbnail_cache_->SetUIResourceProvider(ui_resource_provider_.GetWeakPtr());

    EXPECT_CALL(ui_resource_provider_, CreateUIResource(::testing::_))
        .WillRepeatedly(::testing::Return(1));
  }

  void TearDown() override {}

  ThumbnailCache& thumbnail_cache() { return *thumbnail_cache_; }
  void RecordCacheMetrics() { thumbnail_cache_->RecordCacheMetrics(); }

  content::BrowserTaskEnvironment task_environment_;

 private:
  MockUIResourceProvider ui_resource_provider_;
  std::unique_ptr<ThumbnailCache> thumbnail_cache_;
};

// TODO(crbug.com/40885026): Tests are being added in the process of refactoring
// and optimizing the ThumbnailCache for modern usage add more tests here.

TEST_F(ThumbnailCacheTest, PruneCache) {
  constexpr int kTabId1 = 1;
  constexpr int kTabId2 = 2;
  constexpr int kDimension = 16;
  SkBitmap bitmap;
  ASSERT_TRUE(bitmap.tryAllocN32Pixels(kDimension * kKiB, kDimension));
  bitmap.setImmutable();

  thumbnail_cache().UpdateVisibleIds(std::vector<TabId>({kTabId1, kTabId2}),
                                     -1);
  EXPECT_TRUE(thumbnail_cache().CheckAndUpdateThumbnailMetaData(
      kTabId1, GURL("https://www.foo.com/"), /*force_update=*/false));
  std::unique_ptr<ThumbnailCaptureTracker, base::OnTaskRunnerDeleter> tracker1(
      new ThumbnailCaptureTracker(base::DoNothing()),
      base::OnTaskRunnerDeleter(
          base::SequencedTaskRunner::GetCurrentDefault()));
  thumbnail_cache().Put(kTabId1, std::move(tracker1), bitmap,
                        /*thumbnail_scale=*/1.0f);

  EXPECT_TRUE(thumbnail_cache().CheckAndUpdateThumbnailMetaData(
      kTabId2, GURL("https://www.bar.com/"), /*force_update=*/false));
  std::unique_ptr<ThumbnailCaptureTracker, base::OnTaskRunnerDeleter> tracker2(
      new ThumbnailCaptureTracker(base::DoNothing()),
      base::OnTaskRunnerDeleter(
          base::SequencedTaskRunner::GetCurrentDefault()));
  thumbnail_cache().Put(kTabId2, std::move(tracker2), bitmap,
                        /*thumbnail_scale=*/1.0f);

  EXPECT_TRUE(thumbnail_cache().Get(kTabId1, false));
  EXPECT_TRUE(thumbnail_cache().Get(kTabId2, false));

  thumbnail_cache().UpdateVisibleIds({kTabId1, kTabId2}, kTabId1);
  EXPECT_TRUE(thumbnail_cache().Get(kTabId1, false));
  EXPECT_TRUE(thumbnail_cache().Get(kTabId2, false));

  thumbnail_cache().UpdateVisibleIds({kTabId1, kTabId2}, -1);
  EXPECT_TRUE(thumbnail_cache().Get(kTabId1, false));
  EXPECT_TRUE(thumbnail_cache().Get(kTabId2, false));

  thumbnail_cache().UpdateVisibleIds({kTabId2}, kTabId1);
  EXPECT_FALSE(thumbnail_cache().Get(kTabId1, false));
  EXPECT_TRUE(thumbnail_cache().Get(kTabId2, false));

  thumbnail_cache().UpdateVisibleIds({kTabId1}, kTabId1);
  // The thumbnail will not be paged in yet although will be scheduled.
  EXPECT_FALSE(thumbnail_cache().Get(kTabId1, false));
  EXPECT_FALSE(thumbnail_cache().Get(kTabId2, false));
}

TEST_F(ThumbnailCacheTest, MetricsEmission) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount("Android.ThumbnailCache.InMemoryCacheEntries", 0);
  histograms.ExpectTotalCount("Android.ThumbnailCache.InMemoryCacheSize", 0);

  SkBitmap bitmap;
  constexpr int kTabId = 4;
  constexpr int kDimension = 4;
  ASSERT_TRUE(bitmap.tryAllocN32Pixels(kDimension * kKiB, kDimension));
  bitmap.setImmutable();
  thumbnail_cache().UpdateVisibleIds(std::vector<TabId>({kTabId}), -1);
  EXPECT_TRUE(thumbnail_cache().CheckAndUpdateThumbnailMetaData(
      kTabId, GURL("https://www.foo.com/"), /*force_update=*/false));
  std::unique_ptr<ThumbnailCaptureTracker, base::OnTaskRunnerDeleter> tracker(
      new ThumbnailCaptureTracker(base::DoNothing()),
      base::OnTaskRunnerDeleter(
          base::SequencedTaskRunner::GetCurrentDefault()));
  thumbnail_cache().Put(kTabId, std::move(tracker), bitmap,
                        /*thumbnail_scale=*/1.0f);
  RecordCacheMetrics();

  histograms.ExpectTotalCount("Android.ThumbnailCache.InMemoryCacheEntries", 1);
  histograms.ExpectTotalCount("Android.ThumbnailCache.InMemoryCacheSize", 1);
  histograms.ExpectUniqueSample("Android.ThumbnailCache.InMemoryCacheEntries",
                                1, 1);
  histograms.ExpectUniqueSample("Android.ThumbnailCache.InMemoryCacheSize",
                                kDimension * kDimension * kN32PixelSize, 1);
}

TEST_F(ThumbnailCacheTest, InvalidateIfChanged) {
  constexpr int kTabId1 = 1;
  constexpr int kDimension = 16;
  SkBitmap bitmap;
  ASSERT_TRUE(bitmap.tryAllocN32Pixels(kDimension * kKiB, kDimension));
  bitmap.setImmutable();

  thumbnail_cache().UpdateVisibleIds(std::vector<TabId>({kTabId1}), -1);
  GURL url("https://www.foo.com/");
  EXPECT_TRUE(thumbnail_cache().CheckAndUpdateThumbnailMetaData(
      kTabId1, url, /*force_update=*/false));
  std::unique_ptr<ThumbnailCaptureTracker, base::OnTaskRunnerDeleter> tracker1(
      new ThumbnailCaptureTracker(base::DoNothing()),
      base::OnTaskRunnerDeleter(
          base::SequencedTaskRunner::GetCurrentDefault()));
  thumbnail_cache().Put(kTabId1, std::move(tracker1), bitmap,
                        /*thumbnail_scale=*/1.0f);

  EXPECT_TRUE(thumbnail_cache().Get(kTabId1, false));

  thumbnail_cache().InvalidateThumbnailIfChanged(kTabId1, url);
  EXPECT_TRUE(thumbnail_cache().Get(kTabId1, false));

  thumbnail_cache().InvalidateThumbnailIfChanged(kTabId1, GURL());
  EXPECT_TRUE(thumbnail_cache().Get(kTabId1, false));

  thumbnail_cache().InvalidateThumbnailIfChanged(kTabId1,
                                                 GURL("https://www.bar.com/"));
  EXPECT_FALSE(thumbnail_cache().Get(kTabId1, false));
}

}  // namespace thumbnail
