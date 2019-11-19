// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/test_license_server.h"

#include <utility>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/process/launch.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/media/test_license_server_config.h"

TestLicenseServer::TestLicenseServer(
    std::unique_ptr<TestLicenseServerConfig> server_config)
    : server_config_(std::move(server_config)) {}

TestLicenseServer::~TestLicenseServer() {
  Stop();
}

bool TestLicenseServer::Start() {
  if (license_server_process_.IsValid())
    return true;

  if (!server_config_->IsPlatformSupported()) {
    DVLOG(0) << "License server is not supported on current platform.";
    return false;
  }

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  if (!server_config_->GetServerCommandLine(&command_line)) {
    DVLOG(0) << "Could not get server command line to launch.";
    return false;
  }

  base::Optional<base::EnvironmentMap> env =
      server_config_->GetServerEnvironment();
  if (!env) {
    DVLOG(0) << "Could not get server environment variables.";
    return false;
  }

  DVLOG(0) << "Starting test license server "
           << command_line.GetCommandLineString();
  base::LaunchOptions launch_options;
  launch_options.environment = std::move(*env);
  license_server_process_ = base::LaunchProcess(command_line, launch_options);
  if (!license_server_process_.IsValid()) {
    DVLOG(0) << "Failed to start test license server!";
    return false;
  }
  return true;
}

bool TestLicenseServer::Stop() {
  if (!license_server_process_.IsValid())
    return true;
  DVLOG(0) << "Killing license server.";

  bool kill_succeeded = false;
  {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_sync_primitives;
    kill_succeeded = license_server_process_.Terminate(1, true);
  }

  if (kill_succeeded) {
    license_server_process_.Close();
  } else {
    DVLOG(1) << "Kill failed?!";
  }
  return kill_succeeded;
}

std::string TestLicenseServer::GetServerURL() {
  return server_config_->GetServerURL();
}
