// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/desktop_capture.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/png_codec.h"

namespace glic {

TEST(GlicScreenshotCapturerTest, MANUAL_ConvertFrameToJpeg) {
  base::FilePath input_path =
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII("glic_convert_frame_to_jpeg.png");
  std::optional<std::vector<uint8_t>> input_data =
      base::ReadFileToBytes(input_path);
  ASSERT_TRUE(input_data.has_value());

  SkBitmap input_bitmap = gfx::PNGCodec::Decode(input_data.value());
  ASSERT_FALSE(input_bitmap.drawsNothing());

  webrtc::DesktopSize size(input_bitmap.width(), input_bitmap.height());
  auto frame = std::make_unique<webrtc::BasicDesktopFrame>(size);
  UNSAFE_TODO(memcpy(frame->data(), input_bitmap.getPixels(),
                     input_bitmap.computeByteSize()));
  frame->mutable_updated_region()->SetRect(webrtc::DesktopRect::MakeSize(size));

  std::vector<uint8_t> jpeg_data =
      GlicScreenshotCapturer::ConvertFrameToJpegForTesting(std::move(frame));
  EXPECT_FALSE(jpeg_data.empty());
}

}  // namespace glic
