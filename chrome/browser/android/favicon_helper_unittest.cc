// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/favicon_helper.h"

#include <vector>

#include "base/test/task_environment.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

using ::testing::_;
const int kDummyTaskId = 1;

favicon_base::FaviconRawBitmapResult
CreateTestBitmapResult(GURL url, int size, SkColor color = SK_ColorRED) {
  favicon_base::FaviconRawBitmapResult result;
  result.expired = false;

  // Create bitmap and fill with |color|.
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  bitmap.eraseColor(color);
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &data->data());

  result.bitmap_data = data;
  result.pixel_size = gfx::Size(size, size);
  result.icon_url = url;
  result.icon_type = favicon_base::IconType::kFavicon;
  CHECK(result.is_valid());
  return result;
}

class FaviconHelperTest : public testing::Test {
 public:
  FaviconHelperTest() {
    ON_CALL(mock_favicon_service_, GetRawFaviconForPageURL(_, _, _, _, _, _))
        .WillByDefault([](const GURL& url, auto, int size, auto,
                          favicon_base::FaviconRawBitmapCallback callback,
                          auto) {
          std::move(callback).Run(CreateTestBitmapResult(url, size));
          return kDummyTaskId;
        });

    get_composed_favicon_raw_results_callback_ = base::BindOnce(
        &FaviconHelperTest::onNativeGetComposedFaviconImageFinished,
        base::Unretained(this));
  }

  ~FaviconHelperTest() override { favicon_helper_->Destroy(nullptr); }

  void onNativeGetComposedFaviconImageFinished(
      const std::vector<favicon_base::FaviconRawBitmapResult>& result) {
    raw_bitmap_results_ = result;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  favicon::MockFaviconService mock_favicon_service_;
  FaviconHelper* favicon_helper_ = new FaviconHelper();
  std::vector<favicon_base::FaviconRawBitmapResult> raw_bitmap_results_;
  favicon_base::FaviconResultsCallback
      get_composed_favicon_raw_results_callback_;
};

TEST_F(FaviconHelperTest, GetLargestSizeIndex) {
  std::vector<gfx::Size> sizes;
  gfx::Size size1 = gfx::Size(INT_MAX, INT_MAX);
  sizes.push_back(size1);
  gfx::Size size2 = gfx::Size(16, 16);
  sizes.push_back(size2);
  gfx::Size size3 = gfx::Size(32, 32);
  sizes.push_back(size3);
  EXPECT_EQ(2u, FaviconHelper::GetLargestSizeIndex(sizes));
  sizes.clear();
  sizes.push_back(size1);
  sizes.push_back(size1);
  EXPECT_EQ(0u, FaviconHelper::GetLargestSizeIndex(sizes));
}

TEST_F(FaviconHelperTest, GetComposedFaviconImage) {
  raw_bitmap_results_.clear();
  std::vector<std::string> urls = {"http://www.tab1.com",
                                   "http://www.tab2.com"};
  GURL url1 = GURL(urls[0]);
  GURL url2 = GURL(urls[1]);

  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(url1, _, _, 16, _, _))
      .Times(1);
  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(url2, _, _, 16, _, _))
      .Times(1);

  favicon_helper_->GetComposedFaviconImageInternal(
      &mock_favicon_service_, urls, 16,
      std::move(get_composed_favicon_raw_results_callback_));

  task_environment_.RunUntilIdle();

  EXPECT_EQ(2, (int)raw_bitmap_results_.size());
}

TEST_F(FaviconHelperTest, GetComposedFaviconImageWithOneFaviconFailed) {
  raw_bitmap_results_.clear();
  std::vector<std::string> urls = {"http://www.tab1.com", "http://www.tab2.com",
                                   "http://www.tab3.com"};
  GURL url1 = GURL(urls[0]);
  GURL url2 = GURL(urls[1]);
  GURL url3 = GURL(urls[2]);

  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(url1, _, _, 16, _, _))
      .Times(1);
  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(url2, _, _, 16, _, _))
      .Times(1);
  // With one favicon failed.
  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(url3, _, _, 16, _, _))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::FaviconRawBitmapCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
        return kDummyTaskId;
      });

  favicon_helper_->GetComposedFaviconImageInternal(
      &mock_favicon_service_, urls, 16,
      std::move(get_composed_favicon_raw_results_callback_));

  task_environment_.RunUntilIdle();

  EXPECT_EQ(2, (int)raw_bitmap_results_.size());
}

TEST_F(FaviconHelperTest, GetComposedFaviconImageOrderMatchesInput) {
  raw_bitmap_results_.clear();
  std::vector<std::string> urls = {"http://www.tab1.com", "http://www.tab2.com",
                                   "http://www.tab3.com"};
  GURL url1 = GURL(urls[0]);
  GURL url2 = GURL(urls[1]);
  GURL url3 = GURL(urls[2]);

  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(url1, _, _, 16, _, _))
      .Times(1);

  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(url2, _, _, 16, _, _))
      .WillOnce([](const GURL& url, auto, int size, auto,
                   favicon_base::FaviconRawBitmapCallback callback,
                   base::CancelableTaskTracker* tracker) {
        tracker->PostTask(
            base::ThreadTaskRunnerHandle::Get().get(), FROM_HERE,
            base::BindOnce(
                [](favicon_base::FaviconRawBitmapCallback callback, GURL url,
                   int size) {
                  sleep(5);
                  std::move(callback).Run(CreateTestBitmapResult(url, size));
                },
                std::move(callback), url, size));
        return kDummyTaskId;
      });

  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(url3, _, _, 16, _, _))
      .Times(1);

  favicon_helper_->GetComposedFaviconImageInternal(
      &mock_favicon_service_, urls, 16,
      std::move(get_composed_favicon_raw_results_callback_));

  task_environment_.RunUntilIdle();

  EXPECT_EQ(3, (int)raw_bitmap_results_.size());
  EXPECT_EQ(url1, raw_bitmap_results_[0].icon_url);
  EXPECT_EQ(url2, raw_bitmap_results_[1].icon_url);
  EXPECT_EQ(url3, raw_bitmap_results_[2].icon_url);
}
