// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnail/cc/etc1_thumbnail_helper.h"

#include <cstring>
#include <memory>
#include <utility>
#include <vector>

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
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/display/screen.h"
#include "ui/display/screen_base.h"

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
    SkColor colors[] = {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE};
    SkScalar pos[] = {0, SK_Scalar1 / 2, SK_Scalar1};
    SkPaint paint;
    paint.setShader(SkGradientShader::MakeSweep(256, 256, colors, pos, 3));
    return paint;
  }

  base::FilePath GetFile(int tab_id) { return interface_->GetFilePath(tab_id); }

  content::BrowserTaskEnvironment task_environment_;

 private:
  std::unique_ptr<thumbnail::Etc1ThumbnailHelper> interface_;
  base::ScopedTempDir temp_dir_;
  display::ScreenBase screen_;
};

TEST_F(Etc1ThumbnailHelperTest, CompressAndDecompressThumbnail) {
  // Create a bitmap
  SkBitmap image;
  ASSERT_TRUE(image.tryAllocN32Pixels(kWidth, kHeight));
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

  // Decompress the image
  base::RunLoop loop2;
  base::OnceCallback<void(bool, const SkBitmap&)> decompress_once =
      base::BindOnce(
          [](SkBitmap* image, bool success, const SkBitmap& decompressed_data) {
            EXPECT_TRUE(success);
            EXPECT_FALSE(decompressed_data.empty());

            EXPECT_EQ(decompressed_data.width(), image->width());
            EXPECT_EQ(decompressed_data.height(), image->height());
          },
          &image)
          .Then(loop2.QuitClosure());

  GetInterface().Decompress(std::move(decompress_once), compressed_data_copy,
                            1.f, data_size_copy);
  loop2.Run();
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

  EXPECT_EQ(
      0, memcmp(compressed_data_copy->pixels(), read_compressed_data->pixels(),
                compressed_data_copy->rowBytes()));

  base::FilePath file_path_post_read = GetFile(tab_id);
  EXPECT_TRUE(base::PathExists(file_path_post_read));

  // Delete the image
  GetInterface().Delete(tab_id);
  task_environment_.RunUntilIdle();

  // Check deletion
  base::FilePath post_delete_file_path = GetFile(tab_id);
  EXPECT_FALSE(base::PathExists(post_delete_file_path));
}

}  // namespace thumbnail
