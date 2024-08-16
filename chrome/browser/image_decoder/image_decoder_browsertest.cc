// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/image_decoder/image_decoder.h"

#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace {

std::vector<uint8_t> GetValidPngData() {
  // 1x1 PNG. Does not get much smaller than this.
  static const char kPngData[] =
      "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52"
      "\x00\x00\x00\x01\x00\x00\x00\x01\x08\x02\x00\x00\x00\x90\x77\x53"
      "\xde\x00\x00\x00\x0c\x49\x44\x41\x54\x08\xd7\x63\xf8\xff\xff\x3f"
      "\x00\x05\xfe\x02\xfe\xdc\xcc\x59\xe7\x00\x00\x00\x00\x49\x45\x4e"
      "\x44\xae\x42\x60\x82";
  // Need to specify the buffer size because it contains NULs.
  return std::vector<uint8_t>(kPngData, kPngData + sizeof(kPngData) - 1);
}

std::vector<uint8_t> GetValidJpgData() {
  // 1x1 JPG created from the 1x1 PNG above.
  static const char kJpgData[] =
      "\xFF\xD8\xFF\xE0\x00\x10\x4A\x46\x49\x46\x00\x01\x01\x01\x00\x48"
      "\x00\x48\x00\x00\xFF\xDB\x00\x43\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
      "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
      "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
      "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
      "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xDB\x00\x43\x01\xFF\xFF"
      "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
      "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
      "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
      "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xC0"
      "\x00\x11\x08\x00\x01\x00\x01\x03\x01\x22\x00\x02\x11\x01\x03\x11"
      "\x01\xFF\xC4\x00\x15\x00\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x03\xFF\xC4\x00\x14\x10\x01\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xC4"
      "\x00\x14\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\xFF\xC4\x00\x14\x11\x01\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xDA\x00\x0C\x03\x01"
      "\x00\x02\x11\x03\x11\x00\x3F\x00\xA0\x00\xFF\xD9";
  // Need to specify the buffer size because it contains NULs.
  return std::vector<uint8_t>(kJpgData, kJpgData + sizeof(kJpgData) - 1);
}

class TestImageRequest : public ImageDecoder::ImageRequest {
 public:
  explicit TestImageRequest(base::OnceClosure quit_closure)
      : decode_succeeded_(false),
        quit_closure_(std::move(quit_closure)),
        quit_called_(false) {}

  TestImageRequest(const TestImageRequest&) = delete;
  TestImageRequest& operator=(const TestImageRequest&) = delete;

  ~TestImageRequest() override {
    if (!quit_called_) {
      std::move(quit_closure_).Run();
    }
  }

  bool decode_succeeded() const { return decode_succeeded_; }

  const SkBitmap& get_bitmap() const { return bitmap_; }

 private:
  void OnImageDecoded(const SkBitmap& decoded_image) override {
    decode_succeeded_ = true;
    bitmap_ = decoded_image;
    Quit();
  }

  void OnDecodeImageFailed() override { Quit(); }

  void Quit() {
    EXPECT_FALSE(quit_called_);
    quit_called_ = true;
    std::move(quit_closure_).Run();
  }

  bool decode_succeeded_;

  base::OnceClosure quit_closure_;
  bool quit_called_;
  SkBitmap bitmap_;
};

}  // namespace

class ImageDecoderBrowserTest : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, Basic) {
  base::RunLoop run_loop;
  TestImageRequest test_request(run_loop.QuitClosure());
  ImageDecoder::Start(&test_request, std::vector<uint8_t>());
  run_loop.Run();
  EXPECT_FALSE(test_request.decode_succeeded());
}

#if BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, BasicDecodeWithOptionsString) {
  base::RunLoop run_loop;
  TestImageRequest test_request(run_loop.QuitClosure());
  const std::vector<uint8_t> data = GetValidPngData();
  ImageDecoder::StartWithOptions(&test_request,
                                 std::string(data.begin(), data.end()),
                                 ImageDecoder::PNG_CODEC,
                                 /*shrink_to_fit=*/false);
  run_loop.Run();
  EXPECT_TRUE(test_request.decode_succeeded());
}

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, RobustJpegCodecWithJpegData) {
  base::RunLoop run_loop;
  TestImageRequest test_request(run_loop.QuitClosure());
  ImageDecoder::StartWithOptions(
      &test_request, GetValidJpgData(), ImageDecoder::DEFAULT_CODEC,
      /*shrink_to_fit=*/false, /*desired_image_frame_size=*/gfx::Size());
  run_loop.Run();
  EXPECT_TRUE(test_request.decode_succeeded());
}

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, RobustPngCodecWithPngData) {
  base::RunLoop run_loop;
  TestImageRequest test_request(run_loop.QuitClosure());
  ImageDecoder::StartWithOptions(
      &test_request, GetValidPngData(), ImageDecoder::PNG_CODEC,
      /*shrink_to_fit=*/false, /*desired_image_frame_size=*/gfx::Size());
  run_loop.Run();
  EXPECT_TRUE(test_request.decode_succeeded());
}

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, RobustPngCodecWithJpegData) {
  base::RunLoop run_loop;
  TestImageRequest test_request(run_loop.QuitClosure());
  ImageDecoder::StartWithOptions(
      &test_request, GetValidJpgData(), ImageDecoder::PNG_CODEC,
      /*shrink_to_fit=*/false, /*desired_image_frame_size=*/gfx::Size());
  run_loop.Run();
  // Should fail with JPEG data because only PNG data is allowed.
  EXPECT_FALSE(test_request.decode_succeeded());
}

#endif  // BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, BasicDecode) {
  base::RunLoop run_loop;
  TestImageRequest test_request(run_loop.QuitClosure());
  ImageDecoder::Start(&test_request, GetValidPngData());
  run_loop.Run();
  EXPECT_TRUE(test_request.decode_succeeded());
}

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, BasicDecodeString) {
  base::RunLoop run_loop;
  TestImageRequest test_request(run_loop.QuitClosure());
  const std::vector<uint8_t> data = GetValidPngData();
  ImageDecoder::Start(&test_request, std::string(data.begin(), data.end()));
  run_loop.Run();
  EXPECT_TRUE(test_request.decode_succeeded());
  EXPECT_EQ(test_request.get_bitmap().height(), 1);
  EXPECT_EQ(test_request.get_bitmap().width(), 1);
  EXPECT_EQ(test_request.get_bitmap().getColor(0, 0), 0xffffffffUL);
}

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, BasicDecodeStringJpeg) {
  base::RunLoop run_loop;
  TestImageRequest test_request(run_loop.QuitClosure());
  const std::vector<uint8_t> data = GetValidJpgData();
  ImageDecoder::Start(&test_request, std::string(data.begin(), data.end()));
  run_loop.Run();
  EXPECT_TRUE(test_request.decode_succeeded());
  EXPECT_EQ(test_request.get_bitmap().height(), 1);
  EXPECT_EQ(test_request.get_bitmap().width(), 1);
  EXPECT_EQ(test_request.get_bitmap().getColor(0, 0), 0xffffffffUL);
}

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, StartAndDestroy) {
  base::RunLoop run_loop;
  std::unique_ptr<TestImageRequest> test_request(
      new TestImageRequest(run_loop.QuitClosure()));
  ImageDecoder::Start(test_request.get(), std::vector<uint8_t>());
  test_request.reset();
  run_loop.Run();
}
