// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/multiprocess_test.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"

namespace base {

#if !BUILDFLAG(IS_ANDROID)
Process SpawnMultiProcessTestChild(const std::string& procname,
                                   const CommandLine& base_command_line,
                                   const LaunchOptions& options) {
  CommandLine command_line(base_command_line);
  // TODO(viettrungluu): See comment above |MakeCmdLine()| in the header file.
  // This is a temporary hack, since |MakeCmdLine()| has to provide a full
  // command line.
  if (!command_line.HasSwitch(switches::kTestChildProcess))
    command_line.AppendSwitchASCII(switches::kTestChildProcess, procname);

  return LaunchProcess(command_line, options);
}

bool WaitForMultiprocessTestChildExit(const Process& process,
                                      TimeDelta timeout,
                                      int* exit_code) {
  return process.WaitForExitWithTimeout(timeout, exit_code);
}

bool TerminateMultiProcessTestChild(const Process& process,
                                    int exit_code,
                                    bool wait) {
  return process.Terminate(exit_code, wait);
}

#endif  // !BUILDFLAG(IS_ANDROID)

CommandLine GetMultiProcessTestChildBaseCommandLine() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  CommandLine cmd_line = *CommandLine::ForCurrentProcess();
  cmd_line.SetProgram(MakeAbsoluteFilePath(cmd_line.GetProgram()));
  return cmd_line;
}

// MultiProcessTest ------------------------------------------------------------

MultiProcessTest::MultiProcessTest() = default;

Process MultiProcessTest::SpawnChild(const std::string& procname) {
  LaunchOptions options;
#if BUILDFLAG(IS_WIN)
  options.start_hidden = true;
#endif
  return SpawnChildWithOptions(procname, options);
}

Process MultiProcessTest::SpawnChildWithOptions(const std::string& procname,
                                                const LaunchOptions& options) {
  return SpawnMultiProcessTestChild(procname, MakeCmdLine(procname), options);
}

CommandLine MultiProcessTest::MakeCmdLine(const std::string& procname) {
  CommandLine command_line = GetMultiProcessTestChildBaseCommandLine();
  command_line.AppendSwitchASCII(switches::kTestChildProcess, procname);
  return command_line;
}

}  // namespace base
