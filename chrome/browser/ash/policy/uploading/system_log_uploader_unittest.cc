// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/uploading/system_log_uploader.h"

#include <map>
#include <utility>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/http/http_request_headers.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::ContainerEq;

namespace policy {

namespace {

constexpr char kPolicyDump[] = "{}";

// The list of tested system log file names.
const char* const kTestSystemLogFileNames[] = {"name1.txt", "name32.txt"};

constexpr char kZippedData[] = "zipped_data";

constexpr RemoteCommandJob::UniqueIDType kCommandId = 12345;

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
  MockUploadJob(UploadJob::Delegate* delegate,
                bool is_upload_error,
                bool is_immediate_upload);
  ~MockUploadJob() override;

  // policy::UploadJob:
  void AddDataSegment(const std::string& name,
                      const std::string& filename,
                      const std::map<std::string, std::string>& header_entries,
                      std::unique_ptr<std::string> data) override;
  void Start() override;

 protected:
  const std::map<std::string, std::string> kExpectedUploadHeaders = {
      {SystemLogUploader::kFileTypeHeaderName,
       SystemLogUploader::kFileTypeZippedLogFile},
      {net::HttpRequestHeaders::kContentType,
       SystemLogUploader::kContentTypeOctetStream}};

  // Immediate upload headers need to contain "Command-ID" field as they're
  // triggered by a remote command.
  const std::map<std::string, std::string> kExpectedHeadersImmediateUpload = {
      {SystemLogUploader::kFileTypeHeaderName,
       SystemLogUploader::kFileTypeZippedLogFile},
      {net::HttpRequestHeaders::kContentType,
       SystemLogUploader::kContentTypeOctetStream},
      {SystemLogUploader::kCommandIdHeaderName,
       base::NumberToString(kCommandId)}};

  raw_ptr<UploadJob::Delegate> delegate_;
  bool is_upload_error_;
  bool is_immediate_upload_;
};

MockUploadJob::MockUploadJob(UploadJob::Delegate* delegate,
                             bool is_upload_error,
                             bool is_immediate_upload)
    : delegate_(delegate),
      is_upload_error_(is_upload_error),
      is_immediate_upload_(is_immediate_upload) {}

MockUploadJob::~MockUploadJob() = default;

void MockUploadJob::AddDataSegment(
    const std::string& name,
    const std::string& filename,
    const std::map<std::string, std::string>& header_entries,
    std::unique_ptr<std::string> data) {
  // Test all fields to upload.
  EXPECT_EQ(SystemLogUploader::kZippedLogsName, name);

  EXPECT_EQ(SystemLogUploader::kZippedLogsFileName, filename);

  EXPECT_THAT(header_entries,
              ContainerEq(is_immediate_upload_ ? kExpectedHeadersImmediateUpload
                                               : kExpectedUploadHeaders));
  EXPECT_EQ(kZippedData, *data);
}

void MockUploadJob::Start() {
  DCHECK(delegate_);

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
                        const SystemLogUploader::SystemLogs& system_logs,
                        bool is_immediate_upload)
      : is_upload_error_(is_upload_error),
        is_immediate_upload_(is_immediate_upload),
        system_logs_(system_logs) {}
  ~MockSystemLogDelegate() override = default;

  std::string GetPolicyAsJSON() override { return kPolicyDump; }

  void LoadSystemLogs(LogUploadCallback upload_callback) override {
    EXPECT_TRUE(is_upload_allowed_);
    std::move(upload_callback)
        .Run(std::make_unique<SystemLogUploader::SystemLogs>(system_logs_));
  }

  std::unique_ptr<UploadJob> CreateUploadJob(
      const GURL& url,
      UploadJob::Delegate* delegate) override {
    return std::make_unique<MockUploadJob>(delegate, is_upload_error_,
                                           is_immediate_upload_);
  }

  void ZipSystemLogs(std::unique_ptr<SystemLogUploader::SystemLogs> system_logs,
                     ZippedLogUploadCallback upload_callback) override {
    for (const auto& log : system_logs_)
      EXPECT_TRUE(base::Contains(*system_logs, log));
    std::move(upload_callback).Run(std::string(kZippedData));
  }

  void set_upload_allowed(bool is_upload_allowed) {
    is_upload_allowed_ = is_upload_allowed;
  }

 private:
  bool is_upload_allowed_;
  bool is_upload_error_;
  bool is_immediate_upload_;
  SystemLogUploader::SystemLogs system_logs_;
};

class MockSystemLogUploader : public SystemLogUploader {
 public:
  MockSystemLogUploader(
      std::unique_ptr<Delegate> syslog_delegate,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : SystemLogUploader(std::move(syslog_delegate), task_runner) {}
  MOCK_METHOD(void, OnSuccess, (), (override));
};

}  //  namespace

class SystemLogUploaderTest : public testing::TestWithParam<bool> {
 public:
  TestingPrefServiceSimple local_state_;
  SystemLogUploaderTest() : task_runner_(new base::TestSimpleTaskRunner()) {}

  void SetUp() override {
    RegisterLocalState(local_state_.registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
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
  content::BrowserTaskEnvironment task_environment_;
  ash::ScopedCrosSettingsTestHelper settings_helper_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::test::ScopedFeatureList feature_list;
};

// Verify log throttling. Try successive kLogThrottleCount log uploads by
// creating a new task. First kLogThrottleCount logs should have 0 delay.
// Successive logs should have delay greater than zero.
TEST_P(SystemLogUploaderTest, LogThrottleTest) {
  for (int upload_num = 0;
       upload_num < SystemLogUploader::kLogThrottleCount + 3; upload_num++) {
    EXPECT_FALSE(task_runner_->HasPendingTask());
    auto syslog_delegate = std::make_unique<MockSystemLogDelegate>(
        false, SystemLogUploader::SystemLogs(), /*is_immediate_upload=*/false);

    syslog_delegate->set_upload_allowed(true);
    settings_helper_.SetBoolean(ash::kSystemLogUploadEnabled, true);

    SystemLogUploader uploader(std::move(syslog_delegate), task_runner_);

    EXPECT_EQ(1U, task_runner_->NumPendingTasks());

    if (upload_num < SystemLogUploader::kLogThrottleCount) {
      EXPECT_EQ(task_runner_->NextPendingTaskDelay(), base::Milliseconds(0));
    } else {
      EXPECT_GT(task_runner_->NextPendingTaskDelay(), base::Milliseconds(0));
    }

    task_runner_->RunPendingTasks();
    task_runner_->ClearPendingTasks();
  }
}

// Verify that we never throttle immediate log upload.
TEST_P(SystemLogUploaderTest, ImmediateLogUpload) {
  EXPECT_FALSE(task_runner_->HasPendingTask());
  auto syslog_delegate = std::make_unique<MockSystemLogDelegate>(
      false, SystemLogUploader::SystemLogs(), /*is_immediate_upload=*/true);

  syslog_delegate->set_upload_allowed(true);
  settings_helper_.SetBoolean(ash::kSystemLogUploadEnabled, true);

  SystemLogUploader uploader(std::move(syslog_delegate), task_runner_);
  for (int upload_num = 0;
       upload_num < SystemLogUploader::kLogThrottleCount + 3; upload_num++) {
    uploader.ScheduleNextSystemLogUploadImmediately(kCommandId);
    EXPECT_EQ(task_runner_->NextPendingTaskDelay(), base::Milliseconds(0));
    task_runner_->RunPendingTasks();
    task_runner_->ClearPendingTasks();
  }
}

// Check disabled system log uploads by default.
TEST_P(SystemLogUploaderTest, Basic) {
  EXPECT_FALSE(task_runner_->HasPendingTask());

  std::unique_ptr<MockSystemLogDelegate> syslog_delegate(
      new MockSystemLogDelegate(/*is_upload_error=*/false,
                                SystemLogUploader::SystemLogs(),
                                /*is_immediate_upload=*/false));
  syslog_delegate->set_upload_allowed(false);
  SystemLogUploader uploader(std::move(syslog_delegate), task_runner_);

  task_runner_->RunPendingTasks();
}

// One success task pending.
TEST_P(SystemLogUploaderTest, SuccessTest) {
  EXPECT_FALSE(task_runner_->HasPendingTask());

  std::unique_ptr<MockSystemLogDelegate> syslog_delegate(
      new MockSystemLogDelegate(/*is_upload_error=*/false,
                                SystemLogUploader::SystemLogs(),
                                /*is_immediate_upload=*/false));
  syslog_delegate->set_upload_allowed(true);
  settings_helper_.SetBoolean(ash::kSystemLogUploadEnabled, true);
  SystemLogUploader uploader(std::move(syslog_delegate), task_runner_);

  EXPECT_EQ(1U, task_runner_->NumPendingTasks());

  RunPendingUploadTaskAndCheckNext(
      uploader, base::Milliseconds(SystemLogUploader::kDefaultUploadDelayMs));
}

// Three failed responses received.
TEST_P(SystemLogUploaderTest, ThreeFailureTest) {
  EXPECT_FALSE(task_runner_->HasPendingTask());

  std::unique_ptr<MockSystemLogDelegate> syslog_delegate(
      new MockSystemLogDelegate(/*is_upload_error=*/true,
                                SystemLogUploader::SystemLogs(),
                                /*is_immediate_upload=*/false));
  syslog_delegate->set_upload_allowed(true);
  settings_helper_.SetBoolean(ash::kSystemLogUploadEnabled, true);
  SystemLogUploader uploader(std::move(syslog_delegate), task_runner_);

  EXPECT_EQ(1U, task_runner_->NumPendingTasks());

  // Do not retry two times consequentially.
  RunPendingUploadTaskAndCheckNext(
      uploader, base::Milliseconds(SystemLogUploader::kErrorUploadDelayMs));
  // We are using the kDefaultUploadDelayMs and not the kErrorUploadDelayMs here
  // because there's just one retry.
  RunPendingUploadTaskAndCheckNext(
      uploader, base::Milliseconds(SystemLogUploader::kDefaultUploadDelayMs));
  RunPendingUploadTaskAndCheckNext(
      uploader, base::Milliseconds(SystemLogUploader::kErrorUploadDelayMs));
}

// Check header fields of system log files to upload.
TEST_P(SystemLogUploaderTest, CheckHeaders) {
  EXPECT_FALSE(task_runner_->HasPendingTask());

  SystemLogUploader::SystemLogs system_logs = GenerateTestSystemLogFiles();
  std::unique_ptr<MockSystemLogDelegate> syslog_delegate(
      new MockSystemLogDelegate(/*is_upload_error=*/false, system_logs,
                                /*is_immediate_upload=*/false));
  syslog_delegate->set_upload_allowed(true);
  settings_helper_.SetBoolean(ash::kSystemLogUploadEnabled, true);
  SystemLogUploader uploader(std::move(syslog_delegate), task_runner_);

  EXPECT_EQ(1U, task_runner_->NumPendingTasks());

  RunPendingUploadTaskAndCheckNext(
      uploader, base::Milliseconds(SystemLogUploader::kDefaultUploadDelayMs));
}

// Disable system log uploads after one failed log upload.
TEST_P(SystemLogUploaderTest, DisableLogUpload) {
  EXPECT_FALSE(task_runner_->HasPendingTask());

  std::unique_ptr<MockSystemLogDelegate> syslog_delegate(
      new MockSystemLogDelegate(/*is_upload_error=*/true,
                                SystemLogUploader::SystemLogs(),
                                /*is_immediate_upload=*/false));
  MockSystemLogDelegate* mock_delegate = syslog_delegate.get();
  settings_helper_.SetBoolean(ash::kSystemLogUploadEnabled, true);
  mock_delegate->set_upload_allowed(true);
  SystemLogUploader uploader(std::move(syslog_delegate), task_runner_);

  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
  RunPendingUploadTaskAndCheckNext(
      uploader, base::Milliseconds(SystemLogUploader::kErrorUploadDelayMs));

  // Disable log upload and check that frequency is usual, because there is no
  // errors, we should not upload logs.
  settings_helper_.SetBoolean(ash::kSystemLogUploadEnabled, false);
  mock_delegate->set_upload_allowed(false);
  task_runner_->RunPendingTasks();

  RunPendingUploadTaskAndCheckNext(
      uploader, base::Milliseconds(SystemLogUploader::kDefaultUploadDelayMs));
  RunPendingUploadTaskAndCheckNext(
      uploader, base::Milliseconds(SystemLogUploader::kDefaultUploadDelayMs));
}

// Test that we observe for settings to become trusted and create log jobs
// when the settings become trusted.
TEST_F(SystemLogUploaderTest, DeviceSettingsPendingToTrusted) {
  EXPECT_FALSE(task_runner_->HasPendingTask());

  std::unique_ptr<MockSystemLogDelegate> syslog_delegate(
      new MockSystemLogDelegate(/*is_upload_error=*/false,
                                SystemLogUploader::SystemLogs(),
                                /*is_immediate_upload=*/false));
  MockSystemLogDelegate* mock_delegate = syslog_delegate.get();
  settings_helper_.SetBoolean(ash::kSystemLogUploadEnabled, true);
  settings_helper_.SetTrustedStatus(
      ash::CrosSettingsProvider::TEMPORARILY_UNTRUSTED);
  mock_delegate->set_upload_allowed(true);
  MockSystemLogUploader uploader(std::move(syslog_delegate), task_runner_);

  // We should only see one log job success case after running all of the tasks.
  EXPECT_CALL(uploader, OnSuccess()).Times(1);

  // Tasks should not be pending while trusted settings are pending.
  EXPECT_EQ(0U, task_runner_->NumPendingTasks());

  // Change settings to trusted to trigger
  // the log uploader's settings observer callback.
  settings_helper_.SetTrustedStatus(ash::CrosSettingsProvider::TRUSTED);

  // There should be a pending task now
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
  task_runner_->RunPendingTasks();
}

// Test that log jobs are not created when settings are untrusted
// and permanently untrusted.
TEST_F(SystemLogUploaderTest, DeviceSettingsPendingToUntrusted) {
  EXPECT_FALSE(task_runner_->HasPendingTask());

  std::unique_ptr<MockSystemLogDelegate> syslog_delegate(
      new MockSystemLogDelegate(/*is_upload_error=*/false,
                                SystemLogUploader::SystemLogs(),
                                /*is_immediate_upload=*/false));
  MockSystemLogDelegate* mock_delegate = syslog_delegate.get();
  settings_helper_.SetBoolean(ash::kSystemLogUploadEnabled, true);
  settings_helper_.SetTrustedStatus(
      ash::CrosSettingsProvider::TEMPORARILY_UNTRUSTED);
  mock_delegate->set_upload_allowed(true);
  MockSystemLogUploader uploader(std::move(syslog_delegate), task_runner_);

  // We should not see any log job successes after running all of the tasks.
  EXPECT_CALL(uploader, OnSuccess()).Times(0);

  // Tasks should not be pending while trusted settings are pending.
  EXPECT_EQ(0U, task_runner_->NumPendingTasks());

  // Change settings to permanently untrusted.
  settings_helper_.SetTrustedStatus(
      ash::CrosSettingsProvider::PERMANENTLY_UNTRUSTED);

  // Tasks should not be pending if trusted settings are permanently untrusted.
  EXPECT_EQ(0U, task_runner_->NumPendingTasks());
}

INSTANTIATE_TEST_SUITE_P(SystemLogUploaderTestInstance,
                         SystemLogUploaderTest,
                         testing::Bool());

}  // namespace policy
