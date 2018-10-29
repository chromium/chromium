// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_suite.h"

#include <signal.h>

#include <memory>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/debug/profiler.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/memory.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/test/gtest_xml_unittest_result_printer.h"
#include "base/test/gtest_xml_util.h"
#include "base/test/icu_test_util.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#if defined(OS_IOS)
#include "base/test/test_listener_ios.h"
#endif  // OS_IOS
#endif  // OS_MACOSX

#if !defined(OS_WIN)
#include "base/i18n/rtl.h"
#if !defined(OS_IOS)
#include "base/strings/string_util.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#endif
#endif

#if defined(OS_ANDROID)
#include "base/test/test_support_android.h"
#endif

#if defined(OS_IOS)
#include "base/test/test_support_ios.h"
#endif

#if defined(OS_LINUX)
#include "base/test/fontconfig_util_linux.h"
#endif

namespace base {

namespace {

// Returns true if the test is marked as "MAYBE_".
// When using different prefixes depending on platform, we use MAYBE_ and
// preprocessor directives to replace MAYBE_ with the target prefix.
bool IsMarkedMaybe(const testing::TestInfo& test) {
  return strncmp(test.name(), "MAYBE_", 6) == 0;
}

class DisableMaybeTests : public testing::EmptyTestEventListener {
 public:
  void OnTestStart(const testing::TestInfo& test_info) override {
    ASSERT_FALSE(IsMarkedMaybe(test_info))
        << "Probably the OS #ifdefs don't include all of the necessary "
           "platforms.\nPlease ensure that no tests have the MAYBE_ prefix "
           "after the code is preprocessed.";
  }
};

class ResetCommandLineBetweenTests : public testing::EmptyTestEventListener {
 public:
  ResetCommandLineBetweenTests() : old_command_line_(CommandLine::NO_PROGRAM) {}

  void OnTestStart(const testing::TestInfo& test_info) override {
    old_command_line_ = *CommandLine::ForCurrentProcess();
  }

  void OnTestEnd(const testing::TestInfo& test_info) override {
    *CommandLine::ForCurrentProcess() = old_command_line_;
  }

 private:
  CommandLine old_command_line_;

  DISALLOW_COPY_AND_ASSIGN(ResetCommandLineBetweenTests);
};

class CheckForLeakedGlobals : public testing::EmptyTestEventListener {
 public:
  CheckForLeakedGlobals() = default;

  // Check for leaks in individual tests.
  void OnTestStart(const testing::TestInfo& test) override {
    scheduler_set_before_test_ = TaskScheduler::GetInstance();
  }
  void OnTestEnd(const testing::TestInfo& test) override {
    DCHECK_EQ(scheduler_set_before_test_, TaskScheduler::GetInstance())
        << " in test " << test.test_case_name() << "." << test.name();
  }

  // Check for leaks in test cases (consisting of one or more tests).
  void OnTestCaseStart(const testing::TestCase& test_case) override {
    scheduler_set_before_case_ = TaskScheduler::GetInstance();
  }
  void OnTestCaseEnd(const testing::TestCase& test_case) override {
    DCHECK_EQ(scheduler_set_before_case_, TaskScheduler::GetInstance())
        << " in case " << test_case.name();
  }

 private:
  TaskScheduler* scheduler_set_before_test_ = nullptr;
  TaskScheduler* scheduler_set_before_case_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CheckForLeakedGlobals);
};

const std::string& GetProfileName() {
  static const base::NoDestructor<std::string> profile_name([]() {
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    if (command_line.HasSwitch(switches::kProfilingFile))
      return command_line.GetSwitchValueASCII(switches::kProfilingFile);
    else
      return std::string("test-profile-{pid}");
  }());
  return *profile_name;
}

void InitializeLogging() {
#if defined(OS_ANDROID)
  InitAndroidTestLogging();
#else
  FilePath exe;
  PathService::Get(FILE_EXE, &exe);
  FilePath log_filename = exe.ReplaceExtension(FILE_PATH_LITERAL("log"));
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file = log_filename.value().c_str();
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);
  // We want process and thread IDs because we may have multiple processes.
  // Note: temporarily enabled timestamps in an effort to catch bug 6361.
  logging::SetLogItems(true, true, true, true);
#endif  // !defined(OS_ANDROID)
}

}  // namespace

int RunUnitTestsUsingBaseTestSuite(int argc, char **argv) {
  TestSuite test_suite(argc, argv);
  return LaunchUnitTests(argc, argv,
                         Bind(&TestSuite::Run, Unretained(&test_suite)));
}

TestSuite::TestSuite(int argc, char** argv) {
  PreInitialize();
  InitializeFromCommandLine(argc, argv);
  // Logging must be initialized before any thread has a chance to call logging
  // functions.
  InitializeLogging();
}

#if defined(OS_WIN)
TestSuite::TestSuite(int argc, wchar_t** argv) {
  PreInitialize();
  InitializeFromCommandLine(argc, argv);
  // Logging must be initialized before any thread has a chance to call logging
  // functions.
  InitializeLogging();
}
#endif  // defined(OS_WIN)

TestSuite::~TestSuite() {
  if (initialized_command_line_)
    CommandLine::Reset();
}

void TestSuite::InitializeFromCommandLine(int argc, char** argv) {
  initialized_command_line_ = CommandLine::Init(argc, argv);
  testing::InitGoogleTest(&argc, argv);
  testing::InitGoogleMock(&argc, argv);

#if defined(OS_IOS)
  InitIOSRunHook(this, argc, argv);
#endif
}

#if defined(OS_WIN)
void TestSuite::InitializeFromCommandLine(int argc, wchar_t** argv) {
  // Windows CommandLine::Init ignores argv anyway.
  initialized_command_line_ = CommandLine::Init(argc, NULL);
  testing::InitGoogleTest(&argc, argv);
  testing::InitGoogleMock(&argc, argv);
}
#endif  // defined(OS_WIN)

void TestSuite::PreInitialize() {
  DCHECK(!is_initialized_);

#if defined(OS_WIN)
  testing::GTEST_FLAG(catch_exceptions) = false;
#endif
  EnableTerminationOnHeapCorruption();
#if defined(OS_LINUX) && defined(USE_AURA)
  // When calling native char conversion functions (e.g wrctomb) we need to
  // have the locale set. In the absence of such a call the "C" locale is the
  // default. In the gtk code (below) gtk_init() implicitly sets a locale.
  setlocale(LC_ALL, "");
#endif  // defined(OS_LINUX) && defined(USE_AURA)

  // On Android, AtExitManager is created in
  // testing/android/native_test_wrapper.cc before main() is called.
#if !defined(OS_ANDROID)
  at_exit_manager_.reset(new AtExitManager);
#endif

  // Don't add additional code to this function.  Instead add it to
  // Initialize().  See bug 6436.
}

void TestSuite::AddTestLauncherResultPrinter() {
  // Only add the custom printer if requested.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTestLauncherOutput)) {
    return;
  }

  FilePath output_path(CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      switches::kTestLauncherOutput));

  // Do not add the result printer if output path already exists. It's an
  // indicator there is a process printing to that file, and we're likely
  // its child. Do not clobber the results in that case.
  if (PathExists(output_path)) {
    LOG(WARNING) << "Test launcher output path " << output_path.AsUTF8Unsafe()
                 << " exists. Not adding test launcher result printer.";
    return;
  }

  printer_ = new XmlUnitTestResultPrinter;
  CHECK(printer_->Initialize(output_path))
      << "Output path is " << output_path.AsUTF8Unsafe()
      << " and PathExists(output_path) is " << PathExists(output_path);
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(printer_);
}

// Don't add additional code to this method.  Instead add it to
// Initialize().  See bug 6436.
int TestSuite::Run() {
#if defined(OS_IOS)
  RunTestsFromIOSApp();
#endif

#if defined(OS_MACOSX)
  mac::ScopedNSAutoreleasePool scoped_pool;
#endif

  Initialize();
  std::string client_func =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kTestChildProcess);

  // Check to see if we are being run as a client process.
  if (!client_func.empty())
    return multi_process_function_list::InvokeChildProcessTest(client_func);
#if defined(OS_IOS)
  test_listener_ios::RegisterTestEndListener();
#endif

  int result = RUN_ALL_TESTS();

#if defined(OS_MACOSX)
  // This MUST happen before Shutdown() since Shutdown() tears down
  // objects (such as NotificationService::current()) that Cocoa
  // objects use to remove themselves as observers.
  scoped_pool.Recycle();
#endif

  Shutdown();

  return result;
}

void TestSuite::DisableCheckForLeakedGlobals() {
  DCHECK(!is_initialized_);
  check_for_leaked_globals_ = false;
}

void TestSuite::UnitTestAssertHandler(const char* file,
                                      int line,
                                      const base::StringPiece summary,
                                      const base::StringPiece stack_trace) {
#if defined(OS_ANDROID)
  // Correlating test stdio with logcat can be difficult, so we emit this
  // helpful little hint about what was running.  Only do this for Android
  // because other platforms don't separate out the relevant logs in the same
  // way.
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  if (test_info) {
    LOG(ERROR) << "Currently running: " << test_info->test_case_name() << "."
               << test_info->name();
    fflush(stderr);
  }
#endif  // defined(OS_ANDROID)

  // XmlUnitTestResultPrinter inherits gtest format, where assert has summary
  // and message. In GTest, summary is just a logged text, and message is a
  // logged text, concatenated with stack trace of assert.
  // Concatenate summary and stack_trace here, to pass it as a message.
  if (printer_) {
    const std::string summary_str = summary.as_string();
    const std::string stack_trace_str = summary_str + stack_trace.as_string();
    printer_->OnAssert(file, line, summary_str, stack_trace_str);
  }

  // The logging system actually prints the message before calling the assert
  // handler. Just exit now to avoid printing too many stack traces.
  _exit(1);
}

#if defined(OS_WIN)
namespace {

// Disable optimizations to prevent function folding or other transformations
// that will make the call stacks on failures more confusing.
#pragma optimize("", off)
// Handlers for invalid parameter, pure call, and abort. They generate a
// breakpoint to ensure that we get a call stack on these failures.
void InvalidParameter(const wchar_t* expression,
                      const wchar_t* function,
                      const wchar_t* file,
                      unsigned int line,
                      uintptr_t reserved) {
  // CRT printed message is sufficient.
  __debugbreak();
  _exit(1);
}

void PureCall() {
  fprintf(stderr, "Pure-virtual function call. Terminating.\n");
  __debugbreak();
  _exit(1);
}

void AbortHandler(int signal) {
  // Print EOL after the CRT abort message.
  fprintf(stderr, "\n");
  __debugbreak();
}
#pragma optimize("", on)

}  // namespace
#endif

void TestSuite::SuppressErrorDialogs() {
#if defined(OS_WIN)
  UINT new_flags = SEM_FAILCRITICALERRORS |
                   SEM_NOGPFAULTERRORBOX |
                   SEM_NOOPENFILEERRORBOX;

  // Preserve existing error mode, as discussed at
  // http://blogs.msdn.com/oldnewthing/archive/2004/07/27/198410.aspx
  UINT existing_flags = SetErrorMode(new_flags);
  SetErrorMode(existing_flags | new_flags);

#if defined(_DEBUG)
  // Suppress the "Debug Assertion Failed" dialog.
  // TODO(hbono): remove this code when gtest has it.
  // http://groups.google.com/d/topic/googletestframework/OjuwNlXy5ac/discussion
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
#endif  // defined(_DEBUG)

  // See crbug.com/783040 for test code to trigger all of these failures.
  _set_invalid_parameter_handler(InvalidParameter);
  _set_purecall_handler(PureCall);
  signal(SIGABRT, AbortHandler);
#endif  // defined(OS_WIN)
}

void TestSuite::Initialize() {
  DCHECK(!is_initialized_);

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
#if !defined(OS_IOS)
  if (command_line->HasSwitch(switches::kWaitForDebugger)) {
    debug::WaitForDebugger(60, true);
  }
#endif
  // Set up a FeatureList instance, so that code using that API will not hit a
  // an error that it's not set. It will be cleared automatically.
  // TestFeatureForBrowserTest1 and TestFeatureForBrowserTest2 used in
  // ContentBrowserTestScopedFeatureListTest to ensure ScopedFeatureList keeps
  // features from command line.
  std::string enabled =
      command_line->GetSwitchValueASCII(switches::kEnableFeatures);
  std::string disabled =
      command_line->GetSwitchValueASCII(switches::kDisableFeatures);
  enabled += ",TestFeatureForBrowserTest1";
  disabled += ",TestFeatureForBrowserTest2";
  scoped_feature_list_.InitFromCommandLine(enabled, disabled);

  // The enable-features and disable-features flags were just slurped into a
  // FeatureList, so remove them from the command line. Tests should enable and
  // disable features via the ScopedFeatureList API rather than command-line
  // flags.
  CommandLine new_command_line(command_line->GetProgram());
  CommandLine::SwitchMap switches = command_line->GetSwitches();

  switches.erase(switches::kEnableFeatures);
  switches.erase(switches::kDisableFeatures);

  for (const auto& iter : switches)
    new_command_line.AppendSwitchNative(iter.first, iter.second);

  *CommandLine::ForCurrentProcess() = new_command_line;

#if defined(OS_IOS)
  InitIOSTestMessageLoop();
#endif  // OS_IOS

#if defined(OS_ANDROID)
  InitAndroidTestMessageLoop();
#endif  // else defined(OS_ANDROID)

  CHECK(debug::EnableInProcessStackDumping());
#if defined(OS_WIN)
  RouteStdioToConsole(true);
  // Make sure we run with high resolution timer to minimize differences
  // between production code and test code.
  Time::EnableHighResolutionTimer(true);
#endif  // defined(OS_WIN)

  // In some cases, we do not want to see standard error dialogs.
  if (!debug::BeingDebugged() &&
      !command_line->HasSwitch("show-error-dialogs")) {
    SuppressErrorDialogs();
    debug::SetSuppressDebugUI(true);
    assert_handler_ = std::make_unique<logging::ScopedLogAssertHandler>(
        base::Bind(&TestSuite::UnitTestAssertHandler, base::Unretained(this)));
  }

  base::test::InitializeICUForTesting();

  // On the Mac OS X command line, the default locale is *_POSIX. In Chromium,
  // the locale is set via an OS X locale API and is never *_POSIX.
  // Some tests (such as those involving word break iterator) will behave
  // differently and fail if we use *POSIX locale. Setting it to en_US here
  // does not affect tests that explicitly overrides the locale for testing.
  // This can be an issue on all platforms other than Windows.
  // TODO(jshin): Should we set the locale via an OS X locale API here?
#if !defined(OS_WIN)
#if defined(OS_IOS)
  i18n::SetICUDefaultLocale("en_US");
#else
  std::string default_locale(uloc_getDefault());
  if (EndsWith(default_locale, "POSIX", CompareCase::INSENSITIVE_ASCII))
    i18n::SetICUDefaultLocale("en_US");
#endif
#endif

#if defined(OS_LINUX)
  // TODO(thomasanderson): Call TearDownFontconfig() in Shutdown().  It would
  // currently crash because of leaked FcFontSet's in font_fallback_linux.cc.
  SetUpFontconfig();
#endif

  // Add TestEventListeners to enforce certain properties across tests.
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new DisableMaybeTests);
  listeners.Append(new ResetCommandLineBetweenTests);
  if (check_for_leaked_globals_)
    listeners.Append(new CheckForLeakedGlobals);

  AddTestLauncherResultPrinter();

  TestTimeouts::Initialize();

  trace_to_file_.BeginTracingFromCommandLineOptions();

  base::debug::StartProfiling(GetProfileName());

  is_initialized_ = true;
}

void TestSuite::Shutdown() {
  DCHECK(is_initialized_);
  base::debug::StopProfiling();
}

}  // namespace base
