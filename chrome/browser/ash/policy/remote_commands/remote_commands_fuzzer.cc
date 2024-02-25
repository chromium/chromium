// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <initializer_list>
#include <memory>
#include <string>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/syslog_logging.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ash/policy/remote_commands/crd/device_command_start_crd_session_job.h"
#include "chrome/browser/ash/policy/remote_commands/crd/fake_start_crd_session_job_delegate.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_get_routine_update_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_reboot_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_run_routine_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_screenshot_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_set_volume_job.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr logging::LogSeverity kLogSeverity = logging::LOGGING_FATAL;

// A log handler that discards messages whose severity is lower than the
// threshold. It's needed in order to suppress unneeded syslog logging (which by
// default is exempt from the level set by `logging::SetMinLogLevel()`).
bool VoidifyingLogHandler(int severity,
                          const char* /*file*/,
                          int /*line*/,
                          size_t /*message_start*/,
                          const std::string& /*str*/) {
  return severity < kLogSeverity;
}

// Holds the state and performs initialization that's shared across fuzzer runs.
struct Environment {
  Environment() {
    // Discard all log messages, including the syslog ones, below the threshold.
    logging::SetMinLogLevel(kLogSeverity);
    logging::SetSyslogLoggingForTesting(/*logging_enabled=*/false);
    logging::SetLogMessageHandler(&VoidifyingLogHandler);

    CHECK(base::i18n::InitializeICU());
  }

  // Initialize the "at exit manager" singleton used by the tested code.
  base::AtExitManager at_exit_manager;
  ProfileManager profile_manager{/*user_data_dir=*/{}};
};

class StubDeviceCommandScreenshotJobDelegate
    : public DeviceCommandScreenshotJob::Delegate {
 public:
  bool IsScreenshotAllowed() override { return false; }
  void TakeSnapshot(gfx::NativeWindow,
                    const gfx::Rect&,
                    ui::GrabSnapshotDataCallback) override {}
  std::unique_ptr<UploadJob> CreateUploadJob(const GURL&,
                                             UploadJob::Delegate*) override {
    return nullptr;
  }
};

}  // namespace

// Fuzzer for command payload parsers in RemoteCommandJob subclasses.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  FuzzedDataProvider fuzzed_data_provider(data, size);

  // Prepare all possible job objects (this is a cheap operation).
  DeviceCommandScreenshotJob screenshot_job(
      std::make_unique<StubDeviceCommandScreenshotJobDelegate>());
  DeviceCommandSetVolumeJob set_volume_job;
  FakeStartCrdSessionJobDelegate start_crd_session_job_delegate;
  DeviceCommandStartCrdSessionJob start_crd_session_job(
      start_crd_session_job_delegate);
  DeviceCommandRunRoutineJob run_routine_job;
  DeviceCommandGetRoutineUpdateJob get_routine_update_job;
  DeviceCommandRebootJob reboot_job;
  std::initializer_list<RemoteCommandJob* const> jobs = {
      &screenshot_job,  &set_volume_job,         &start_crd_session_job,
      &run_routine_job, &get_routine_update_job, &reboot_job,
  };

  // Select a random job.
  RemoteCommandJob* const job = fuzzed_data_provider.PickValueInArray(jobs);

  // Initialize the job instance, including parsing the command payload.
  em::RemoteCommand remote_command;
  remote_command.set_type(job->GetType());
  remote_command.set_command_id(1);
  remote_command.set_age_of_command(1);
  remote_command.set_payload(
      fuzzed_data_provider.ConsumeRemainingBytesAsString());
  job->Init(/*now=*/base::TimeTicks::Now(), remote_command, em::SignedData());

  return 0;
}

}  // namespace policy
