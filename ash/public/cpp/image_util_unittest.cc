// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/image_util.h"

#include <string>

#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/test/in_process_image_decoder.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {
namespace {

// How long to wait for a DecodeImageCallback to be run before a test times
// out and fails. Prevents the unit tests from hanging indefinitely (or for a
// very long time) if there's a bug in the code where the callback is not being
// run.
constexpr base::TimeDelta kDecodeImageTimeout = base::Seconds(3);

gfx::ImageSkia CreateTestImage(int width, int height, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

using EncodingFunction =
    base::RepeatingCallback<bool(const SkBitmap&, std::vector<unsigned char>*)>;

void EncodeImage(const gfx::ImageSkia& image,
                 std::string* output,
                 EncodingFunction encoding_fn) {
  std::vector<unsigned char> encoded_data;
  ASSERT_TRUE(encoding_fn.Run(*image.bitmap(), &encoded_data));
  output->assign(reinterpret_cast<const char*>(encoded_data.data()),
                 encoded_data.size());
}

std::string EncodeToString(const gfx::ImageSkia& image,
                           EncodingFunction encoding_fn) {
  std::string output;
  EncodeImage(image, &output, encoding_fn);
  return output;
}

std::string EncodeAsJpeg(const gfx::ImageSkia& image) {
  return EncodeToString(
      image, base::BindRepeating([](const SkBitmap& bitmap,
                                    std::vector<unsigned char>* encoded_data) {
        return gfx::JPEGCodec::Encode(bitmap, /*quality=*/90, encoded_data);
      }));
}

std::string EncodeAsPng(const gfx::ImageSkia& image) {
  return EncodeToString(
      image, base::BindRepeating([](const SkBitmap& bitmap,
                                    std::vector<unsigned char>* encoded_data) {
        return gfx::PNGCodec::EncodeBGRASkBitmap(
            bitmap, /*discard_transparency=*/false, encoded_data);
      }));
}

}  // namespace

class ImageUtilTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir()); }

  base::FilePath CreateFilePath(base::FilePath::StringPieceType file_name) {
    return scoped_temp_dir_.GetPath().Append(file_name);
  }

  image_util::DecodeImageCallback CreateDecodeImageCallback(
      base::RunLoop& run_loop,
      gfx::ImageSkia& image_out) {
    return base::BindLambdaForTesting(
        [&image_out, &run_loop](const gfx::ImageSkia& result) {
          image_out = result;
          run_loop.Quit();
        });
  }

  gfx::ImageSkia DecodeImageFile(base::FilePath::StringPieceType file_name,
                                 data_decoder::mojom::ImageCodec codec) {
    base::test::ScopedRunLoopTimeout timeout(FROM_HERE, kDecodeImageTimeout);
    base::RunLoop run_loop;
    gfx::ImageSkia image_out;
    image_util::DecodeImageFile(CreateDecodeImageCallback(run_loop, image_out),
                                CreateFilePath(file_name), codec);
    run_loop.Run();
    return image_out;
  }

  gfx::ImageSkia DecodeImageData(data_decoder::mojom::ImageCodec codec,
                                 const std::string& data) {
    base::test::ScopedRunLoopTimeout timeout(FROM_HERE, kDecodeImageTimeout);
    base::RunLoop run_loop;
    gfx::ImageSkia image_out;
    image_util::DecodeImageData(CreateDecodeImageCallback(run_loop, image_out),
                                codec, data);
    run_loop.Run();
    return image_out;
  }

  base::test::TaskEnvironment task_environment_;
  InProcessImageDecoder decoder_;
  base::ScopedTempDir scoped_temp_dir_;
};

TEST_F(ImageUtilTest, DecodeImageDataDefaultCodec) {
  gfx::ImageSkia original_image = CreateTestImage(200, 100, SK_ColorYELLOW);
  gfx::ImageSkia decoded_image = DecodeImageData(
      data_decoder::mojom::ImageCodec::kDefault, EncodeAsJpeg(original_image));
  EXPECT_TRUE(gfx::test::AreImagesEqual(gfx::Image(decoded_image),
                                        gfx::Image(original_image)));
}

TEST_F(ImageUtilTest, DecodeImageDataPng) {
  gfx::ImageSkia original_image = CreateTestImage(200, 100, SK_ColorYELLOW);
  gfx::ImageSkia decoded_image = DecodeImageData(
      data_decoder::mojom::ImageCodec::kPng, EncodeAsPng(original_image));
  EXPECT_TRUE(gfx::test::AreImagesEqual(gfx::Image(decoded_image),
                                        gfx::Image(original_image)));
}

TEST_F(ImageUtilTest, DecodeImageDataFailsForInvalidData) {
  gfx::ImageSkia decoded_image =
      DecodeImageData(data_decoder::mojom::ImageCodec::kDefault, "gibberish");
  EXPECT_TRUE(decoded_image.isNull());
}

TEST_F(ImageUtilTest, DecodeImageFileDefaultCodec) {
  gfx::ImageSkia original_image = CreateTestImage(200, 100, SK_ColorYELLOW);
  ASSERT_TRUE(base::WriteFile(CreateFilePath("test_image.jpg"),
                              EncodeAsJpeg(original_image)));
  gfx::ImageSkia decoded_image = DecodeImageFile(
      "test_image.jpg", data_decoder::mojom::ImageCodec::kDefault);
  EXPECT_TRUE(gfx::test::AreImagesEqual(gfx::Image(decoded_image),
                                        gfx::Image(original_image)));
}

TEST_F(ImageUtilTest, DecodeImageFilePng) {
  gfx::ImageSkia original_image = CreateTestImage(200, 100, SK_ColorYELLOW);
  ASSERT_TRUE(base::WriteFile(CreateFilePath("test_image.png"),
                              EncodeAsPng(original_image)));
  gfx::ImageSkia decoded_image =
      DecodeImageFile("test_image.png", data_decoder::mojom::ImageCodec::kPng);
  EXPECT_TRUE(gfx::test::AreImagesEqual(gfx::Image(decoded_image),
                                        gfx::Image(original_image)));
}

TEST_F(ImageUtilTest, DecodeImageFileFailsForInvalidData) {
  ASSERT_TRUE(base::WriteFile(CreateFilePath("test_image.jpg"), "gibberish"));
  gfx::ImageSkia decoded_image = DecodeImageFile(
      "test_image.jpg", data_decoder::mojom::ImageCodec::kDefault);
}

TEST_F(ImageUtilTest, DecodeImageFileFailsForMissingFile) {
  gfx::ImageSkia decoded_image = DecodeImageFile(
      "test_image.jpg", data_decoder::mojom::ImageCodec::kDefault);
  EXPECT_TRUE(decoded_image.isNull());
}

TEST_F(ImageUtilTest, DecodeImageFileMultipleFiles) {
  gfx::ImageSkia original_image_1 = CreateTestImage(200, 100, SK_ColorYELLOW);
  gfx::ImageSkia original_image_2 = CreateTestImage(100, 200, SK_ColorCYAN);
  ASSERT_TRUE(base::WriteFile(CreateFilePath("test_image_1.jpg"),
                              EncodeAsJpeg(original_image_1)));
  ASSERT_TRUE(base::WriteFile(CreateFilePath("test_image_2.jpg"),
                              EncodeAsJpeg(original_image_2)));
  gfx::ImageSkia decoded_image_1 = DecodeImageFile(
      "test_image_1.jpg", data_decoder::mojom::ImageCodec::kDefault);
  gfx::ImageSkia decoded_image_2 = DecodeImageFile(
      "test_image_2.jpg", data_decoder::mojom::ImageCodec::kDefault);
  EXPECT_TRUE(gfx::test::AreImagesEqual(gfx::Image(decoded_image_1),
                                        gfx::Image(original_image_1)));
  EXPECT_TRUE(gfx::test::AreImagesEqual(gfx::Image(decoded_image_2),
                                        gfx::Image(original_image_2)));
}

TEST_F(ImageUtilTest, DecodeImageFileSameFileMultipleTimes) {
  gfx::ImageSkia original_image = CreateTestImage(200, 100, SK_ColorYELLOW);
  ASSERT_TRUE(base::WriteFile(CreateFilePath("test_image.jpg"),
                              EncodeAsJpeg(original_image)));
  gfx::ImageSkia decoded_image = DecodeImageFile(
      "test_image.jpg", data_decoder::mojom::ImageCodec::kDefault);
  EXPECT_TRUE(gfx::test::AreImagesEqual(gfx::Image(decoded_image),
                                        gfx::Image(original_image)));

  original_image = CreateTestImage(100, 200, SK_ColorCYAN);
  ASSERT_TRUE(base::WriteFile(CreateFilePath("test_image.jpg"),
                              EncodeAsJpeg(original_image)));
  decoded_image = DecodeImageFile("test_image.jpg",
                                  data_decoder::mojom::ImageCodec::kDefault);
  EXPECT_TRUE(gfx::test::AreImagesEqual(gfx::Image(decoded_image),
                                        gfx::Image(original_image)));
}

}  // namespace ash
