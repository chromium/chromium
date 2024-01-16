// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/sync/crosapi_session_sync_favicon_delegate.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/favicon/core/history_ui_favicon_request_handler.h"
#include "components/favicon_base/favicon_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

constexpr char kTestUrl[] = "www.google.com";

class FakeHistoryUiFaviconRequestHandler
    : public favicon::HistoryUiFaviconRequestHandler {
 public:
  FakeHistoryUiFaviconRequestHandler() = default;
  ~FakeHistoryUiFaviconRequestHandler() override = default;

  void GetRawFaviconForPageURL(
      const GURL& page_url,
      int desired_size_in_pixel,
      favicon_base::FaviconRawBitmapCallback callback,
      favicon::HistoryUiFaviconRequestOrigin request_origin_for_uma) override {
    NOTIMPLEMENTED();
  }

  void GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::FaviconImageCallback callback,
      favicon::HistoryUiFaviconRequestOrigin request_origin_for_uma) override {
    favicon_base::FaviconImageResult result;
    if (result_image_) {
      result.image = *result_image_;
    }

    std::move(callback).Run(std::move(result));
  }

  void SetResultImage(gfx::Image* image) { result_image_ = image; }

 private:
  raw_ptr<gfx::Image> result_image_ = nullptr;
};

gfx::Image GetTestImage() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(gfx::kFaviconSize, gfx::kFaviconSize);
  bitmap.eraseColor(SK_ColorBLUE);
  return gfx::Image::CreateFrom1xBitmap(bitmap);
}

}  // namespace

class CrosapiSessionSyncFaviconDelegateTest : public testing::Test {
 public:
  void CreateFaviconDelegate(
      FakeHistoryUiFaviconRequestHandler* favicon_request_handler) {
    favicon_delegate_ = std::make_unique<CrosapiSessionSyncFaviconDelegate>(
        favicon_request_handler);
  }

  CrosapiSessionSyncFaviconDelegate* favicon_delegate() {
    return favicon_delegate_.get();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<CrosapiSessionSyncFaviconDelegate> favicon_delegate_;
};

TEST_F(CrosapiSessionSyncFaviconDelegateTest,
       GetFaviconImageForPageURL_NoHandler) {
  CreateFaviconDelegate(/*favicon_request_handler=*/nullptr);
  base::test::TestFuture<const gfx::ImageSkia&> future;
  favicon_delegate()->GetFaviconImageForPageURL(GURL(kTestUrl),
                                                future.GetCallback());
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(gfx::Image(), gfx::Image(future.Get())));
}

TEST_F(CrosapiSessionSyncFaviconDelegateTest,
       GetFaviconImageForPageURL_ImagesMatch) {
  FakeHistoryUiFaviconRequestHandler favicon_request_handler;
  CreateFaviconDelegate(&favicon_request_handler);

  gfx::Image expected_image = GetTestImage();
  favicon_request_handler.SetResultImage(&expected_image);

  base::test::TestFuture<const gfx::ImageSkia&> future;
  favicon_delegate()->GetFaviconImageForPageURL(GURL(kTestUrl),
                                                future.GetCallback());
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(expected_image, gfx::Image(future.Get())));
}
