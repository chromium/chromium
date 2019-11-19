// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/single_log_file_log_source.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace system_logs {

class SingleLogFileLogSourceTest : public ::testing::Test {
 public:
  SingleLogFileLogSourceTest() : num_callback_calls_(0) {
    InitializeTestLogDir();
  }

  ~SingleLogFileLogSourceTest() override {
    SingleLogFileLogSource::SetChromeStartTimeForTesting(nullptr);
  }

 protected:
  // Sets up a dummy system log directory.
  void InitializeTestLogDir() {
    ASSERT_TRUE(log_dir_.CreateUniqueTempDir());

    // Create file "messages".
    const base::FilePath messages_path = log_dir_.GetPath().Append("messages");
    base::WriteFile(messages_path, "", 0);
    EXPECT_TRUE(base::PathExists(messages_path)) << messages_path.value();

    // Create file "ui/ui.LATEST".
    const base::FilePath ui_dir_path = log_dir_.GetPath().Append("ui");
    ASSERT_TRUE(base::CreateDirectory(ui_dir_path)) << ui_dir_path.value();

    const base::FilePath ui_latest_path = ui_dir_path.Append("ui.LATEST");
    base::WriteFile(ui_latest_path, "", 0);
    ASSERT_TRUE(base::PathExists(ui_latest_path)) << ui_latest_path.value();
  }

  // Initializes the unit under test, |source_| to read a file from the dummy
  // system log directory.
  void InitializeSource(SingleLogFileLogSource::SupportedSource source_type) {
    source_ = std::make_unique<SingleLogFileLogSource>(source_type);
    source_->log_file_dir_path_ = log_dir_.GetPath();
    log_file_path_ = source_->log_file_dir_path_.Append(source_->source_name());
    ASSERT_TRUE(base::PathExists(log_file_path_)) << log_file_path_.value();
  }

  // Writes/appends (respectively) a string |input| to file indicated by
  // |relative_path| under |log_dir_|.
  bool WriteFile(const base::FilePath& relative_path,
                 const std::string& input) {
    return base::WriteFile(log_dir_.GetPath().Append(relative_path),
                           input.data(),
                           input.size()) == static_cast<int>(input.size());
  }
  bool AppendToFile(const base::FilePath& relative_path,
                    const std::string& input) {
    return base::AppendToFile(log_dir_.GetPath().Append(relative_path),
                              input.data(), input.size());
  }

  // Moves source file to destination path, then creates an empty file at the
  // path of the original source file.
  //
  // |src_relative_path|: Source file path relative to |log_dir_|.
  // |dest_relative_path|: Destination path relative to |log_dir_|.
  bool RotateFile(const base::FilePath& src_relative_path,
                  const base::FilePath& dest_relative_path) {
    return base::Move(log_dir_.GetPath().Append(src_relative_path),
                      log_dir_.GetPath().Append(dest_relative_path)) &&
           WriteFile(src_relative_path, "");
  }

  // Calls source_.Fetch() to start a logs fetch operation. Passes in
  // OnFileRead() as a callback. Runs until Fetch() has completed.
  void FetchFromSource() {
    source_->Fetch(base::Bind(&SingleLogFileLogSourceTest::OnFileRead,
                              base::Unretained(this)));
    task_environment_.RunUntilIdle();
  }

  // Callback for fetching logs from |source_|. Overwrites the previous stored
  // value of |latest_response_|.
  void OnFileRead(std::unique_ptr<SystemLogsResponse> response) {
    ++num_callback_calls_;
    if (response->empty())
      return;

    // Since |source_| represents a single log source, it should only return a
    // single string result.
    EXPECT_EQ(1U, response->size());
    latest_response_ = std::move(response->begin()->second);
  }

  int num_callback_calls() const { return num_callback_calls_; }

  const std::string& latest_response() const { return latest_response_; }

  const base::FilePath& log_file_path() const { return log_file_path_; }

 private:
  // Creates the necessary browser threads.
  content::BrowserTaskEnvironment task_environment_;

  // Unit under test.
  std::unique_ptr<SingleLogFileLogSource> source_;

  // Counts the number of times that |source_| has invoked the callback.
  int num_callback_calls_;

  // Stores the string response returned from |source_| the last time it invoked
  // OnFileRead.
  std::string latest_response_;

  // Temporary dir for creating a dummy log file.
  base::ScopedTempDir log_dir_;

  // Path to the dummy log file in |log_dir_|.
  base::FilePath log_file_path_;

  DISALLOW_COPY_AND_ASSIGN(SingleLogFileLogSourceTest);
};

TEST_F(SingleLogFileLogSourceTest, EmptyFile) {
  InitializeSource(SingleLogFileLogSource::SupportedSource::kMessages);
  FetchFromSource();

  EXPECT_EQ(1, num_callback_calls());
  EXPECT_EQ("", latest_response());
}

TEST_F(SingleLogFileLogSourceTest, SingleRead) {
  InitializeSource(SingleLogFileLogSource::SupportedSource::kUiLatest);

  EXPECT_TRUE(AppendToFile(base::FilePath("ui/ui.LATEST"), "Hello world!\n"));
  FetchFromSource();

  EXPECT_EQ(1, num_callback_calls());
  EXPECT_EQ("Hello world!\n", latest_response());
}

TEST_F(SingleLogFileLogSourceTest, IncrementalReads) {
  InitializeSource(SingleLogFileLogSource::SupportedSource::kMessages);

  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "Hello world!\n"));
  FetchFromSource();

  EXPECT_EQ(1, num_callback_calls());
  EXPECT_EQ("Hello world!\n", latest_response());

  EXPECT_TRUE(AppendToFile(base::FilePath("messages"),
                           "The quick brown fox jumps over the lazy dog\n"));
  FetchFromSource();

  EXPECT_EQ(2, num_callback_calls());
  EXPECT_EQ("The quick brown fox jumps over the lazy dog\n", latest_response());

  EXPECT_TRUE(AppendToFile(base::FilePath("messages"),
                           "Some like it hot.\nSome like it cold\n"));
  FetchFromSource();

  EXPECT_EQ(3, num_callback_calls());
  EXPECT_EQ("Some like it hot.\nSome like it cold\n", latest_response());

  // As a sanity check, read entire contents of file separately to make sure it
  // was written incrementally, and hence read incrementally.
  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(log_file_path(), &file_contents));
  EXPECT_EQ(
      "Hello world!\nThe quick brown fox jumps over the lazy dog\n"
      "Some like it hot.\nSome like it cold\n",
      file_contents);
}

// The log files read by SingleLogFileLogSource are not expected to be
// overwritten. This test is just to ensure that the SingleLogFileLogSource
// class is robust enough not to break in the event of an overwrite.
TEST_F(SingleLogFileLogSourceTest, FileOverwrite) {
  InitializeSource(SingleLogFileLogSource::SupportedSource::kUiLatest);

  EXPECT_TRUE(AppendToFile(base::FilePath("ui/ui.LATEST"), "0123456789\n"));
  FetchFromSource();

  EXPECT_EQ(1, num_callback_calls());
  EXPECT_EQ("0123456789\n", latest_response());

  // Overwrite the file.
  EXPECT_TRUE(WriteFile(base::FilePath("ui/ui.LATEST"), "abcdefg\n"));
  FetchFromSource();

  // Should re-read from the beginning.
  EXPECT_EQ(2, num_callback_calls());
  EXPECT_EQ("abcdefg\n", latest_response());

  // Append to the file to make sure incremental read still works.
  EXPECT_TRUE(AppendToFile(base::FilePath("ui/ui.LATEST"), "hijk\n"));
  FetchFromSource();

  EXPECT_EQ(3, num_callback_calls());
  EXPECT_EQ("hijk\n", latest_response());

  // Overwrite again, this time with a longer length than the existing file.
  // Previous contents:
  //   abcdefg~hijk~     <-- "~" is a single-char representation of newline.
  // New contents:
  //   lmnopqrstuvwxyz~  <-- excess text beyond end of prev contents: "yz~"
  EXPECT_TRUE(WriteFile(base::FilePath("ui/ui.LATEST"), "lmnopqrstuvwxyz\n"));
  FetchFromSource();

  EXPECT_EQ(4, num_callback_calls());
  EXPECT_EQ("yz\n", latest_response());
}

TEST_F(SingleLogFileLogSourceTest, IncompleteLines) {
  InitializeSource(SingleLogFileLogSource::SupportedSource::kMessages);

  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "0123456789"));
  FetchFromSource();

  EXPECT_EQ(1, num_callback_calls());
  EXPECT_EQ("", latest_response());

  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "abcdefg"));
  FetchFromSource();

  EXPECT_EQ(2, num_callback_calls());
  EXPECT_EQ("", latest_response());

  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "hijk\n"));
  FetchFromSource();

  EXPECT_EQ(3, num_callback_calls());
  // All the previously written text should be read this time.
  EXPECT_EQ("0123456789abcdefghijk\n", latest_response());

  // Check ability to read whole lines while leaving the remainder for later.
  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "Hello world\n"));
  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "Goodbye world"));
  FetchFromSource();

  EXPECT_EQ(4, num_callback_calls());
  EXPECT_EQ("Hello world\n", latest_response());

  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "\n"));
  FetchFromSource();

  EXPECT_EQ(5, num_callback_calls());
  EXPECT_EQ("Goodbye world\n", latest_response());
}

TEST_F(SingleLogFileLogSourceTest, HandleLogFileRotation) {
  InitializeSource(SingleLogFileLogSource::SupportedSource::kMessages);

  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "1st log file\n"));
  FetchFromSource();
  EXPECT_EQ(1, num_callback_calls());
  EXPECT_EQ("1st log file\n", latest_response());

  // Rotate file. Make sure the rest of the old file and the contents of the new
  // file are both read.
  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "More 1st log file\n"));
  EXPECT_TRUE(
      RotateFile(base::FilePath("messages"), base::FilePath("messages.1")));
  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "2nd log file\n"));

  FetchFromSource();
  EXPECT_EQ(2, num_callback_calls());
  EXPECT_EQ("More 1st log file\n2nd log file\n", latest_response());

  // Rotate again, but this time omit the newline before rotating.
  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "No newline here..."));
  EXPECT_TRUE(
      RotateFile(base::FilePath("messages"), base::FilePath("messages.1")));
  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "3rd log file\n"));
  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "Also no newline here"));

  FetchFromSource();
  EXPECT_EQ(3, num_callback_calls());
  // Make sure the rotation didn't break anything: the last part of the new file
  // does not end with a newline; thus the new file should not be read.
  EXPECT_EQ("No newline here...3rd log file\n", latest_response());

  // Finish the previous read attempt by adding the missing newline.
  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "...yet\n"));
  FetchFromSource();
  EXPECT_EQ(4, num_callback_calls());
  EXPECT_EQ("Also no newline here...yet\n", latest_response());
}

}  // namespace system_logs
