// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/upgrade_util.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "chrome/browser/first_run/upgrade_util_linux.h"

namespace {

double saved_last_modified_time_of_exe = 0;

}  // namespace

namespace upgrade_util {

bool RelaunchChromeBrowserImpl(const base::CommandLine& command_line) {
  base::LaunchOptions options;
  // Don't set NO_NEW_PRIVS on the relaunched browser process.
  options.allow_new_privs = true;
  return base::LaunchProcess(command_line, options).IsValid();
}

bool IsUpdatePendingRestart() {
  return saved_last_modified_time_of_exe != GetLastModifiedTimeOfExe();
}

void SaveLastModifiedTimeOfExe() {
  saved_last_modified_time_of_exe = GetLastModifiedTimeOfExe();
}

double GetLastModifiedTimeOfExe() {
  base::FilePath exe_file_path;
  if (!base::PathService::Get(base::FILE_EXE, &exe_file_path)) {
    LOG(WARNING) << "Failed to get base::FilePath object for FILE_EXE.";
    return saved_last_modified_time_of_exe;
  }
  base::File::Info exe_file_info;
  if (!base::GetFileInfo(exe_file_path, &exe_file_info)) {
    LOG(WARNING) << "Failed to get FileInfo object for FILE_EXE - "
                 << exe_file_path.value();
    return saved_last_modified_time_of_exe;
  }
  return exe_file_info.last_modified.InSecondsFSinceUnixEpoch();
}

}  // namespace upgrade_util
