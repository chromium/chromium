// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/command_line_log_source.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// Gathers log data from various scripts/programs.
void ExecuteCommandLines(system_logs::SystemLogsResponse* response) {
  // TODO(tudalex): Move program calling in a array or something similar to make
  // it more easier to modify and understand.
  std::vector<std::pair<std::string, base::CommandLine>> commands;

  base::CommandLine command(base::FilePath("/usr/bin/amixer"));
  command.AppendArg("-c0");
  command.AppendArg("contents");
  commands.emplace_back("alsa controls", command);

  command = base::CommandLine((base::FilePath("/usr/bin/cras_test_client")));
  command.AppendArg("--dump_server_info");
  command.AppendArg("--dump_audio_thread");
  commands.emplace_back("cras", command);

  command = base::CommandLine((base::FilePath("/usr/bin/audio_diagnostics")));
  commands.emplace_back("audio_diagnostics", command);

#if 0
  // This command hangs as of R39. TODO(alhli): Make cras_test_client more
  // robust or add a wrapper script that times out, and fix this or remove
  // this code. crbug.com/419523
  command = base::CommandLine((base::FilePath("/usr/bin/cras_test_client")));
  command.AppendArg("--loopback_file");
  command.AppendArg("/dev/null");
  command.AppendArg("--rate");
  command.AppendArg("44100");
  command.AppendArg("--duration_seconds");
  command.AppendArg("0.01");
  command.AppendArg("--show_total_rms");
  commands.emplace_back("cras_rms", command);
#endif

  command = base::CommandLine((base::FilePath("/usr/bin/printenv")));
  commands.emplace_back("env", command);

  // Get disk space usage information
  command = base::CommandLine(base::FilePath("/bin/df"));
  commands.emplace_back("disk_usage", command);

  for (const auto& command : commands) {
    VLOG(1) << "Executting System Logs Command: " << command.first;
    std::string output;
    base::GetAppOutput(command.second, &output);
    response->emplace(command.first, output);
  }
}

}  // namespace

namespace system_logs {

CommandLineLogSource::CommandLineLogSource() : SystemLogsSource("CommandLine") {
}

CommandLineLogSource::~CommandLineLogSource() {
}

void CommandLineLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  auto response = std::make_unique<SystemLogsResponse>();
  SystemLogsResponse* response_ptr = response.get();
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ExecuteCommandLines, response_ptr),
      base::BindOnce(std::move(callback), std::move(response)));
}

}  // namespace system_logs
