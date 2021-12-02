// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "chrome/browser/icon_transcoder/svg_icon_transcoder.h"
#include "chrome/test/base/browser_with_test_window_test.h"

constexpr char kSvgData[] =
    "<svg width='20px' height='20px' viewBox='0 0 24 24' fill='rgb(95,99,104)' "
    "xmlns='http://www.w3.org/2000/svg'><path d='M0 0h24v24H0V0z' "
    "fill='none'/><path d='M19 19H5V5h7V3H5c-1.11 0-2 .9-2 2v14c0 1.1.89 2 2 "
    "2h14c1.1 0 2-.9 2-2v-7h-2v7zM14 3v2h3.59l-9.83 9.83 1.41 1.41L19 "
    "6.41V10h2V3h-7z'/></svg>";

constexpr char kInvalidSvgData[] = "<svg garbled not really useful>bad</svg>";
constexpr char kGarbageData[] = "this is not even svg-like";
constexpr gfx::Size kSize(48, 48);

class SvgIconTranscoderTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    svg_path_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("icon.svg"));
    png_path_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("icon.png"));
    svg_icon_transcoder_ = std::make_unique<apps::SvgIconTranscoder>(profile());
  }

  void TearDown() override {
    svg_icon_transcoder_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  void ExpectTranscodeSuccess(std::string icon_data) {
    EXPECT_FALSE(icon_data.empty());
    compressed_icon_data_ = std::move(icon_data);
  }

  void ExpectTranscodeFailure(std::string icon_data) {
    EXPECT_TRUE(icon_data.empty());
    compressed_icon_data_ = std::move(icon_data);
  }

  void ExpectSavedIcon(base::FilePath path) {
    EXPECT_TRUE(base::PathExists(path));
    std::string saved_data;
    EXPECT_TRUE(base::ReadFileToString(path, &saved_data));
    EXPECT_EQ(saved_data, compressed_icon_data_);
  }

  void WriteIconData(const base::FilePath& path,
                     const std::string& icon_data) const {
    EXPECT_TRUE(base::WriteFile(path, icon_data));
  }

  void WaitForTranscodeDone() { task_environment()->RunUntilIdle(); }

  const base::FilePath png_path() const { return png_path_; }
  const base::FilePath svg_path() const { return svg_path_; }

 protected:
  std::string compressed_icon_data_;
  std::unique_ptr<apps::SvgIconTranscoder> svg_icon_transcoder_;
  base::FilePath svg_path_;
  base::FilePath png_path_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(SvgIconTranscoderTest, TranscodeFromDataSuccessNoSave) {
  svg_icon_transcoder_->Transcode(
      std::string(kSvgData), base::FilePath(), kSize,
      base::BindOnce(&SvgIconTranscoderTest::ExpectTranscodeSuccess,
                     base::Unretained(this)));
  WaitForTranscodeDone();
}

TEST_F(SvgIconTranscoderTest, TranscodeFromDataSuccessAndSave) {
  svg_icon_transcoder_->Transcode(
      std::string(kSvgData), png_path(), kSize,
      base::BindOnce(&SvgIconTranscoderTest::ExpectTranscodeSuccess,
                     base::Unretained(this)));
  WaitForTranscodeDone();
}

TEST_F(SvgIconTranscoderTest, TranscodeFromInvalid) {
  svg_icon_transcoder_->Transcode(
      std::string(kInvalidSvgData), base::FilePath(), kSize,
      base::BindOnce(&SvgIconTranscoderTest::ExpectTranscodeFailure,
                     base::Unretained(this)));
  WaitForTranscodeDone();
}

TEST_F(SvgIconTranscoderTest, TranscodeFromGarbage) {
  svg_icon_transcoder_->Transcode(
      std::string(kGarbageData), base::FilePath(), kSize,
      base::BindOnce(&SvgIconTranscoderTest::ExpectTranscodeFailure,
                     base::Unretained(this)));
  WaitForTranscodeDone();
}

TEST_F(SvgIconTranscoderTest, TranscodeFromFileSuccessNoSave) {
  WriteIconData(svg_path_, kSvgData);

  svg_icon_transcoder_->Transcode(
      svg_path(), base::FilePath(), kSize,
      base::BindOnce(&SvgIconTranscoderTest::ExpectTranscodeSuccess,
                     base::Unretained(this)));
  WaitForTranscodeDone();
}

TEST_F(SvgIconTranscoderTest, TranscodeFromFileSuccessAndSave) {
  WriteIconData(svg_path_, kSvgData);

  svg_icon_transcoder_->Transcode(
      svg_path(), png_path(), kSize,
      base::BindOnce(&SvgIconTranscoderTest::ExpectTranscodeSuccess,
                     base::Unretained(this)));
  WaitForTranscodeDone();
}
