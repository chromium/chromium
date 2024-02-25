// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnail/cc/jpeg_thumbnail_helper.h"

#include <cstring>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "chrome/browser/thumbnail/cc/thumbnail.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace thumbnail {
namespace {

constexpr int kDimension = 16;
constexpr int kKiB = 1024;

SkPaint SetupPaint() {
  SkColor colors[] = {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE};
  SkScalar pos[] = {0, SK_Scalar1 / 2, SK_Scalar1};
  SkPaint paint;
  paint.setShader(SkGradientShader::MakeSweep(256, 256, colors, pos, 3));
  return paint;
}

}  // anonymous namespace

class JpegThumbnailHelperTest : public ::testing::Test {
 protected:
  JpegThumbnailHelperTest()
      : task_environment_(
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    interface_ = std::make_unique<thumbnail::JpegThumbnailHelper>(
        temp_dir_.GetPath(),
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  }

  void TearDown() override {}

  thumbnail::JpegThumbnailHelper& GetInterface() { return *interface_; }
  base::FilePath GetFile(int tab_id) {
    return interface_->GetJpegFilePath(tab_id);
  }

  base::test::TaskEnvironment task_environment_;

 private:
  std::unique_ptr<thumbnail::JpegThumbnailHelper> interface_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(JpegThumbnailHelperTest, CompressThumbnail) {
  // Create a bitmap
  SkBitmap image;
  ASSERT_TRUE(image.tryAllocN32Pixels(kDimension * kKiB, kDimension));
  SkCanvas canvas(image);
  canvas.drawPaint(SetupPaint());
  image.setImmutable();

  // Compress the bitmap
  base::RunLoop loop1;
  base::OnceCallback<void(std::vector<uint8_t>)> once =
      base::BindOnce([](std::vector<uint8_t> jpeg_data) {
        EXPECT_FALSE(jpeg_data.empty());
        auto bitmap =
            gfx::JPEGCodec::Decode(jpeg_data.data(), jpeg_data.size());
        EXPECT_TRUE(bitmap);
        EXPECT_GT(bitmap->width(), 0);
        EXPECT_GT(bitmap->height(), 0);
      }).Then(loop1.QuitClosure());

  GetInterface().Compress(image, std::move(once));
  task_environment_.RunUntilIdle();
  loop1.Run();
}

TEST_F(JpegThumbnailHelperTest, WriteThumbnail) {
  int tab_id = 0;

  // Create a bitmap
  SkBitmap image;
  ASSERT_TRUE(image.tryAllocN32Pixels(kDimension * kKiB, kDimension));
  SkCanvas canvas(image);
  canvas.drawPaint(SetupPaint());
  image.setImmutable();

  constexpr int kCompressionQuality = 97;
  std::vector<uint8_t> data;
  gfx::JPEGCodec::Encode(image, kCompressionQuality, &data);

  // Write the image
  base::RunLoop loop1;
  GetInterface().Write(tab_id, data,
                       base::BindOnce(
                           [](base::OnceClosure quit, bool success) {
                             EXPECT_TRUE(success);
                             std::move(quit).Run();
                           },
                           loop1.QuitClosure()));
  task_environment_.RunUntilIdle();
  loop1.Run();

  base::FilePath file_path = GetFile(tab_id);
  EXPECT_TRUE(base::PathExists(file_path));

  // Compare original data with written data
  std::optional<std::vector<uint8_t>> read_data =
      base::ReadFileToBytes(file_path);
  ASSERT_EQ(data.size(), read_data->size());
  EXPECT_EQ(0, memcmp(data.data(), read_data->data(), data.size()));
}

TEST_F(JpegThumbnailHelperTest, ReadThumbnail) {
  int tab_id = 0;

  // Create a bitmap
  SkBitmap image;
  ASSERT_TRUE(image.tryAllocN32Pixels(kDimension * kKiB, kDimension));
  SkCanvas canvas(image);
  canvas.drawPaint(SetupPaint());
  image.setImmutable();

  constexpr int kCompressionQuality = 97;
  std::vector<uint8_t> data;
  gfx::JPEGCodec::Encode(image, kCompressionQuality, &data);

  // Write the image
  base::FilePath file_path = GetFile(tab_id);
  base::File file(file_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  file.Write(0, reinterpret_cast<const char*>(data.data()), data.size());

  // Read the image
  base::RunLoop loop1;
  base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> once =
      base::BindOnce([](std::optional<std::vector<uint8_t>> compressed_data) {
        EXPECT_TRUE(compressed_data.has_value());
        EXPECT_FALSE(compressed_data->empty());
        auto bitmap = gfx::JPEGCodec::Decode(compressed_data->data(),
                                             compressed_data->size());
        EXPECT_TRUE(bitmap);
        EXPECT_GT(bitmap->width(), 0);
        EXPECT_GT(bitmap->height(), 0);
      }).Then(loop1.QuitClosure());

  GetInterface().Read(tab_id, std::move(once));
  task_environment_.RunUntilIdle();
  loop1.Run();
}

TEST_F(JpegThumbnailHelperTest, DeleteThumbnail) {
  int tab_id = 0;

  // Create a bitmap
  SkBitmap image;
  ASSERT_TRUE(image.tryAllocN32Pixels(kDimension * kKiB, kDimension));
  SkCanvas canvas(image);
  canvas.drawPaint(SetupPaint());
  image.setImmutable();

  constexpr int kCompressionQuality = 97;
  std::vector<uint8_t> data;
  gfx::JPEGCodec::Encode(image, kCompressionQuality, &data);

  // Write the image
  base::FilePath file_path = GetFile(tab_id);
  base::File file(file_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  file.Write(0, reinterpret_cast<const char*>(data.data()), data.size());

  // Delete the image
  GetInterface().Delete(tab_id);
  task_environment_.RunUntilIdle();

  // Check deletion occurred
  base::FilePath post_delete_file_path = GetFile(tab_id);
  EXPECT_FALSE(base::PathExists(post_delete_file_path));
}

}  // namespace thumbnail
