// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/icon_transcoder/svg_icon_transcoder.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_test.h"

constexpr char kSvgData[] =
    "<svg width='20px' height='20px' viewBox='0 0 24 24' fill='rgb(95,99,104)' "
    "xmlns='http://www.w3.org/2000/svg'><path d='M0 0h24v24H0V0z' "
    "fill='none'/><path d='M19 19H5V5h7V3H5c-1.11 0-2 .9-2 2v14c0 1.1.89 2 2 "
    "2h14c1.1 0 2-.9 2-2v-7h-2v7zM14 3v2h3.59l-9.83 9.83 1.41 1.41L19 "
    "6.41V10h2V3h-7z'/></svg>";

constexpr char kInvalidSvgData[] = "<svg garbled not really useful>bad</svg>";
constexpr char kGarbageData[] = "this is not even svg-like";
constexpr gfx::Size kSize(48, 48);

class SvgIconTranscoderTest : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    svg_path_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("icon.svg"));
    png_path_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("icon.png"));
  }

  void TearDownInProcessBrowserTestFixture() override {
    svg_icon_transcoder_.reset();
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  apps::SvgIconTranscoder* svg_icon_transcoder() {
    if (!svg_icon_transcoder_) {
      svg_icon_transcoder_ =
          std::make_unique<apps::SvgIconTranscoder>(browser()->profile());
    }
    return svg_icon_transcoder_.get();
  }

  void ExpectTranscodeSuccess(base::OnceClosure done_closure,
                              std::string icon_data) {
    EXPECT_FALSE(icon_data.empty());
    compressed_icon_data_ = std::move(icon_data);
    std::move(done_closure).Run();
  }

  void ExpectTranscodeFailure(base::OnceClosure done_closure,
                              std::string icon_data) {
    EXPECT_TRUE(icon_data.empty());
    compressed_icon_data_ = std::move(icon_data);
    std::move(done_closure).Run();
  }

  void ExpectSavedIcon(bool saved) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_EQ(base::PathExists(png_path()), saved);
    if (saved) {
      std::string saved_data;
      ASSERT_TRUE(base::ReadFileToString(png_path(), &saved_data));
      EXPECT_EQ(saved_data, compressed_icon_data_);
    }
  }

  void WriteIconData(const base::FilePath& path,
                     const std::string& icon_data) const {
    base::RunLoop run_loop;
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            [](const base::FilePath& path, const std::string& icon_data) {
              EXPECT_TRUE(base::WriteFile(path, icon_data));
            },
            path, icon_data),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  const base::FilePath png_path() const { return png_path_; }
  const base::FilePath svg_path() const { return svg_path_; }

 protected:
  std::string compressed_icon_data_;
  std::unique_ptr<apps::SvgIconTranscoder> svg_icon_transcoder_;
  base::FilePath svg_path_;
  base::FilePath png_path_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(SvgIconTranscoderTest, TranscodeFromDataSuccessNoSave) {
  base::RunLoop run_loop;
  svg_icon_transcoder()->Transcode(
      std::string(kSvgData), base::FilePath(), kSize,
      base::BindOnce(&SvgIconTranscoderTest::ExpectTranscodeSuccess,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  ExpectSavedIcon(false);
}

IN_PROC_BROWSER_TEST_F(SvgIconTranscoderTest, TranscodeFromDataSuccessAndSave) {
  base::RunLoop run_loop;
  svg_icon_transcoder()->Transcode(
      std::string(kSvgData), png_path(), kSize,
      base::BindOnce(&SvgIconTranscoderTest::ExpectTranscodeSuccess,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  ExpectSavedIcon(true);
}

IN_PROC_BROWSER_TEST_F(SvgIconTranscoderTest, TranscodeFromInvalid) {
  base::RunLoop run_loop;
  svg_icon_transcoder()->Transcode(
      std::string(kInvalidSvgData), base::FilePath(), kSize,
      base::BindOnce(&SvgIconTranscoderTest::ExpectTranscodeFailure,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  ExpectSavedIcon(false);
}

IN_PROC_BROWSER_TEST_F(SvgIconTranscoderTest, TranscodeFromGarbage) {
  base::RunLoop run_loop;
  svg_icon_transcoder()->Transcode(
      std::string(kGarbageData), base::FilePath(), kSize,
      base::BindOnce(&SvgIconTranscoderTest::ExpectTranscodeFailure,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  ExpectSavedIcon(false);
}

IN_PROC_BROWSER_TEST_F(SvgIconTranscoderTest, TranscodeFromFileSuccessNoSave) {
  base::RunLoop run_loop;
  WriteIconData(svg_path_, kSvgData);

  svg_icon_transcoder()->Transcode(
      svg_path(), base::FilePath(), kSize,
      base::BindOnce(&SvgIconTranscoderTest::ExpectTranscodeSuccess,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  ExpectSavedIcon(false);
}

IN_PROC_BROWSER_TEST_F(SvgIconTranscoderTest, TranscodeFromFileSuccessAndSave) {
  base::RunLoop run_loop;
  WriteIconData(svg_path_, kSvgData);

  svg_icon_transcoder()->Transcode(
      svg_path(), png_path(), kSize,
      base::BindOnce(&SvgIconTranscoderTest::ExpectTranscodeSuccess,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  ExpectSavedIcon(true);
}
