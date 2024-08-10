// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/file_opening_job.h"

#include <memory>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_command_line.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_analysis_request.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class FileOpeningJobTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void OnGotFileData(std::unique_ptr<FileAnalysisRequest> request,
                     BinaryUploadService::Result result,
                     BinaryUploadService::Request::Data data) {
    EXPECT_EQ(BinaryUploadService::Result::SUCCESS, result);
    EXPECT_TRUE(data.contents.empty());
    EXPECT_FALSE(data.mime_type.empty());
    EXPECT_EQ(3u, data.size);
    // printf "foo" | sha256sum |  tr '[:lower:]' '[:upper:]'
    EXPECT_EQ(
        "2C26B46B68FFC68FF99B453C1D30413413422D706483BFA0F98A5E886266E7AE",
        data.hash);

    ++on_got_file_data_count_;
    if (on_got_file_data_count_ == quit_file_count_)
      quit_closure_.Run();
  }

  std::vector<FileOpeningJob::FileOpeningTask> CreateFilesAndTasks(int num) {
    std::vector<FileOpeningJob::FileOpeningTask> tasks(num);

    for (int i = 0; i < num; ++i) {
      base::FilePath path = temp_dir_.GetPath().AppendASCII(
          base::StringPrintf("foo%d.txt", next_file_id_));
      ++next_file_id_;
      base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
      file.WriteAtCurrentPos(base::byte_span_from_cstring("foo"));

      auto request = std::make_unique<FileAnalysisRequest>(
          enterprise_connectors::AnalysisSettings(), path, path.BaseName(),
          /*mime_type*/ "",
          /*delay_opening_file*/ true, base::DoNothing());
      auto* request_raw = request.get();
      request_raw->GetRequestData(
          base::BindOnce(&FileOpeningJobTest::OnGotFileData,
                         weak_factory_.GetWeakPtr(), std::move(request)));
      tasks[i].request = request_raw;
    }

    return tasks;
  }

 protected:
  base::ScopedTempDir temp_dir_;
  content::BrowserTaskEnvironment browser_task_environment_;

  int next_file_id_ = 0;
  int on_got_file_data_count_ = 0;
  int quit_file_count_ = 0;
  base::RepeatingClosure quit_closure_;

  base::WeakPtrFactory<FileOpeningJobTest> weak_factory_{this};
};

TEST_F(FileOpeningJobTest, SingleFile) {
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  quit_file_count_ = 1;

  auto tasks = CreateFilesAndTasks(1);
  FileOpeningJob job(std::move(tasks));

  run_loop.Run();
  EXPECT_EQ(1, on_got_file_data_count_);
}

TEST_F(FileOpeningJobTest, MultiFiles) {
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  quit_file_count_ = 100;

  auto tasks = CreateFilesAndTasks(100);
  FileOpeningJob job(std::move(tasks));

  run_loop.Run();
  EXPECT_EQ(100, on_got_file_data_count_);
}

TEST_F(FileOpeningJobTest, MaxThreadsFlag) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  quit_file_count_ = 500;

  EXPECT_EQ(5u, FileOpeningJob::GetMaxFileOpeningThreads());

  command_line->AppendSwitchASCII("wp-max-file-opening-threads", "10");
  EXPECT_EQ(10u, FileOpeningJob::GetMaxFileOpeningThreads());

  command_line->RemoveSwitch("wp-max-file-opening-threads");
  command_line->AppendSwitchASCII("wp-max-file-opening-threads", "0");
  EXPECT_EQ(5u, FileOpeningJob::GetMaxFileOpeningThreads());

  command_line->RemoveSwitch("wp-max-file-opening-threads");
  command_line->AppendSwitchASCII("wp-max-file-opening-threads", "foo");
  EXPECT_EQ(5u, FileOpeningJob::GetMaxFileOpeningThreads());

  command_line->RemoveSwitch("wp-max-file-opening-threads");
  command_line->AppendSwitchASCII("wp-max-file-opening-threads", "-1");
  EXPECT_EQ(5u, FileOpeningJob::GetMaxFileOpeningThreads());
}

}  // namespace safe_browsing
