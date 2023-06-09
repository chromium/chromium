// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/ppapi/ppapi_test.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/common/nacl_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

class NaClGdbDebugStubTest : public PPAPINaClNewlibTest {
 public:
  NaClGdbDebugStubTest() {
  }

  void SetUpCommandLine(base::CommandLine* command_line) override;

  void StartTestScript(base::Process* test_process,
                       const std::string& test_name,
                       int debug_stub_port);
  void RunDebugStubTest(const std::string& nacl_module,
                        const std::string& test_name);
};

void NaClGdbDebugStubTest::SetUpCommandLine(base::CommandLine* command_line) {
  PPAPINaClNewlibTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kEnableNaClDebug);
}

void NaClGdbDebugStubTest::StartTestScript(base::Process* test_process,
                                           const std::string& test_name,
                                           int debug_stub_port) {
  // We call python script to reuse GDB RSP protocol implementation.
  base::CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("python3")));
  base::FilePath script;
  base::PathService::Get(chrome::DIR_TEST_DATA, &script);
  script = script.AppendASCII("nacl/debug_stub_browser_tests.py");
  cmd.AppendArgPath(script);
  cmd.AppendArg(base::NumberToString(debug_stub_port));
  cmd.AppendArg(test_name);
  LOG(INFO) << cmd.GetCommandLineString();
  *test_process = base::LaunchProcess(cmd, base::LaunchOptions());
}

void NaClGdbDebugStubTest::RunDebugStubTest(const std::string& nacl_module,
                                            const std::string& test_name) {
  base::Process test_script;
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  nacl::NaClBrowser::SetGdbDebugStubPortListenerForTest(
      base::BindRepeating(&NaClGdbDebugStubTest::StartTestScript,
                          base::Unretained(this), &test_script, test_name));
  // Turn on debug stub logging.
  env->SetVar("NACLVERBOSITY", "1");
  RunTestViaHTTP(nacl_module);
  env->UnSetVar("NACLVERBOSITY");
  nacl::NaClBrowser::ClearGdbDebugStubPortListenerForTest();
  int exit_code;
  test_script.WaitForExit(&exit_code);
  EXPECT_EQ(0, exit_code);
}

// NaCl tests are disabled under ASAN because of qualification test.
#if defined(ADDRESS_SANITIZER)
# define MAYBE_Empty DISABLED_Empty
#else
# define MAYBE_Empty Empty
#endif

IN_PROC_BROWSER_TEST_F(NaClGdbDebugStubTest, MAYBE_Empty) {
  RunDebugStubTest("Empty", "continue");
}

#if defined(ADDRESS_SANITIZER)
# define MAYBE_Breakpoint DISABLED_Breakpoint
#elif (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && \
    defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux: http://crbug.com/238469
# define MAYBE_Breakpoint DISABLED_Breakpoint
#else
# define MAYBE_Breakpoint Breakpoint
#endif

IN_PROC_BROWSER_TEST_F(NaClGdbDebugStubTest, MAYBE_Breakpoint) {
  RunDebugStubTest("Empty", "breakpoint");
}
