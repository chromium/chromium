// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnail/cc/etc1_thumbnail_helper.h"

#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/thumbnail/cc/thumbnail.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/android_opengl/etc1/etc1.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/effects/SkGradient.h"
#include "ui/android/resources/etc1_utils.h"
#include "ui/display/screen.h"
#include "ui/display/screen_base.h"
#include "ui/gfx/skia_util.h"

namespace thumbnail {
namespace {

constexpr int kDimension = 16;
constexpr int kKiB = 1024;
constexpr int kWidth = kDimension * kKiB;
constexpr int kHeight = kDimension;

}  // namespace

class Etc1ThumbnailHelperTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    interface_ = std::make_unique<thumbnail::Etc1ThumbnailHelper>(
        temp_dir_.GetPath(),
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));

    if (screen_.GetNumDisplays() == 0) {
      screen_.display_list().AddDisplay({1, gfx::Rect(kWidth, kHeight)},
                                        display::DisplayList::Type::PRIMARY);
    }
    display::Screen::SetScreenInstance(&screen_);
  }

  void TearDown() override { display::Screen::SetScreenInstance(nullptr); }

  thumbnail::Etc1ThumbnailHelper& GetInterface() { return *interface_; }
  SkPaint SetupPaint() {
    SkColor4f colors[] = {SkColors::kRed, SkColors::kGreen, SkColors::kBlue};
    SkScalar pos[] = {0, SK_Scalar1 / 2, SK_Scalar1};
    SkPaint paint;
    paint.setShader(SkShaders::SweepGradient({256, 256}, {{colors, pos, SkTileMode::kClamp}, {}}));
    return paint;
  }

  base::FilePath GetFile(int tab_id) { return interface_->GetFilePath(tab_id); }

  content::BrowserTaskEnvironment task_environment_;

 private:
  std::unique_ptr<thumbnail::Etc1ThumbnailHelper> interface_;
  base::ScopedTempDir temp_dir_;
  display::ScreenBase screen_;
};

TEST_F(Etc1ThumbnailHelperTest, CompressThumbnail) {
  // Create a bitmap
  SkBitmap image;
  ASSERT_TRUE(image.tryAllocN32Pixels(kWidth, kHeight));
  SkCanvas canvas(image);
  canvas.drawPaint(SetupPaint());
  image.setImmutable();

  // Compress the bitmap
  base::RunLoop loop1;
  base::OnceCallback<void(sk_sp<SkPixelRef>, const gfx::Size&)> compress_once =
      base::BindOnce([](sk_sp<SkPixelRef> compressed_data,
                        const gfx::Size& data_size) {
        EXPECT_GT(compressed_data->width(), 0);
        EXPECT_GT(compressed_data->height(), 0);

        EXPECT_GT(data_size.width(), 0);
        EXPECT_GT(data_size.height(), 0);

        gfx::Size buffer_size =
            gfx::Size(compressed_data->width(), compressed_data->height());

        EXPECT_EQ(data_size, buffer_size);

        SkBitmap raw_data;
        raw_data.allocPixels(SkImageInfo::MakeN32(
            buffer_size.width(), buffer_size.height(), kOpaque_SkAlphaType));
        bool success = etc1_decode_image(
            reinterpret_cast<unsigned char*>(compressed_data->pixels()),
            reinterpret_cast<unsigned char*>(raw_data.getPixels()),
            buffer_size.width(), buffer_size.height(), raw_data.bytesPerPixel(),
            raw_data.rowBytes());
        EXPECT_TRUE(success);
      }).Then(loop1.QuitClosure());

  GetInterface().Compress(image, true, std::move(compress_once));
  loop1.Run();
}

TEST_F(Etc1ThumbnailHelperTest, WriteReadAndDeleteThumbnail) {
  int tab_id = 0;

  // Create a bitmap
  SkBitmap image;
  ASSERT_TRUE(image.tryAllocN32Pixels(kDimension * kKiB, kDimension));
  SkCanvas canvas(image);
  canvas.drawPaint(SetupPaint());
  image.setImmutable();

  sk_sp<SkPixelRef> compressed_data_copy;
  gfx::Size data_size_copy;

  // Compress the bitmap
  base::RunLoop loop1;
  base::OnceCallback<void(sk_sp<SkPixelRef>, const gfx::Size&)> compress_once =
      base::BindOnce(
          [](sk_sp<SkPixelRef>* compressed_data_copy, gfx::Size* data_size_copy,
             sk_sp<SkPixelRef> compressed_data, const gfx::Size& data_size) {
            EXPECT_GT(compressed_data->width(), 0);
            EXPECT_GT(compressed_data->height(), 0);

            EXPECT_GT(data_size.width(), 0);
            EXPECT_GT(data_size.height(), 0);

            gfx::Size buffer_size =
                gfx::Size(compressed_data->width(), compressed_data->height());

            EXPECT_EQ(data_size, buffer_size);

            SkBitmap raw_data;
            raw_data.allocPixels(SkImageInfo::MakeN32(buffer_size.width(),
                                                      buffer_size.height(),
                                                      kOpaque_SkAlphaType));
            bool success = etc1_decode_image(
                reinterpret_cast<unsigned char*>(compressed_data->pixels()),
                reinterpret_cast<unsigned char*>(raw_data.getPixels()),
                buffer_size.width(), buffer_size.height(),
                raw_data.bytesPerPixel(), raw_data.rowBytes());
            EXPECT_TRUE(success);

            *compressed_data_copy = compressed_data;
            *data_size_copy = data_size;
          },
          &compressed_data_copy, &data_size_copy)
          .Then(loop1.QuitClosure());

  GetInterface().Compress(image, true, std::move(compress_once));
  loop1.Run();

  // Write the image
  base::RunLoop loop2;
  GetInterface().Write(tab_id, compressed_data_copy, 1.f, data_size_copy,
                       loop2.QuitClosure());
  loop2.Run();

  base::FilePath file_path_post_write = GetFile(tab_id);
  EXPECT_TRUE(base::PathExists(file_path_post_write));

  sk_sp<SkPixelRef> read_compressed_data;
  gfx::Size read_data_size;

  // Read the image
  base::RunLoop loop3;
  base::OnceCallback<void(sk_sp<SkPixelRef>, float, const gfx::Size&)>
      read_once =
          base::BindOnce(
              [](SkBitmap* image, sk_sp<SkPixelRef>* read_compressed_data,
                 gfx::Size* read_data_size, sk_sp<SkPixelRef> compressed_data,
                 float scale, const gfx::Size& data_size) {
                gfx::Size buffer_size = gfx::Size(kWidth, kHeight);

                EXPECT_GT(compressed_data->width(), 0);
                EXPECT_GT(compressed_data->height(), 0);

                EXPECT_GT(data_size.width(), 0);
                EXPECT_GT(data_size.height(), 0);

                SkBitmap raw_data;
                raw_data.allocPixels(SkImageInfo::MakeN32(buffer_size.width(),
                                                          buffer_size.height(),
                                                          kOpaque_SkAlphaType));
                bool success = etc1_decode_image(
                    reinterpret_cast<unsigned char*>(compressed_data->pixels()),
                    reinterpret_cast<unsigned char*>(image->getPixels()),
                    buffer_size.width(), buffer_size.height(),
                    raw_data.bytesPerPixel(), raw_data.rowBytes());
                EXPECT_TRUE(success);
                EXPECT_EQ(scale, 1.f);

                *read_compressed_data = compressed_data;
                *read_data_size = data_size;
              },
              &image, &read_compressed_data, &read_data_size)
              .Then(loop3.QuitClosure());

  GetInterface().Read(
      tab_id, base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                                 std::move(read_once)));
  loop3.Run();

  EXPECT_EQ(compressed_data_copy->width(), read_compressed_data->width());
  EXPECT_EQ(compressed_data_copy->height(), read_compressed_data->height());
  EXPECT_EQ(data_size_copy.width(), read_data_size.width());
  EXPECT_EQ(data_size_copy.height(), read_data_size.height());

  EXPECT_EQ(0, UNSAFE_TODO(memcmp(compressed_data_copy->pixels(),
                                  read_compressed_data->pixels(),
                                  compressed_data_copy->rowBytes())));

  base::FilePath file_path_post_read = GetFile(tab_id);
  EXPECT_TRUE(base::PathExists(file_path_post_read));

  // Delete the image
  GetInterface().Delete(tab_id);
  task_environment_.RunUntilIdle();

  // Check deletion
  base::FilePath post_delete_file_path = GetFile(tab_id);
  EXPECT_FALSE(base::PathExists(post_delete_file_path));
}

TEST_F(Etc1ThumbnailHelperTest, DeleteAllExceptForIds) {
  std::vector<int> tab_ids = {1, 2, 3, 4, 5};
  for (int tab_id : tab_ids) {
    base::WriteFile(GetFile(tab_id), "thumbnail_data");
    EXPECT_TRUE(base::PathExists(GetFile(tab_id)));
  }

  // Create a jpeg file to make sure it is not deleted.
  base::FilePath jpeg_file = GetFile(1).AddExtension(".jpeg");
  base::WriteFile(jpeg_file, "thumbnail_data");
  EXPECT_TRUE(base::PathExists(jpeg_file));

  GetInterface().DeleteAllExceptForIds({2, 4});
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::PathExists(GetFile(1)));
  EXPECT_TRUE(base::PathExists(GetFile(2)));
  EXPECT_FALSE(base::PathExists(GetFile(3)));
  EXPECT_TRUE(base::PathExists(GetFile(4)));
  EXPECT_FALSE(base::PathExists(GetFile(5)));

  EXPECT_TRUE(base::PathExists(jpeg_file));
}

TEST_F(Etc1ThumbnailHelperTest, ReadAndDecompressThumbnail) {
  constexpr int kTabId = 7;

  SkBitmap image;
  ASSERT_TRUE(image.tryAllocN32Pixels(kWidth, kHeight));
  SkCanvas canvas(image);
  canvas.drawPaint(SetupPaint());
  image.setImmutable();

  sk_sp<SkPixelRef> compressed_data;
  gfx::Size data_size;
  {
    base::RunLoop loop;
    GetInterface().Compress(
        image, true,
        base::BindOnce(
            [](sk_sp<SkPixelRef>* out_data, gfx::Size* out_size,
               sk_sp<SkPixelRef> data, const gfx::Size& size) {
              *out_data = std::move(data);
              *out_size = size;
            },
            &compressed_data, &data_size)
            .Then(loop.QuitClosure()));
    loop.Run();
  }
  ASSERT_TRUE(compressed_data);

  SkBitmap expected_decompressed =
      ui::Etc1::DecompressBitmap(data_size, compressed_data);
  ASSERT_FALSE(expected_decompressed.empty());

  {
    base::RunLoop loop;
    GetInterface().Write(kTabId, compressed_data, 1.f, data_size,
                         loop.QuitClosure());
    loop.Run();
  }
  EXPECT_TRUE(base::PathExists(GetFile(kTabId)));

  {
    base::RunLoop loop;
    GetInterface().ReadAndDecompress(
        kTabId,
        base::BindOnce(
            [](const SkBitmap& expected, bool success, const SkBitmap& bitmap) {
              EXPECT_TRUE(success);
              ASSERT_FALSE(bitmap.empty());
              EXPECT_TRUE(gfx::BitmapsAreEqual(bitmap, expected));
            },
            expected_decompressed)
            .Then(loop.QuitClosure()));
    loop.Run();
  }
}

TEST_F(Etc1ThumbnailHelperTest, ReadAndDecompressMissingFile) {
  constexpr int kMissingTabId = 999;
  ASSERT_FALSE(base::PathExists(GetFile(kMissingTabId)));

  base::RunLoop loop;
  GetInterface().ReadAndDecompress(
      kMissingTabId, base::BindOnce([](bool success, const SkBitmap& bitmap) {
                       EXPECT_FALSE(success);
                       EXPECT_TRUE(bitmap.empty());
                     }).Then(loop.QuitClosure()));
  loop.Run();
}

TEST_F(Etc1ThumbnailHelperTest, ReadAndDecompressCorruptFile) {
  constexpr int kCorruptTabId = 888;
  base::FilePath corrupt_path = GetFile(kCorruptTabId);
  ASSERT_TRUE(base::WriteFile(corrupt_path, "corrupt_etc1_data"));
  ASSERT_TRUE(base::PathExists(corrupt_path));

  base::RunLoop loop;
  GetInterface().ReadAndDecompress(
      kCorruptTabId, base::BindOnce([](bool success, const SkBitmap& bitmap) {
                       EXPECT_FALSE(success);
                       EXPECT_TRUE(bitmap.empty());
                     }).Then(loop.QuitClosure()));
  loop.Run();

  EXPECT_FALSE(base::PathExists(corrupt_path));
}

}  // namespace thumbnail
