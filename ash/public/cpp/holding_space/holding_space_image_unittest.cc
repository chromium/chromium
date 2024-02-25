// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_image.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {

namespace {

// Appearance.
constexpr gfx::Size kImageSize(32, 32);
constexpr int kFileTypeIconSize = 20;

// Helpers ---------------------------------------------------------------------

gfx::ImageSkia ExtractFileTypeIcon(const gfx::ImageSkia& image) {
  gfx::Rect file_type_icon_bounds(image.size());
  file_type_icon_bounds.ClampToCenteredSize(
      gfx::Size(kFileTypeIconSize, kFileTypeIconSize));
  return gfx::ImageSkiaOperations::ExtractSubset(image, file_type_icon_bounds);
}

bool ContainsFileTypeIcon(const gfx::ImageSkia& image,
                          const base::FilePath& file_path) {
  gfx::ImageSkia actual = ExtractFileTypeIcon(image);
  gfx::ImageSkia expected =
      chromeos::GetIconForPath(file_path, /*dark_background=*/false);
  return gfx::test::AreImagesEqual(gfx::Image(actual), gfx::Image(expected));
}

bool ContainsFolderTypeIcon(const gfx::ImageSkia& image) {
  gfx::ImageSkia actual = ExtractFileTypeIcon(image);
  gfx::ImageSkia expected = chromeos::GetIconFromType(
      chromeos::IconType::kFolder, /*dark_background=*/false);
  return gfx::test::AreImagesEqual(gfx::Image(actual), gfx::Image(expected));
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

  void GenerateImage(const base::FilePath& file_path,
                     const gfx::Size& size,
                     HoldingSpaceImage::BitmapCallback callback) {
    auto request = std::make_unique<PendingRequest>();
    request->file_path = file_path;
    request->size = size;
    request->callback = std::move(callback);
    pending_requests_.push_back(std::move(request));
  }

  size_t NumberOfPendingRequests() const { return pending_requests_.size(); }

  const base::FilePath& GetPendingRequestFilePath(size_t index) const {
    if (index >= pending_requests_.size()) {
      ADD_FAILURE() << "Invalid index " << index;
      static base::FilePath kEmptyPath;
      return kEmptyPath;
    }
    return pending_requests_[index]->file_path;
  }

  void FulfillRequest(size_t index, SkColor color) {
    ASSERT_LT(index, pending_requests_.size());

    auto it = pending_requests_.begin() + index;
    SkBitmap result = gfx::test::CreateBitmap((*it)->size.width(),
                                              (*it)->size.height(), color);
    HoldingSpaceImage::BitmapCallback callback = std::move((*it)->callback);

    pending_requests_.erase(it);

    std::move(callback).Run(&result, base::File::FILE_OK);
  }

  void FailRequest(size_t index,
                   base::File::Error error = base::File::FILE_ERROR_FAILED) {
    ASSERT_LT(index, pending_requests_.size());

    auto it = pending_requests_.begin() + index;
    HoldingSpaceImage::BitmapCallback callback = std::move((*it)->callback);
    pending_requests_.erase(it);

    std::move(callback).Run(/*bitmap=*/nullptr, error);
  }

 private:
  struct PendingRequest {
    base::FilePath file_path;
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
    image_->GetImageSkia().GetRepresentation(1.0f);
  }

  void OnHoldingSpaceItemImageChanged() {
    image_->GetImageSkia().GetRepresentation(1.0f);
    ++image_change_count_;
  }

  size_t GetAndResetImageChangeCount() {
    size_t result = image_change_count_;
    image_change_count_ = 0;
    return result;
  }

 private:
  const raw_ptr<const HoldingSpaceImage, DanglingUntriaged> image_;
  base::CallbackListSubscription image_subscription_;
  size_t image_change_count_ = 0;
};

std::unique_ptr<HoldingSpaceItem> CreateTestItem(
    const base::FilePath& file_path,
    ImageGenerator* image_generator,
    const gfx::Size& image_size) {
  return HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kPinnedFile,
      HoldingSpaceFile(file_path, HoldingSpaceFile::FileSystemType::kTest,
                       GURL("filesystem:file_system_url")),
      base::BindLambdaForTesting([&](HoldingSpaceItem::Type type,
                                     const base::FilePath& file_path) {
        return std::make_unique<HoldingSpaceImage>(
            image_size, file_path, image_generator->CreateResolverCallback());
      }));
}

}  // namespace

class HoldingSpaceImageTest : public ::testing::Test {
 public:
  HoldingSpaceImageTest() = default;
  HoldingSpaceImageTest(const HoldingSpaceImageTest&) = delete;
  HoldingSpaceImageTest& operator=(const HoldingSpaceImageTest&) = delete;
  ~HoldingSpaceImageTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

// Tests the basic flow for generating holding space image bitmaps.
TEST_F(HoldingSpaceImageTest, ImageGeneration) {
  const base::FilePath kTestFile("test_file.test");
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item =
      CreateTestItem(kTestFile, &image_generator, kImageSize);

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());

  // The test client implementation requests an image on construction.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kTestFile, image_generator.GetPendingRequestFilePath(0));
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  // The image should return the placeholder bitmap.
  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorTRANSPARENT, image.bitmap()->getColor(5, 5));

  // Generate the holding space item image, and verify the icon has been
  // updated.
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  image_generator.FulfillRequest(0, SK_ColorBLUE);
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
}

// Tests the basic flow for generating holding space image bitmaps where 2x
// bitmap gets requested.
TEST_F(HoldingSpaceImageTest, ImageGenerationWith2xScale) {
  const base::FilePath kTestFile("test_file.test");
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item =
      CreateTestItem(kTestFile, &image_generator, kImageSize);

  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kTestFile, image_generator.GetPendingRequestFilePath(0));
  image_generator.FulfillRequest(0, SK_ColorBLUE);
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  // Request 2x bitmap.
  const SkBitmap bitmap_2x = image.GetRepresentation(2.0f).GetBitmap();
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  // Use placeholder while the image request is in progress.
  EXPECT_EQ(kImageSize.height() * 2, bitmap_2x.height());
  EXPECT_EQ(kImageSize.width() * 2, bitmap_2x.width());
  EXPECT_EQ(SK_ColorTRANSPARENT, bitmap_2x.getColor(5, 5));

  // Verify that the image gets updated once the holding space image is
  // generated.
  image_generator.FulfillRequest(0, SK_ColorBLUE);

  image = holding_space_item->image().GetImageSkia();
  const SkBitmap loaded_bitmap_2x = image.GetRepresentation(2.0f).GetBitmap();
  EXPECT_EQ(kImageSize.height() * 2, loaded_bitmap_2x.height());
  EXPECT_EQ(kImageSize.width() * 2, loaded_bitmap_2x.width());
  EXPECT_EQ(SK_ColorBLUE, loaded_bitmap_2x.getColor(5, 5));
}

// Verifies that the holding space image handles failed holding space image
// requests.
TEST_F(HoldingSpaceImageTest, ImageLoadFailure) {
  const base::FilePath file_path("test_file.txt");
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item =
      CreateTestItem(file_path, &image_generator, kImageSize);

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());

  // Test image client requests an image representation during construction.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  // Use placeholder while the image request is in progress.
  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorTRANSPARENT, image.bitmap()->getColor(5, 5));

  // Simulate failed holding space item image request, and verify the icon will
  // fallback to the file type icon.
  image_generator.FailRequest(0);

  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());
  image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_TRUE(ContainsFileTypeIcon(image, file_path));
}

// Verifies that the holding space image handles failed holding space image
// requests and special cases the fallback image for folders.
TEST_F(HoldingSpaceImageTest, ImageLoadFailureForFolder) {
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item = CreateTestItem(
      base::FilePath("test_folder"), &image_generator, kImageSize);

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());

  // Test image client requests an image representation during construction.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  // Use placeholder while the image request is in progress.
  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorTRANSPARENT, image.bitmap()->getColor(5, 5));

  // Simulate failed holding space item image request using the special case
  // error code used to signal that the backing file is a folder.
  image_generator.FailRequest(0, base::File::FILE_ERROR_NOT_A_FILE);

  // Verify the image will fallback to the folder type icon.
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());
  image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_TRUE(ContainsFolderTypeIcon(image));
}

// Verifies that the holding space image can be updated using
// `HoldingSpaceItem::InvalidateImage()`.
TEST_F(HoldingSpaceImageTest, ImageRefresh) {
  ImageGenerator image_generator;
  const base::FilePath kTestFile("test_file.test");
  std::unique_ptr<HoldingSpaceItem> holding_space_item =
      CreateTestItem(kTestFile, &image_generator, kImageSize);

  // Finish loading the initial image.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kTestFile, image_generator.GetPendingRequestFilePath(0));
  image_generator.FulfillRequest(0, SK_ColorBLUE);
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  // Request image refresh, and verify another image gets requested.
  holding_space_item->InvalidateImage();
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());

  ASSERT_TRUE(
      holding_space_item->image_for_testing().FireInvalidateTimerForTesting());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kTestFile, image_generator.GetPendingRequestFilePath(0));

  // While image load request is in progress, use the previously loaded icon.
  image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  // Verify that image gets updated once the image load request completes.
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  image_generator.FulfillRequest(0, SK_ColorGREEN);
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorGREEN, image.bitmap()->getColor(5, 5));

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
}

// Verifies that image refresh requests issued in quick succession do not result
// in multiple image load requests.
TEST_F(HoldingSpaceImageTest, ImageRefreshThrottling) {
  ImageGenerator image_generator;
  const base::FilePath kTestFile("test_file.test");
  std::unique_ptr<HoldingSpaceItem> holding_space_item =
      CreateTestItem(kTestFile, &image_generator, kImageSize);

  // Finish loading the initial image.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  image_generator.FulfillRequest(0, SK_ColorBLUE);
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  // Request image refresh multiple times, and verify that image load gets
  // requested only once.
  holding_space_item->InvalidateImage();
  holding_space_item->InvalidateImage();
  holding_space_item->InvalidateImage();
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());

  ASSERT_TRUE(
      holding_space_item->image_for_testing().FireInvalidateTimerForTesting());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kTestFile, image_generator.GetPendingRequestFilePath(0));
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  // Verify that image gets updated once the image load request completes.
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  image_generator.FulfillRequest(0, SK_ColorGREEN);
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorGREEN, image.bitmap()->getColor(5, 5));

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
}

// Verifies that holding space image can be refreshed while the initial image
// load is in progress.
TEST_F(HoldingSpaceImageTest, ImageRefreshDuringInitialLoad) {
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item = CreateTestItem(
      base::FilePath("test_file.txt"), &image_generator, kImageSize);

  // The test client implementation requests an image on construction.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  // Use placeholder while the image load is in progress.
  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorTRANSPARENT, image.bitmap()->getColor(5, 5));

  holding_space_item->InvalidateImage();
  ASSERT_TRUE(
      holding_space_item->image_for_testing().FireInvalidateTimerForTesting());

  // Verify that placeholder image remains to be used.
  image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorTRANSPARENT, image.bitmap()->getColor(5, 5));

  EXPECT_EQ(2u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  // Fulfill the initial request - the load result should be ignored.
  image_generator.FulfillRequest(0, SK_ColorBLUE);
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorTRANSPARENT, image.bitmap()->getColor(5, 5));

  // Fulfill the later request, and verify the icon gets updated.
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  image_generator.FulfillRequest(0, SK_ColorGREEN);
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorGREEN, image.bitmap()->getColor(5, 5));

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
}

// Verifies that holding space image can be refreshed while the initial image
// load is in progress - test the case where the initial load request finishes
// after the request for refreshed image.
TEST_F(HoldingSpaceImageTest,
       ImageRefreshDuringInitialLoadWithOutOfOrderResponses) {
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item = CreateTestItem(
      base::FilePath("test_file.txt"), &image_generator, kImageSize);

  // The test client implementation requests an image on construction.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  // Use placeholder while the image load is in progress.
  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorTRANSPARENT, image.bitmap()->getColor(5, 5));

  holding_space_item->InvalidateImage();
  ASSERT_TRUE(
      holding_space_item->image_for_testing().FireInvalidateTimerForTesting());
  EXPECT_EQ(2u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  // Fulfill the later request, and verify the icon gets updated.
  image_generator.FulfillRequest(1, SK_ColorGREEN);
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorGREEN, image.bitmap()->getColor(5, 5));

  // Fulfill the initial request, and verify the result is ignored
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  image_generator.FulfillRequest(0, SK_ColorBLUE);
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorGREEN, image.bitmap()->getColor(5, 5));

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
}

// Verifies that holding space image representation can be requested even after
// the holding space item gets deleted (in which case the image will continue
// using the image placeholder).
TEST_F(HoldingSpaceImageTest, ImageRequestsAfterItemDestruction) {
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item = CreateTestItem(
      base::FilePath("test_file.txt"), &image_generator, kImageSize);

  // Finish the flow for loading 1x bitmap.
  TestImageClient image_client(&holding_space_item->image());
  image_generator.FulfillRequest(0, SK_ColorBLUE);

  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));

  // Reset the holding space item, and request 2x representation.
  holding_space_item.reset();
  const SkBitmap bitmap_2x = image.GetRepresentation(2.0f).GetBitmap();

  // Verify that image returns the placeholder bitmap, and that no image
  // generation requests are actually issued.
  EXPECT_EQ(kImageSize.height() * 2, bitmap_2x.height());
  EXPECT_EQ(kImageSize.width() * 2, bitmap_2x.width());
  EXPECT_EQ(SK_ColorTRANSPARENT, bitmap_2x.getColor(5, 5));

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
}

// Tests that HoldingSpaceImage can handle holding space item destruction while
// image load is still in progress.
TEST_F(HoldingSpaceImageTest, ItemDestructionDuringImageLoad) {
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item = CreateTestItem(
      base::FilePath("test_file.txt"), &image_generator, kImageSize);

  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorTRANSPARENT, image.bitmap()->getColor(5, 5));

  // Reset the item, and then simulate image request response.
  holding_space_item.reset();
  image_generator.FulfillRequest(0, SK_ColorBLUE);

  // Verify that the image keeps using the placeholder bitmap.
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorTRANSPARENT, image.bitmap()->getColor(5, 5));

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
}

// Tests that HoldingSpaceImage can handle holding space item destruction while
// image load is still in progress, in case the pending image load fails.
TEST_F(HoldingSpaceImageTest, ItemDestructionDuringFailedImageLoad) {
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item = CreateTestItem(
      base::FilePath("test_file.txt"), &image_generator, kImageSize);

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  TestImageClient image_client(&holding_space_item->image());

  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorTRANSPARENT, image.bitmap()->getColor(5, 5));

  // Reset the item, and then simulate a failed image request response.
  holding_space_item.reset();
  image_generator.FailRequest(0);

  // Verify that the image keeps using the placeholder bitmap.
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorTRANSPARENT, image.bitmap()->getColor(5, 5));

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
}

// Verifies that HoldingSpaceImage can handle holding space item destruction
// while image refresh is in progress.
TEST_F(HoldingSpaceImageTest, ItemDestructionDuringImageRefresh) {
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item = CreateTestItem(
      base::FilePath("test_file.txt"), &image_generator, kImageSize);

  // Run thr flow for loading the initial image version.
  TestImageClient image_client(&holding_space_item->image());
  image_generator.FulfillRequest(0, SK_ColorBLUE);
  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));

  holding_space_item->InvalidateImage();
  ASSERT_TRUE(
      holding_space_item->image_for_testing().FireInvalidateTimerForTesting());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  // Reset the item before image load request completes.
  holding_space_item.reset();
  image_generator.FulfillRequest(0, SK_ColorGREEN);

  // Verify that the image keeps using previously generated icon.
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
}

// Tests that image load requests use the new file path if the item's backing
// file path changes.
TEST_F(HoldingSpaceImageTest, HandleBackingFilePathChange) {
  const base::FilePath kTestFile("test_file.test");
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item =
      CreateTestItem(kTestFile, &image_generator, kImageSize);

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());

  // Update the backing file path before any image representations are
  // requested.
  const base::FilePath kUpdatedTestFile("updated_test_file.test");
  holding_space_item->SetBackingFile(HoldingSpaceFile(
      kUpdatedTestFile, HoldingSpaceFile::FileSystemType::kTest,
      GURL("filesystem:updated_file_system_url")));

  // Create test image client to issue an image request.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kUpdatedTestFile, image_generator.GetPendingRequestFilePath(0));
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  image_generator.FulfillRequest(0, SK_ColorBLUE);
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  // Refresh image, and verify the reload request also uses the upated file
  // path.
  holding_space_item->InvalidateImage();
  ASSERT_TRUE(
      holding_space_item->image_for_testing().FireInvalidateTimerForTesting());
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kUpdatedTestFile, image_generator.GetPendingRequestFilePath(0));
  image_generator.FulfillRequest(0, SK_ColorGREEN);
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorGREEN, image.bitmap()->getColor(5, 5));
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
}

// Tests that image load requests use the new file path if the item's backing
// file path changes.
TEST_F(HoldingSpaceImageTest, HandleBackingFilePathChangeFor2xBitmap) {
  const base::FilePath kTestFile("test_file.test");
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item =
      CreateTestItem(kTestFile, &image_generator, kImageSize);

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());

  // Create test image client to issue an image request.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kTestFile, image_generator.GetPendingRequestFilePath(0));
  image_generator.FulfillRequest(0, SK_ColorBLUE);
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  // Update the backing file path, and verify requests for 2x bitmap will use
  // the new file path.
  const base::FilePath kUpdatedTestFile("updated_test_file.test");
  holding_space_item->SetBackingFile(HoldingSpaceFile(
      kUpdatedTestFile, HoldingSpaceFile::FileSystemType::kTest,
      GURL("filesystem:updated_file_system_url")));
  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());

  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  image.GetRepresentation(2.0f);

  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kUpdatedTestFile, image_generator.GetPendingRequestFilePath(0));
  image_generator.FulfillRequest(0, SK_ColorBLUE);

  const SkBitmap bitmap_2x = image.GetRepresentation(2.0f).GetBitmap();
  EXPECT_EQ(kImageSize.height() * 2, bitmap_2x.height());
  EXPECT_EQ(kImageSize.width() * 2, bitmap_2x.width());
  EXPECT_EQ(SK_ColorBLUE, bitmap_2x.getColor(10, 10));
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());
}

// Tests that failed image loads will be retried for requests issued before
// the path change.
TEST_F(HoldingSpaceImageTest, RetryFailedImageRequestsOnFilePathChange) {
  const base::FilePath kTestFile("test_file.test");
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item =
      CreateTestItem(kTestFile, &image_generator, kImageSize);

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());

  // Create test image client to issue an image request.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kTestFile, image_generator.GetPendingRequestFilePath(0));

  // Update the backing file path, and simulate image load failure.
  const base::FilePath kUpdatedTestFile("updated_test_file.test");
  holding_space_item->SetBackingFile(HoldingSpaceFile(
      kUpdatedTestFile, HoldingSpaceFile::FileSystemType::kTest,
      GURL("filesystem:updated_file_system_url")));
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  image_generator.FailRequest(0);

  // Verify that image load is retried using the new file path.
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kUpdatedTestFile, image_generator.GetPendingRequestFilePath(0));

  image_generator.FulfillRequest(0, SK_ColorBLUE);

  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());
}

// Tests that failed image loads will be not be retried for requests that failed
// before file path change.
TEST_F(HoldingSpaceImageTest,
       DontRetryImageRequestsFailedBeforeFilePathChange) {
  const base::FilePath kTestFile("test_file.test");
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item =
      CreateTestItem(kTestFile, &image_generator, kImageSize);

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());

  // Create test image client to issue an image request.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kTestFile, image_generator.GetPendingRequestFilePath(0));
  // Simulate request failure before updating the backing file path.
  image_generator.FailRequest(0);

  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_TRUE(ContainsFileTypeIcon(image, kTestFile));

  // Update the backing file path, and verify the failed request was not
  // retried.
  const base::FilePath kUpdatedTestFile("updated_test_file.test");
  holding_space_item->SetBackingFile(HoldingSpaceFile(
      kUpdatedTestFile, HoldingSpaceFile::FileSystemType::kTest,
      GURL("filesystem:updated_file_system_url")));

  // Verify that image load is retried using the new file path.
  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());

  image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_TRUE(ContainsFileTypeIcon(image, kTestFile));
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());
}

// Tests that failed image loads will be not be retried for requests issued
// after file path change.
TEST_F(HoldingSpaceImageTest, DontRetryImageRequestsFailedAfterPathChange) {
  const base::FilePath kTestFile("test_file.test");
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item =
      CreateTestItem(kTestFile, &image_generator, kImageSize);

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());

  // Update the backing file path before creating a client that requests an
  // image representation.
  const base::FilePath kUpdatedTestFile("updated_test_file.test");
  holding_space_item->SetBackingFile(HoldingSpaceFile(
      kUpdatedTestFile, HoldingSpaceFile::FileSystemType::kTest,
      GURL("filesystem:updated_file_system_url")));

  // Create test image client, and simulate image load failure.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kUpdatedTestFile, image_generator.GetPendingRequestFilePath(0));
  image_generator.FailRequest(0);

  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_TRUE(ContainsFileTypeIcon(image, kUpdatedTestFile));
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  // Verify that the request is not retried.
  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
}

// Tests that changing the backing file alone does not retry image loads.
TEST_F(HoldingSpaceImageTest, DontRetryImageLoadOnBackingFileChange) {
  const base::FilePath kTestFile("test_file.test");
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item =
      CreateTestItem(kTestFile, &image_generator, kImageSize);

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());

  // Create test image client, and simulate a successfull image load.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kTestFile, image_generator.GetPendingRequestFilePath(0));
  image_generator.FulfillRequest(0, SK_ColorBLUE);

  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());
  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));

  // Update the backing file path, and verify the image load is not requested
  // again.
  const base::FilePath kUpdatedTestFile("updated_test_file.test");
  holding_space_item->SetBackingFile(HoldingSpaceFile(
      kUpdatedTestFile, HoldingSpaceFile::FileSystemType::kTest,
      GURL("filesystem:updated_file_system_url")));

  image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));

  // Verify that the request is not retried.
  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
}

// Tests that changing the backing file alone does not retry image loads, even
// if the image load finishes after file path changes.
TEST_F(HoldingSpaceImageTest,
       DontRetryImageLaodsThatSucceedDuringBackingFileChange) {
  const base::FilePath kTestFile("test_file.test");
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item =
      CreateTestItem(kTestFile, &image_generator, kImageSize);
  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());

  // Create test image client.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kTestFile, image_generator.GetPendingRequestFilePath(0));

  // Update the backing file path, and verify the image load is not requested
  // again.
  const base::FilePath kUpdatedTestFile("updated_test_file.test");
  holding_space_item->SetBackingFile(HoldingSpaceFile(
      kUpdatedTestFile, HoldingSpaceFile::FileSystemType::kTest,
      GURL("filesystem:updated_file_system_url")));
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());

  // Finish initial load request.
  image_generator.FulfillRequest(0, SK_ColorBLUE);

  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());
  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));

  // Verify that the request is not retried.
  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());

  // Image should still be reloaded if it gets refreshed.
  holding_space_item->InvalidateImage();
  ASSERT_TRUE(
      holding_space_item->image_for_testing().FireInvalidateTimerForTesting());

  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());
  EXPECT_EQ(kUpdatedTestFile, image_generator.GetPendingRequestFilePath(0));
}

// Tests a scenario where the item backing file is moved and modified while the
// initial image load request is still in progress,
TEST_F(HoldingSpaceImageTest, ItemPathMovedAndModifiedDuringInitialLoad) {
  const base::FilePath kTestFile("test_file.test");
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item =
      CreateTestItem(kTestFile, &image_generator, kImageSize);
  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());

  // Create test image client to initiate image request.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kTestFile, image_generator.GetPendingRequestFilePath(0));

  // Update the backing file path, and then invalidate the image.
  const base::FilePath kUpdatedTestFile("updated_test_file.test");
  holding_space_item->SetBackingFile(HoldingSpaceFile(
      kUpdatedTestFile, HoldingSpaceFile::FileSystemType::kTest,
      GURL("filesystem:updated_file_system_url")));
  holding_space_item->InvalidateImage();
  ASSERT_TRUE(
      holding_space_item->image_for_testing().FireInvalidateTimerForTesting());
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());

  // Verify that a image reload gets requested.
  EXPECT_EQ(2u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kTestFile, image_generator.GetPendingRequestFilePath(0));
  EXPECT_EQ(kUpdatedTestFile, image_generator.GetPendingRequestFilePath(1));

  // Finish initial load request - the result should be ignored.
  image_generator.FulfillRequest(0, SK_ColorBLUE);

  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorTRANSPARENT, image.bitmap()->getColor(5, 5));

  // Finish the later request, and verify the image gets updated.
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kUpdatedTestFile, image_generator.GetPendingRequestFilePath(0));
  image_generator.FulfillRequest(0, SK_ColorGREEN);

  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());
  image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorGREEN, image.bitmap()->getColor(5, 5));

  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
}

// After failure, holding space images will continue to serve placeholders in
// response to `GetImageSkia()` invocations. Requests to `Invalidate()` the
// image should result in a new attempt to resolve the appropriate bitmap.
TEST_F(HoldingSpaceImageTest, InvalidationAfterFailure) {
  // Create a `holding_space_item` whose image is associated with a fake
  // `image_generator` for testing.
  const base::FilePath kTestFile("test_file.test");
  ImageGenerator image_generator;
  std::unique_ptr<HoldingSpaceItem> holding_space_item =
      CreateTestItem(kTestFile, &image_generator, kImageSize);
  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());

  // Create an `image_client` to handle image requests. Note that this will
  // result in an image request being pended immediately.
  TestImageClient image_client(&holding_space_item->image());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kTestFile, image_generator.GetPendingRequestFilePath(0));

  // While the request is still pending, the image should use a placeholder.
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
  gfx::ImageSkia image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorTRANSPARENT, image.bitmap()->getColor(5, 5));

  // Fail the pending image request.
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kTestFile, image_generator.GetPendingRequestFilePath(0));
  image_generator.FailRequest(0);

  // The image should use a placeholder corresponding to the file type of the
  // associated backing file. Note that image subscribers should have been
  // notified of the change.
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());
  image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_TRUE(ContainsFileTypeIcon(image, kTestFile));

  // Invalidate the image. Because the previous request failed, this should
  // result in a new pending image request but no event to image subscribers.
  holding_space_item->InvalidateImage();
  ASSERT_TRUE(
      holding_space_item->image_for_testing().FireInvalidateTimerForTesting());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kTestFile, image_generator.GetPendingRequestFilePath(0));

  // While the request is still pending, the image should still use a
  // placeholder corresponding to the file type of the associated backing file.
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
  image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_TRUE(ContainsFileTypeIcon(image, kTestFile));

  // Fulfill the pending image request.
  EXPECT_EQ(1u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(kTestFile, image_generator.GetPendingRequestFilePath(0));
  image_generator.FulfillRequest(0, SK_ColorBLUE);

  // Verify the image is now using the fulfilled image. Note that image
  // subscribers should have been notified of the change.
  EXPECT_EQ(1u, image_client.GetAndResetImageChangeCount());
  image = holding_space_item->image().GetImageSkia();
  EXPECT_EQ(kImageSize, image.size());
  EXPECT_EQ(SK_ColorBLUE, image.bitmap()->getColor(5, 5));

  // Absent any new requests to `Invalidate()` the image, future images
  // returned from invoking `GetImageSkia()` should be served from cache.
  EXPECT_EQ(0u, image_generator.NumberOfPendingRequests());
  EXPECT_EQ(0u, image_client.GetAndResetImageChangeCount());
}

}  // namespace ash
