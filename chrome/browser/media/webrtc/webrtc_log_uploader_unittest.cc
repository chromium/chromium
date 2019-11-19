// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_log_uploader.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kTestTime[] = "time";
const char kTestReportId[] = "report-id";
const char kTestLocalId[] = "local-id";

class WebRtcLogUploaderTest : public testing::Test {
 public:
  WebRtcLogUploaderTest() {}

  bool VerifyNumberOfLines(int expected_lines) {
    std::vector<std::string> lines = GetLinesFromListFile();
    EXPECT_EQ(expected_lines, static_cast<int>(lines.size()));
    return expected_lines == static_cast<int>(lines.size());
  }

  bool VerifyLastLineHasAllInfo() {
    std::string last_line = GetLastLineFromListFile();
    if (last_line.empty())
      return false;
    std::vector<std::string> line_parts = base::SplitString(
        last_line, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_EQ(4u, line_parts.size());
    if (4u != line_parts.size())
      return false;
    // The times (indices 0 and 3) is the time when the info was written to the
    // file which we don't know, so just verify that it's not empty.
    EXPECT_FALSE(line_parts[0].empty());
    EXPECT_STREQ(kTestReportId, line_parts[1].c_str());
    EXPECT_STREQ(kTestLocalId, line_parts[2].c_str());
    EXPECT_FALSE(line_parts[3].empty());
    return true;
  }

  // Verify that the last line contains the correct info for a local storage.
  bool VerifyLastLineHasLocalStorageInfoOnly() {
    std::string last_line = GetLastLineFromListFile();
    if (last_line.empty())
      return false;
    std::vector<std::string> line_parts = base::SplitString(
        last_line, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_EQ(4u, line_parts.size());
    if (4u != line_parts.size())
      return false;
    EXPECT_TRUE(line_parts[0].empty());
    EXPECT_TRUE(line_parts[1].empty());
    EXPECT_STREQ(kTestLocalId, line_parts[2].c_str());
    EXPECT_FALSE(line_parts[3].empty());
    return true;
  }

  // Verify that the last line contains the correct info for an upload.
  bool VerifyLastLineHasUploadInfoOnly() {
    std::string last_line = GetLastLineFromListFile();
    if (last_line.empty())
      return false;
    std::vector<std::string> line_parts = base::SplitString(
        last_line, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_EQ(4u, line_parts.size());
    if (4u != line_parts.size())
      return false;
    EXPECT_FALSE(line_parts[0].empty());
    EXPECT_STREQ(kTestReportId, line_parts[1].c_str());
    EXPECT_TRUE(line_parts[2].empty());
    EXPECT_FALSE(line_parts[3].empty());
    return true;
  }

  bool AddLinesToTestFile(int number_of_lines) {
    base::File test_list_file(test_list_path_,
                              base::File::FLAG_OPEN | base::File::FLAG_APPEND);
    EXPECT_TRUE(test_list_file.IsValid());
    if (!test_list_file.IsValid())
      return false;

    for (int i = 0; i < number_of_lines; ++i) {
      EXPECT_EQ(static_cast<int>(sizeof(kTestTime)) - 1,
                test_list_file.WriteAtCurrentPos(kTestTime,
                                                 sizeof(kTestTime) - 1));
      EXPECT_EQ(1, test_list_file.WriteAtCurrentPos(",", 1));
      EXPECT_EQ(static_cast<int>(sizeof(kTestReportId)) - 1,
                test_list_file.WriteAtCurrentPos(kTestReportId,
                                                 sizeof(kTestReportId) - 1));
      EXPECT_EQ(1, test_list_file.WriteAtCurrentPos(",", 1));
      EXPECT_EQ(static_cast<int>(sizeof(kTestLocalId)) - 1,
                test_list_file.WriteAtCurrentPos(kTestLocalId,
                                                 sizeof(kTestLocalId) - 1));
      EXPECT_EQ(1, test_list_file.WriteAtCurrentPos(",", 1));
      EXPECT_EQ(static_cast<int>(sizeof(kTestTime)) - 1,
                test_list_file.WriteAtCurrentPos(kTestTime,
                                                 sizeof(kTestTime) - 1));
      EXPECT_EQ(1, test_list_file.WriteAtCurrentPos("\n", 1));
    }
    return true;
  }

  std::vector<std::string> GetLinesFromListFile() {
    std::string contents;
    int read = base::ReadFileToString(test_list_path_, &contents);
    EXPECT_GT(read, 0);
    if (read == 0)
      return std::vector<std::string>();
    // Since every line should end with '\n', the last line should be empty. So
    // we expect at least two lines including the final empty. Remove the empty
    // line before returning.
    std::vector<std::string> lines = base::SplitString(
        contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_GT(lines.size(), 1u);
    if (lines.size() < 2)
      return std::vector<std::string>();
    EXPECT_TRUE(lines.back().empty());
    if (!lines.back().empty())
      return std::vector<std::string>();
    lines.pop_back();
    return lines;
  }

  std::string GetLastLineFromListFile() {
    std::vector<std::string> lines = GetLinesFromListFile();
    EXPECT_GT(lines.size(), 0u);
    if (lines.empty())
      return std::string();
    return lines[lines.size() - 1];
  }

  void VerifyRtpDumpInMultipart(const std::string& post_data,
                                const std::string& dump_name,
                                const std::string& dump_content) {
    std::vector<std::string> lines = base::SplitStringUsingSubstr(
        post_data, "\r\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    std::string name_line = "Content-Disposition: form-data; name=\"";
    name_line.append(dump_name);
    name_line.append("\"");
    name_line.append("; filename=\"");
    name_line.append(dump_name);
    name_line.append(".gz\"");

    size_t i = 0;
    for (; i < lines.size(); ++i) {
      if (lines[i] == name_line)
        break;
    }

    // The RTP dump takes 4 lines: content-disposition, content-type, empty
    // line, dump content.
    EXPECT_LT(i, lines.size() - 3);

    EXPECT_EQ("Content-Type: application/gzip", lines[i + 1]);
    EXPECT_EQ("", lines[i + 2]);
    EXPECT_EQ(dump_content, lines[i + 3]);
  }

  static void AddLocallyStoredLogInfoToUploadListFile(
      WebRtcLogUploader* log_uploader,
      const base::FilePath& upload_list_path,
      const std::string& local_log_id) {
    base::RunLoop run_loop;
    log_uploader->background_task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            &WebRtcLogUploader::AddLocallyStoredLogInfoToUploadListFile,
            base::Unretained(log_uploader), upload_list_path, local_log_id),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  void FlushRunLoop() {
    base::RunLoop run_loop;
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     run_loop.QuitClosure());
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  base::FilePath test_list_path_;
};

TEST_F(WebRtcLogUploaderTest, AddLocallyStoredLogInfoToUploadListFile) {
  // Get a temporary filename. We don't want the file to exist to begin with
  // since that's the normal use case, hence the delete.
  ASSERT_TRUE(base::CreateTemporaryFile(&test_list_path_));
  EXPECT_TRUE(base::DeleteFile(test_list_path_, false));
  std::unique_ptr<WebRtcLogUploader> webrtc_log_uploader(
      new WebRtcLogUploader());

  AddLocallyStoredLogInfoToUploadListFile(webrtc_log_uploader.get(),
                                          test_list_path_, kTestLocalId);
  AddLocallyStoredLogInfoToUploadListFile(webrtc_log_uploader.get(),
                                          test_list_path_, kTestLocalId);
  ASSERT_TRUE(VerifyNumberOfLines(2));
  ASSERT_TRUE(VerifyLastLineHasLocalStorageInfoOnly());

  const int expected_line_limit = 50;
  ASSERT_TRUE(AddLinesToTestFile(expected_line_limit - 2));
  ASSERT_TRUE(VerifyNumberOfLines(expected_line_limit));
  ASSERT_TRUE(VerifyLastLineHasAllInfo());

  AddLocallyStoredLogInfoToUploadListFile(webrtc_log_uploader.get(),
                                          test_list_path_, kTestLocalId);
  ASSERT_TRUE(VerifyNumberOfLines(expected_line_limit));
  ASSERT_TRUE(VerifyLastLineHasLocalStorageInfoOnly());

  ASSERT_TRUE(AddLinesToTestFile(10));
  ASSERT_TRUE(VerifyNumberOfLines(60));
  ASSERT_TRUE(VerifyLastLineHasAllInfo());

  AddLocallyStoredLogInfoToUploadListFile(webrtc_log_uploader.get(),
                                          test_list_path_, kTestLocalId);
  ASSERT_TRUE(VerifyNumberOfLines(expected_line_limit));
  ASSERT_TRUE(VerifyLastLineHasLocalStorageInfoOnly());

  webrtc_log_uploader->Shutdown();
  FlushRunLoop();
}

TEST_F(WebRtcLogUploaderTest, AddUploadedLogInfoToUploadListFile) {
  // Get a temporary filename. We don't want the file to exist to begin with
  // since that's the normal use case, hence the delete.
  ASSERT_TRUE(base::CreateTemporaryFile(&test_list_path_));
  EXPECT_TRUE(base::DeleteFile(test_list_path_, false));
  std::unique_ptr<WebRtcLogUploader> webrtc_log_uploader(
      new WebRtcLogUploader());

  AddLocallyStoredLogInfoToUploadListFile(webrtc_log_uploader.get(),
                                          test_list_path_, kTestLocalId);
  ASSERT_TRUE(VerifyNumberOfLines(1));
  ASSERT_TRUE(VerifyLastLineHasLocalStorageInfoOnly());

  webrtc_log_uploader->AddUploadedLogInfoToUploadListFile(
      test_list_path_, kTestLocalId, kTestReportId);
  ASSERT_TRUE(VerifyNumberOfLines(1));
  ASSERT_TRUE(VerifyLastLineHasAllInfo());

  // Use a local ID that should not be found in the list.
  webrtc_log_uploader->AddUploadedLogInfoToUploadListFile(
      test_list_path_, "dummy id", kTestReportId);
  ASSERT_TRUE(VerifyNumberOfLines(2));
  ASSERT_TRUE(VerifyLastLineHasUploadInfoOnly());

  webrtc_log_uploader->Shutdown();
  FlushRunLoop();
}

TEST_F(WebRtcLogUploaderTest, AddRtpDumpsToPostedData) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::unique_ptr<WebRtcLogUploader> webrtc_log_uploader(
      new WebRtcLogUploader());

  std::string post_data;
  webrtc_log_uploader->OverrideUploadWithBufferForTesting(&post_data);

  // Create the fake dump files.
  const base::FilePath incoming_dump = temp_dir.GetPath().AppendASCII("recv");
  const base::FilePath outgoing_dump = temp_dir.GetPath().AppendASCII("send");
  const std::string incoming_dump_content = "dummy incoming";
  const std::string outgoing_dump_content = "dummy outgoing";

  base::WriteFile(incoming_dump,
                  &incoming_dump_content[0],
                  incoming_dump_content.size());
  base::WriteFile(outgoing_dump,
                  &outgoing_dump_content[0],
                  outgoing_dump_content.size());

  WebRtcLogUploader::UploadDoneData upload_done_data;
  upload_done_data.paths.directory = temp_dir.GetPath().AppendASCII("log");

  upload_done_data.paths.incoming_rtp_dump = incoming_dump;
  upload_done_data.paths.outgoing_rtp_dump = outgoing_dump;

  std::unique_ptr<WebRtcLogBuffer> log(new WebRtcLogBuffer());
  log->SetComplete();

  base::RunLoop run_loop;
  webrtc_log_uploader->background_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&WebRtcLogUploader::LoggingStoppedDoUpload,
                     base::Unretained(webrtc_log_uploader.get()),
                     std::move(log), std::make_unique<WebRtcLogMetaDataMap>(),
                     upload_done_data),
      run_loop.QuitClosure());
  run_loop.Run();

  VerifyRtpDumpInMultipart(post_data, "rtpdump_recv", incoming_dump_content);
  VerifyRtpDumpInMultipart(post_data, "rtpdump_send", outgoing_dump_content);

  webrtc_log_uploader->Shutdown();
  FlushRunLoop();
}
