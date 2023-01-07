// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/command_line_log_source.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// Gathers log data from various scripts/programs.
void ExecuteCommandLines(system_logs::SystemLogsResponse* response) {
  // TODO(tudalex): Move program calling in a array or something similar to make
  // it more easier to modify and understand.
  std::vector<std::pair<std::string, base::CommandLine>> commands;

  base::CommandLine command_line(base::FilePath("/usr/bin/amixer"));
  command_line.AppendArg("-c0");
  command_line.AppendArg("contents");
  commands.emplace_back("alsa controls", command_line);

  command_line =
      base::CommandLine((base::FilePath("/usr/bin/cras_test_client")));
  command_line.AppendArg("--dump_server_info");
  command_line.AppendArg("--dump_audio_thread");
  commands.emplace_back("cras", command_line);

  command_line =
      base::CommandLine((base::FilePath("/usr/bin/audio_diagnostics")));
  commands.emplace_back("audio_diagnostics", command_line);

#if 0
  // This command hangs as of R39. TODO(alhli): Make cras_test_client more
  // robust or add a wrapper script that times out, and fix this or remove
  // this code. crbug.com/419523
  command_line =
      base::CommandLine((base::FilePath("/usr/bin/cras_test_client")));
  command_line.AppendArg("--loopback_file");
  command_line.AppendArg("/dev/null");
  command_line.AppendArg("--rate");
  command_line.AppendArg("44100");
  command_line.AppendArg("--duration_seconds");
  command_line.AppendArg("0.01");
  command_line.AppendArg("--show_total_rms");
  commands.emplace_back("cras_rms", command_line);
#endif

  command_line = base::CommandLine((base::FilePath("/usr/bin/printenv")));
  commands.emplace_back("env", command_line);

  // Get disk space usage information
  command_line = base::CommandLine(base::FilePath("/bin/df"));
  command_line.AppendArg("--human-readable");
  commands.emplace_back("disk_usage", command_line);

  for (const auto& command : commands) {
    VLOG(1) << "Executing System Logs Command: " << command.first;
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
