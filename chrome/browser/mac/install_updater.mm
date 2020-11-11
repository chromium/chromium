// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/install_updater.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/updater/browser_updater_client.h"
#include "chrome/browser/updater/browser_updater_client_util.h"

namespace {

constexpr char kInstallCommand[] = "install";

int RunCommand(const base::FilePath& exe_path, const char* cmd_switch) {
  base::CommandLine command(exe_path);
  command.AppendSwitch(cmd_switch);

  int exit_code = -1;
  auto process = base::LaunchProcess(command, {});
  if (!process.IsValid())
    return exit_code;

  process.WaitForExitWithTimeout(base::TimeDelta::FromSeconds(120), &exit_code);

  return exit_code;
}

base::FilePath GetUpdaterExecutableName() {
  return base::FilePath(base::StrCat({kUpdaterName, ".app"}))
      .Append(FILE_PATH_LITERAL("Contents"))
      .Append(FILE_PATH_LITERAL("MacOS"))
      .Append(kUpdaterName);
}

}  // namespace

void InstallUpdaterAndRegisterBrowser() {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce([] {
        // The updater executable should be in
        // BRANDING.app/Contents/Frameworks/BRANDING.framework/Versions/V/
        // Helpers/Updater.app/Contents/MacOS/Updater
        const base::FilePath updater_executable_path =
            base::mac::FrameworkBundlePath()
                .Append(FILE_PATH_LITERAL("Helpers"))
                .Append(GetUpdaterExecutableName());

        if (!base::PathExists(updater_executable_path)) {
          VLOG(1) << "The updater does not exist in the bundle.";
          return;
        }

        int exit_code = RunCommand(updater_executable_path, kInstallCommand);
        if (exit_code != 0) {
          VLOG(1) << "Couldn't install the updater. Exit code: " << exit_code;
          return;
        }

        BrowserUpdaterClient::Create()->Register();
      }));
}
