// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/system_log_uploader.h"

#include <utility>

#include "base/strings/stringprintf.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "components/feedback/anonymizer_tool.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/http/http_request_headers.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

namespace {

// Pseudo-location of policy dump file.
constexpr char kPolicyDumpFileLocation[] = "/var/log/policy_dump.json";
constexpr char kPolicyDump[] = "{}";

// The list of tested system log file names.
const char* const kTestSystemLogFileNames[] = {"name1.txt", "name32.txt"};

// Generate the fake system log files.
SystemLogUploader::SystemLogs GenerateTestSystemLogFiles() {
  SystemLogUploader::SystemLogs system_logs;
  for (auto* file_path : kTestSystemLogFileNames) {
    system_logs.push_back(std::make_pair(file_path, file_path));
  }
  return system_logs;
}

class MockUploadJob : public UploadJob {
 public:
  // If is_upload_error is false OnSuccess() will be invoked when the
  // Start() method is called, otherwise OnFailure() will be invoked.
  MockUploadJob(const GURL& upload_url,
                UploadJob::Delegate* delegate,
                bool is_upload_error,
                int max_files);
  ~MockUploadJob() override;

  // policy::UploadJob:
  void AddDataSegment(const std::string& name,
                      const std::string& filename,
                      const std::map<std::string, std::string>& header_entries,
                      std::unique_ptr<std::string> data) override;
  void Start() override;

 protected:
  UploadJob::Delegate* delegate_;
  bool is_upload_error_;
  int file_index_;
  int max_files_;
};

MockUploadJob::MockUploadJob(const GURL& upload_url,
                             UploadJob::Delegate* delegate,
                             bool is_upload_error,
                             int max_files)
    : delegate_(delegate),
      is_upload_error_(is_upload_error),
      file_index_(0),
      max_files_(max_files) {}

MockUploadJob::~MockUploadJob() {}

void MockUploadJob::AddDataSegment(
    const std::string& name,
    const std::string& filename,
    const std::map<std::string, std::string>& header_entries,
    std::unique_ptr<std::string> data) {
  // Test all fields to upload.
  EXPECT_LT(file_index_, max_files_);
  EXPECT_GE(file_index_, 0);

  EXPECT_EQ(base::StringPrintf(SystemLogUploader::kNameFieldTemplate,
                               file_index_ + 1),
            name);

  if (file_index_ == max_files_ - 1) {
    EXPECT_EQ(kPolicyDumpFileLocation, filename);
  } else {
    EXPECT_EQ(kTestSystemLogFileNames[file_index_], filename);
  }

  EXPECT_EQ(2U, header_entries.size());
  EXPECT_EQ(
      SystemLogUploader::kFileTypeLogFile,
      header_entries.find(SystemLogUploader::kFileTypeHeaderName)->second);
  EXPECT_EQ(SystemLogUploader::kContentTypePlainText,
            header_entries.find(net::HttpRequestHeaders::kContentType)->second);

  if (file_index_ == max_files_ - 1) {
    EXPECT_EQ(kPolicyDump, *data);
  } else {
    EXPECT_EQ(kTestSystemLogFileNames[file_index_], *data);
  }

  file_index_++;
}

void MockUploadJob::Start() {
  DCHECK(delegate_);
  // Check if all files were uploaded.
  EXPECT_EQ(max_files_, file_index_);

  if (is_upload_error_) {
    // Send any ErrorCode.
    delegate_->OnFailure(UploadJob::ErrorCode::NETWORK_ERROR);
  } else {
    delegate_->OnSuccess();
  }
}

// MockSystemLogDelegate - mock class that creates an upload job and runs upload
// callback.
class MockSystemLogDelegate : public SystemLogUploader::Delegate {
 public:
  MockSystemLogDelegate(bool is_upload_error,
                        const SystemLogUploader::SystemLogs& system_logs)
      : is_upload_error_(is_upload_error), system_logs_(system_logs) {}
  ~MockSystemLogDelegate() override {}

  std::string GetPolicyAsJSON() override { return kPolicyDump; }

  void LoadSystemLogs(LogUploadCallback upload_callback) override {
    EXPECT_TRUE(is_upload_allowed_);
    std::move(upload_callback)
        .Run(std::make_unique<SystemLogUploader::SystemLogs>(system_logs_));
  }

  std::unique_ptr<UploadJob> CreateUploadJob(
      const GURL& url,
      UploadJob::Delegate* delegate) override {
    return std::make_unique<MockUploadJob>(url, delegate, is_upload_error_,
                                           system_logs_.size() + 1);
  }

  void set_upload_allowed(bool is_upload_allowed) {
    is_upload_allowed_ = is_upload_allowed;
  }

 private:
  bool is_upload_allowed_;
  bool is_upload_error_;
  SystemLogUploader::SystemLogs system_logs_;
};

}  //  namespace

class SystemLogUploaderTest : public testing::Test {
 public:
  SystemLogUploaderTest() : task_runner_(new base::TestSimpleTaskRunner()) {}

  void SetUp() override {
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
  }

  void TearDown() override {
    settings_helper_.RestoreRealDeviceSettingsProvider();
    content::RunAllTasksUntilIdle();
  }

  // Given a pending task to upload system logs.
  void RunPendingUploadTaskAndCheckNext(const SystemLogUploader& uploader,
                                        base::TimeDelta expected_delay) {
    EXPECT_TRUE(task_runner_->HasPendingTask());
    task_runner_->RunPendingTasks();

    // The previous task should have uploaded another log upload task.
    EXPECT_EQ(1U, task_runner_->NumPendingTasks());

    CheckPendingTaskDelay(uploader, expected_delay);
  }

  void CheckPendingTaskDelay(const SystemLogUploader& uploader,
                             base::TimeDelta expected_delay) {
    // The next task should be scheduled sometime between
    // |last_upload_attempt| + |expected_delay| and
    // |now| + |expected_delay|.
    base::Time now = base::Time::NowFromSystemTime();
    base::Time next_task = now + task_runner_->NextPendingTaskDelay();

    EXPECT_LE(next_task, now + expected_delay);
    EXPECT_GE(next_task, uploader.last_upload_attempt() + expected_delay);
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  chromeos::ScopedCrosSettingsTestHelper settings_helper_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
};

// Check disabled system log uploads by default.
TEST_F(SystemLogUploaderTest, Basic) {
  EXPECT_FALSE(task_runner_->HasPendingTask());

  std::unique_ptr<MockSystemLogDelegate> syslog_delegate(
      new MockSystemLogDelegate(false, SystemLogUploader::SystemLogs()));
  syslog_delegate->set_upload_allowed(false);
  SystemLogUploader uploader(std::move(syslog_delegate), task_runner_);

  task_runner_->RunPendingTasks();
}

// One success task pending.
TEST_F(SystemLogUploaderTest, SuccessTest) {
  EXPECT_FALSE(task_runner_->HasPendingTask());

  std::unique_ptr<MockSystemLogDelegate> syslog_delegate(
      new MockSystemLogDelegate(false, SystemLogUploader::SystemLogs()));
  syslog_delegate->set_upload_allowed(true);
  settings_helper_.SetBoolean(chromeos::kSystemLogUploadEnabled, true);
  SystemLogUploader uploader(std::move(syslog_delegate), task_runner_);

  EXPECT_EQ(1U, task_runner_->NumPendingTasks());

  RunPendingUploadTaskAndCheckNext(
      uploader, base::TimeDelta::FromMilliseconds(
                    SystemLogUploader::kDefaultUploadDelayMs));
}

// Three failed responses recieved.
TEST_F(SystemLogUploaderTest, ThreeFailureTest) {
  EXPECT_FALSE(task_runner_->HasPendingTask());

  std::unique_ptr<MockSystemLogDelegate> syslog_delegate(
      new MockSystemLogDelegate(true, SystemLogUploader::SystemLogs()));
  syslog_delegate->set_upload_allowed(true);
  settings_helper_.SetBoolean(chromeos::kSystemLogUploadEnabled, true);
  SystemLogUploader uploader(std::move(syslog_delegate), task_runner_);

  EXPECT_EQ(1U, task_runner_->NumPendingTasks());

  // Do not retry two times consequentially.
  RunPendingUploadTaskAndCheckNext(uploader,
                                   base::TimeDelta::FromMilliseconds(
                                       SystemLogUploader::kErrorUploadDelayMs));
  // We are using the kDefaultUploadDelayMs and not the kErrorUploadDelayMs here
  // because there's just one retry.
  RunPendingUploadTaskAndCheckNext(
      uploader, base::TimeDelta::FromMilliseconds(
                    SystemLogUploader::kDefaultUploadDelayMs));
  RunPendingUploadTaskAndCheckNext(uploader,
                                   base::TimeDelta::FromMilliseconds(
                                       SystemLogUploader::kErrorUploadDelayMs));
}

// Check header fields of system log files to upload.
TEST_F(SystemLogUploaderTest, CheckHeaders) {
  EXPECT_FALSE(task_runner_->HasPendingTask());

  SystemLogUploader::SystemLogs system_logs = GenerateTestSystemLogFiles();
  std::unique_ptr<MockSystemLogDelegate> syslog_delegate(
      new MockSystemLogDelegate(false, system_logs));
  syslog_delegate->set_upload_allowed(true);
  settings_helper_.SetBoolean(chromeos::kSystemLogUploadEnabled, true);
  SystemLogUploader uploader(std::move(syslog_delegate), task_runner_);

  EXPECT_EQ(1U, task_runner_->NumPendingTasks());

  RunPendingUploadTaskAndCheckNext(
      uploader, base::TimeDelta::FromMilliseconds(
                    SystemLogUploader::kDefaultUploadDelayMs));
}

// Disable system log uploads after one failed log upload.
TEST_F(SystemLogUploaderTest, DisableLogUpload) {
  EXPECT_FALSE(task_runner_->HasPendingTask());

  std::unique_ptr<MockSystemLogDelegate> syslog_delegate(
      new MockSystemLogDelegate(true, SystemLogUploader::SystemLogs()));
  MockSystemLogDelegate* mock_delegate = syslog_delegate.get();
  settings_helper_.SetBoolean(chromeos::kSystemLogUploadEnabled, true);
  mock_delegate->set_upload_allowed(true);
  SystemLogUploader uploader(std::move(syslog_delegate), task_runner_);

  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
  RunPendingUploadTaskAndCheckNext(uploader,
                                   base::TimeDelta::FromMilliseconds(
                                       SystemLogUploader::kErrorUploadDelayMs));

  // Disable log upload and check that frequency is usual, because there is no
  // errors, we should not upload logs.
  settings_helper_.SetBoolean(chromeos::kSystemLogUploadEnabled, false);
  mock_delegate->set_upload_allowed(false);
  task_runner_->RunPendingTasks();

  RunPendingUploadTaskAndCheckNext(
      uploader, base::TimeDelta::FromMilliseconds(
                    SystemLogUploader::kDefaultUploadDelayMs));
  RunPendingUploadTaskAndCheckNext(
      uploader, base::TimeDelta::FromMilliseconds(
                    SystemLogUploader::kDefaultUploadDelayMs));
}

}  // namespace policy
