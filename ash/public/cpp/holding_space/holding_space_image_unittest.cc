// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_image.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

namespace {

SkBitmap CreateBitmap(int width, int height, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return bitmap;
}

gfx::ImageSkia CreateImageSkia(int width, int height, SkColor color) {
  return gfx::ImageSkia::CreateFrom1xBitmap(CreateBitmap(width, height, color));
}

// Helper class that provides a test implementation for the async bitmap
// resolver callback used to generate holding space image representations.
class ImageGenerator {
 public:
  ImageGenerator() = default;
  ImageGenerator(const ImageGenerator&) = delete;
  ImageGenerator& operator=(const ImageGenerator&) = delete;
  ~ImageGenerator() = default;

  HoldingSpaceImage::AsyncBitmapResolver CreateResolverCallback() {
    return base::BindRepeating(&ImageGenerator::GenerateImage,
                               weak_factory_.GetWeakPtr());
  }

  void GenerateImage(const gfx::Size& size,
                     float scale,
                     HoldingSpaceImage::BitmapCallback callback) {
    auto request = std::make_unique<PendingRequest>();
    request->size = size;
    request->callback = std::move(callback);
    pending_requests_.push_back(std::move(request));
  }

  size_t NumberOfPendingRequests() const { return pending_requests_.size(); }

  void FulfillRequest(size_t index, SkColor color) {
    ASSERT_LT(index, pending_requests_.size());

    auto it = pending_requests_.begin() + index;
    SkBitmap result =
        CreateBitmap((*it)->size.width(), (*it)->size.height(), color);
    HoldingSpaceImage::BitmapCallback callback = std::move((*it)->callback);

    pending_requests_.erase(it);

    std::move(callback).Run(&result);
  }

  void FailRequest(size_t index) {
    ASSERT_LT(index, pending_requests_.size());

    auto it = pending_requests_.begin() + index;
    HoldingSpaceImage::BitmapCallback callback = std::move((*it)->callback);
    pending_requests_.erase(it);

    std::move(callback).Run(nullptr);
  }

 private:
  struct PendingRequest {
    gfx::Size size;
    HoldingSpaceImage::BitmapCallback callback;
  };

  std::vector<std::unique_ptr<PendingRequest>> pending_requests_;

  base::WeakPtrFactory<ImageGenerator> weak_factory_{this};
};

// Helper class that keeps track of how many times an holding space image has
// been updated, and requests an image representation after each image update.
class TestImageClient {
 public:
  explicit TestImageClient(const HoldingSpaceImage* image) : image_(image) {
    image_subscription_ = image_->AddImageSkiaChangedCallback(
        base::BindRepeating(&TestImageClient::OnHoldingSpaceItemImageChanged,
                            base::Unretained(this)));
    image_->image_skia().GetRepresentation(1.0f);
  }

  void OnHoldingSpaceItemImageChanged() {
    image_->image_skia().GetRepresentation(1.0f);
    ++image_change_count_;
  }

  size_t GetAndResetImageChangeCount() {
    size_t result = image_change_count_;
    image_change_count_ = 0;
    return result;
  }

 private:
  const HoldingSpaceImage* const image_;
  base::CallbackListSubscription image_subscription_;
  size_t image_change_count_ = 0;
};

std::unique_ptr<HoldingSpaceItem> CreateTestItem(
    ImageGenerator* image_generator,
    int image_size,
    SkColor image_color) {
  const base::FilePath file_path("file_path");
  const GURL file_system_url("filesystem:file_system_url");
  gfx::ImageSkia placeholder(
      CreateImageSkia(image_size, image_size, image_color));
  placeholder.AddRepresentation(gfx::ImageSkiaRep(
      CreateBitmap(image_size * 2, image_size * 2, image_color), 2));

  return HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kPinnedFile, file_path, file_system_url,
      std::make_unique<HoldingSpaceImage>(
          placeholder, image_generator->CreateResolverCallback()));
}

}  // namespace

// Tests the basic flow for generating holding space image bitmaps.
TEST(HoldingSpaceImageTest, ImageGeneration) {
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item = CreateTestItem(
      &image_generator, /*image_size=*/10, /*image_color=*/SK_ColorRED);

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());

  // The test client implementation requests an image on construction.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  // The image should return the placeholder bitmap.
  gfx::ImageSkia image = holding_space_item->image().image_skia();
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorRED, image.bitmap()->getColor(5, 5));

  // Generate the holding space item image, and verify the icon has been
  // updated.
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  image_generator.FulfillRequest(0, SK_ColorBLUE);
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  image = holding_space_item->image().image_skia();
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
}

// Tests the basic flow for generating holding space image bitmaps where 2x
// bitmap gets requested.
TEST(HoldingSpaceImageTest, ImageGenerationWith2xScale) {
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item = CreateTestItem(
      &image_generator, /*image_size=*/10, /*image_color=*/SK_ColorRED);

  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  image_generator.FulfillRequest(0, SK_ColorBLUE);
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  gfx::ImageSkia image = holding_space_item->image().image_skia();
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  // Request 2x bitmap.
  const SkBitmap bitmap_2x = image.GetRepresentation(2.0f).GetBitmap();
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  // Use placeholder while the image request is in progress.
  EXPECT_EQ(20, bitmap_2x.height());
  EXPECT_EQ(20, bitmap_2x.width());
  EXPECT_EQ(SK_ColorRED, bitmap_2x.getColor(5, 5));

  // Verify that the image gets updated once the holding space image is
  // generated.
  image_generator.FulfillRequest(0, SK_ColorBLUE);

  image = holding_space_item->image().image_skia();
  const SkBitmap loaded_bitmap_2x = image.GetRepresentation(2.0f).GetBitmap();
  EXPECT_EQ(20, loaded_bitmap_2x.height());
  EXPECT_EQ(20, loaded_bitmap_2x.width());
  EXPECT_EQ(SK_ColorBLUE, loaded_bitmap_2x.getColor(5, 5));
}

// Verifies that the holding space image handles failed holding space image
// requests.
TEST(HoldingSpaceImageTest, ImageLoadFailure) {
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item = CreateTestItem(
      &image_generator, /*image_size=*/10, /*image_color=*/SK_ColorRED);

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());

  // Test image client requests an image representation durion construction.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  // Use placeholder while the image request is in progress.
  gfx::ImageSkia image = holding_space_item->image().image_skia();
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorRED, image.bitmap()->getColor(5, 5));

  // Simulate failed holding space item image request, and verify the icon keeps
  // using the placeholder image.
  image_generator.FailRequest(0);

  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
  image = holding_space_item->image().image_skia();
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorRED, image.bitmap()->getColor(5, 5));
}

// Verifies that the holding space image can be updated using
// `HoldingSpaceItem::InvalidateImage()`.
TEST(HoldingSpaceImageTest, ImageRefresh) {
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item = CreateTestItem(
      &image_generator, /*image_size=*/10, /*image_color=*/SK_ColorRED);

  // Finish loading the initial image.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  image_generator.FulfillRequest(0, SK_ColorBLUE);
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  gfx::ImageSkia image = holding_space_item->image().image_skia();
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  // Request image refresh, and verify another image gets requested.
  holding_space_item->InvalidateImage();

  // While image load request is in progress, use the previously loaded icon.
  image = holding_space_item->image().image_skia();
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  // Verify that image gets updated once the image load request completes.
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  image_generator.FulfillRequest(0, SK_ColorGREEN);
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  image = holding_space_item->image().image_skia();
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorGREEN, image.bitmap()->getColor(5, 5));

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
}

// Verifies that holding space image can be refreshed while the initial image
// load is in progress.
TEST(HoldingSpaceImageTest, ImageRefreshDuringInitialLoad) {
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item = CreateTestItem(
      &image_generator, /*image_size=*/10, /*image_color=*/SK_ColorRED);

  // The test client implementation requests an image on construction.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  // Use placeholder while the image load is in progress.
  gfx::ImageSkia image = holding_space_item->image().image_skia();
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorRED, image.bitmap()->getColor(5, 5));

  holding_space_item->InvalidateImage();

  // Verify that placeholder image remains to be used.
  image = holding_space_item->image().image_skia();
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorRED, image.bitmap()->getColor(5, 5));

  EXPECT_EQ(2u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  // Fulfill the initial request - the load result should be ignored.
  image_generator.FulfillRequest(0, SK_ColorBLUE);
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  image = holding_space_item->image().image_skia();
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorRED, image.bitmap()->getColor(5, 5));

  // Fulfill the later request, and verify the icon gets updated.
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  image_generator.FulfillRequest(0, SK_ColorGREEN);
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  image = holding_space_item->image().image_skia();
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorGREEN, image.bitmap()->getColor(5, 5));

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
}

// Verifies that holding space image can be refreshed while the initial image
// load is in progress - test the case where the initial load request finishes
// after the request for refreshed image.
TEST(HoldingSpaceImageTest,
     ImageRefreshDuringInitialLoadWithOutOfOrderResponses) {
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item = CreateTestItem(
      &image_generator, /*image_size=*/10, /*image_color=*/SK_ColorRED);

  // The test client implementation requests an image on construction.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  // Use placeholder while the image load is in progress.
  gfx::ImageSkia image = holding_space_item->image().image_skia();
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorRED, image.bitmap()->getColor(5, 5));

  holding_space_item->InvalidateImage();
  EXPECT_EQ(2u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  // Fulfill the later request, and verify the icon gets updated.
  image_generator.FulfillRequest(1, SK_ColorGREEN);
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  image = holding_space_item->image().image_skia();
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorGREEN, image.bitmap()->getColor(5, 5));

  // Fulfill the initial request, and verify the result is ignored
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  image_generator.FulfillRequest(0, SK_ColorBLUE);
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  image = holding_space_item->image().image_skia();
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorGREEN, image.bitmap()->getColor(5, 5));

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
}

// Verifies that holding space image representation can be requested even after
// the holding space item gets deleted (in which case the image will continue
// using the image placeholder).
TEST(HoldingSpaceImageTest, ImageRequestsAfterItemDestruction) {
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item = CreateTestItem(
      &image_generator, /*image_size=*/10, /*image_color=*/SK_ColorRED);

  // Finish the flow for loading 1x bitmap.
  TestImageClient image_client(&holding_space_item->image());
  image_generator.FulfillRequest(0, SK_ColorBLUE);

  gfx::ImageSkia image = holding_space_item->image().image_skia();
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));

  // Reset the holding space item, and request 2x representation.
  holding_space_item.reset();
  const SkBitmap bitmap_2x = image.GetRepresentation(2.0f).GetBitmap();

  // Verify that image returns the placeholder bitmap, and that no image
  // generation requests are actually issued.
  EXPECT_EQ(20, bitmap_2x.height());
  EXPECT_EQ(20, bitmap_2x.width());
  EXPECT_EQ(SK_ColorRED, bitmap_2x.getColor(5, 5));

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
}

// Tests that HoldingSpaceImage can handle holding space item destruction while
// image load is still in progress.
TEST(HoldingSpaceImageTest, ItemDestructionDuringImageLoad) {
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item = CreateTestItem(
      &image_generator, /*image_size=*/10, /*image_color=*/SK_ColorRED);

  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  gfx::ImageSkia image = holding_space_item->image().image_skia();
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorRED, image.bitmap()->getColor(5, 5));

  // Reset the item, and then simulate image request response.
  holding_space_item.reset();
  image_generator.FulfillRequest(0, SK_ColorBLUE);

  // Verify that the image keeps using the placeholder bitmap.
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorRED, image.bitmap()->getColor(5, 5));

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
}

// Tests that HoldingSpaceImage can handle holding space item destruction while
// image load is still in progress, in case the pending image load fails.
TEST(HoldingSpaceImageTest, ItemDestructionDuringFailedImageLoad) {
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item = CreateTestItem(
      &image_generator, /*image_size=*/10, /*image_color=*/SK_ColorRED);

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  TestImageClient image_client(&holding_space_item->image());

  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  gfx::ImageSkia image = holding_space_item->image().image_skia();
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorRED, image.bitmap()->getColor(5, 5));

  // Reset the item, and then simulate a failed image request response.
  holding_space_item.reset();
  image_generator.FailRequest(0);

  // Verify that the image keeps using the placeholder bitmap.
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorRED, image.bitmap()->getColor(5, 5));

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
}

// Verifies that HoldingSpaceImage can handle holding space item destruction
// while image refresh is in progress.
TEST(HoldingSpaceImageTest, ItemDestructionDuringImageRefresh) {
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item = CreateTestItem(
      &image_generator, /*image_size=*/10, /*image_color=*/SK_ColorRED);

  // Run thr flow for loading the initial image version.
  TestImageClient image_client(&holding_space_item->image());
  image_generator.FulfillRequest(0, SK_ColorBLUE);
  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  gfx::ImageSkia image = holding_space_item->image().image_skia();
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));

  holding_space_item->InvalidateImage();
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  // Reset the item before image load request completes.
  holding_space_item.reset();
  image_generator.FulfillRequest(0, SK_ColorGREEN);

  // Verify that the image keeps using previously generated icon.
  EXPECT_EQ(gfx::Size(10, 10), image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
}

}  // namespace ash
