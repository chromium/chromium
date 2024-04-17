// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/screenshot_data_collector.h"

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted_memory.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/base/data_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace {

// 100px by 100px red image.
constexpr char kTestImageBase64[] =
    "data:image/jpeg;base64,/9j/4AAQSkZJRgABAQAAAQABAAD/"
    "2wBDAAMCAgMCAgMDAwMEAwMEBQgFBQQEBQoHBwYIDAoMDAsKCwsNDhIQDQ4RDgsLEBYQERMUFR"
    "UVDA8XGBYUGBIUFRT/"
    "2wBDAQMEBAUEBQkFBQkUDQsNFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFB"
    "QUFBQUFBQUFBQUFBT/wAARCABkAGQDASIAAhEBAxEB/8QAFQABAQAAAAAAAAAAAAAAAAAAAAj/"
    "xAAUEAEAAAAAAAAAAAAAAAAAAAAA/8QAFgEBAQEAAAAAAAAAAAAAAAAAAAcJ/"
    "8QAFBEBAAAAAAAAAAAAAAAAAAAAAP/"
    "aAAwDAQACEQMRAD8AnQBDGqYAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD/2Q==";

// Reads in the file at `path`. Sets `file_contents` to be the contents
// of that file.
void ReadFileContents(const base::FilePath& path, std::string& file_contents) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::ReadFileToString(path, &file_contents));
}

}  // namespace

class ScreenshotDataCollectorTest : public ::testing::Test {
 public:
  ScreenshotDataCollectorTest() = default;

  ScreenshotDataCollectorTest(const ScreenshotDataCollectorTest&) = delete;
  ScreenshotDataCollectorTest& operator=(const ScreenshotDataCollectorTest&) =
      delete;

 protected:
  void SetUp() override {
    base::ScopedAllowBlockingForTesting allow_blocking;
    // Converts base64-encoded data into other forms.
    const GURL url(kTestImageBase64);
    std::string mime_type, charset, data;
    EXPECT_TRUE(net::DataURL::Parse(url, &mime_type, &charset, &data));
    bitmap_ = *gfx::JPEGCodec::Decode(
                   reinterpret_cast<const unsigned char*>(data.c_str()),
                   data.length())
                   .get();

    webrtc::DesktopSize size(bitmap_.width(), bitmap_.height());
    frame_ = std::make_unique<webrtc::BasicDesktopFrame>(std::move(size));
    std::memcpy(frame_->data(), bitmap_.getAddr32(0, 0),
                bitmap_.rowBytes() * bitmap_.height());
    jpeg_data_ = base::MakeRefCounted<base::RefCountedString>(std::move(data));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void TearDown() override {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.IsValid());
    ASSERT_TRUE(temp_dir_.Delete());
  }

  base::FilePath GetTempDirForOutput() { return temp_dir_.GetPath(); }

  SkBitmap bitmap_;
  std::unique_ptr<webrtc::DesktopFrame> frame_;
  scoped_refptr<base::RefCountedString> jpeg_data_;
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ScreenshotDataCollectorTest, DesktopFrameToBase64JPEGConversion) {
  std::string jpeg_base64;
  ScreenshotDataCollector data_collector;
  data_collector.ConvertDesktopFrameToBase64JPEG(std::move(frame_),
                                                 jpeg_base64);
  EXPECT_EQ(jpeg_base64, kTestImageBase64);
}

TEST_F(ScreenshotDataCollectorTest, SetAndExportImage) {
  ScreenshotDataCollector data_collector;
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_collect_data;
  const base::FilePath output_dir = GetTempDirForOutput();
  data_collector.SetScreenshotBase64(kTestImageBase64);

  data_collector.ExportCollectedDataWithPII(
      /*pii_types_to_keep=*/{}, output_dir,
      /*task_runner_for_redaction_tool=*/nullptr,
      /*redaction_tool_container=*/nullptr,
      test_future_collect_data.GetCallback());
  EXPECT_EQ(test_future_collect_data.Get(), std::nullopt);
  // Compares the output in the file with the target value.
  std::string output_file_contents;
  ReadFileContents(output_dir.Append(FILE_PATH_LITERAL("screenshot"))
                       .AddExtension(FILE_PATH_LITERAL(".jpg")),
                   output_file_contents);
  EXPECT_EQ(output_file_contents, jpeg_data_->as_string());
}
