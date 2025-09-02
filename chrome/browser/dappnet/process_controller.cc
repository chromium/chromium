// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dappnet/process_controller.h"

#include "base/logging.h"
#include "base/process/launch.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"

namespace dappnet {

ProcessController::ProcessController() {
  port_ = 0;  // Will be set by subclasses
}

ProcessController::~ProcessController() {
  if (IsRunning()) {
    Stop();
  }
}

bool ProcessController::Start() {
  if (IsRunning()) {
    LOG(INFO) << "Process already running with PID: " << GetPid();
    return true;
  }

  base::LaunchOptions options;
#if BUILDFLAG(IS_WIN)
  options.start_hidden = true;
#endif
  
  // Set environment variables
  options.environment = GetEnvironmentVariables();

  base::CommandLine command_line = GetCommandLine();
  LOG(INFO) << "Starting process: " << command_line.GetCommandLineString();

  process_ = base::LaunchProcess(command_line, options);
  if (!process_.IsValid()) {
    LOG(ERROR) << "Failed to launch process";
    return false;
  }

  LOG(INFO) << "Process started with PID: " << process_.Pid();

  // Wait for the process to start up
  if (!VerifyStartup()) {
    LOG(ERROR) << "Process startup verification failed";
    Stop();
    return false;
  }

  LOG(INFO) << "Process startup verified successfully";
  return true;
}

bool ProcessController::Stop() {
  if (!process_.IsValid()) {
    return true;
  }

  int pid = process_.Pid();
  LOG(INFO) << "Stopping process with PID: " << pid;

  // Try graceful termination first
  if (!process_.Terminate(0, true)) {
    LOG(WARNING) << "Failed to terminate process gracefully, attempting force kill";
    process_.Terminate(1, false);
  }

  // Wait up to 5 seconds for the process to exit
  int exit_code;
  if (!process_.WaitForExitWithTimeout(base::Seconds(5), &exit_code)) {
    LOG(ERROR) << "Process did not exit within timeout, force killing";
    process_.Terminate(1, false);
    process_.WaitForExit(&exit_code);
  }

  LOG(INFO) << "Process stopped with exit code: " << exit_code;
  process_.Close();
  return true;
}

bool ProcessController::Restart() {
  LOG(INFO) << "Restarting process";
  Stop();
  
  // Give the system a moment to clean up
  base::PlatformThread::Sleep(base::Milliseconds(500));
  
  return Start();
}

bool ProcessController::IsRunning() const {
  return process_.IsValid();
}

int ProcessController::GetPid() const {
  if (!process_.IsValid()) {
    return 0;
  }
  return process_.Pid();
}

base::EnvironmentMap ProcessController::GetEnvironmentVariables() const {
  return {};
}

}  // namespace dappnet