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
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/tagging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/memory.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/strings/string_piece.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/gtest_xml_unittest_result_printer.h"
#include "base/test/gtest_xml_util.h"
#include "base/test/icu_test_util.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/tracing_buildflags.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if defined(OS_APPLE)
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/process/port_provider_mac.h"
#endif  // OS_APPLE

#if defined(OS_IOS)
#include "base/test/test_listener_ios.h"
#include "base/test/test_support_ios.h"
#else
#include "base/strings/string_util.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#endif

#if defined(OS_ANDROID)
#include "base/test/test_support_android.h"
#endif

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "base/test/fontconfig_util_linux.h"
#endif

#if defined(OS_FUCHSIA)
#include "base/base_paths_fuchsia.h"
#endif

#if defined(OS_WIN)
#if defined(_DEBUG)
#include <crtdbg.h>
#endif  // _DEBUG
#include <windows.h>
#endif  // OS_WIN

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

// Initializes a base::test::ScopedFeatureList for each individual test, which
// involves a FeatureList and a FieldTrialList, such that unit test don't need
// to initialize them manually.
class FeatureListScopedToEachTest : public testing::EmptyTestEventListener {
 public:
  FeatureListScopedToEachTest() = default;
  ~FeatureListScopedToEachTest() override = default;

  FeatureListScopedToEachTest(const FeatureListScopedToEachTest&) = delete;
  FeatureListScopedToEachTest& operator=(const FeatureListScopedToEachTest&) =
      delete;

  void OnTestStart(const testing::TestInfo& test_info) override {
    field_trial_list_ = std::make_unique<FieldTrialList>(
        std::make_unique<MockEntropyProvider>());

    const CommandLine* command_line = CommandLine::ForCurrentProcess();

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
    // FeatureList, so remove them from the command line. Tests should enable
    // and disable features via the ScopedFeatureList API rather than
    // command-line flags.
    CommandLine new_command_line(command_line->GetProgram());
    CommandLine::SwitchMap switches = command_line->GetSwitches();

    switches.erase(switches::kEnableFeatures);
    switches.erase(switches::kDisableFeatures);

    for (const auto& iter : switches)
      new_command_line.AppendSwitchNative(iter.first, iter.second);

    *CommandLine::ForCurrentProcess() = new_command_line;
  }

  void OnTestEnd(const testing::TestInfo& test_info) override {
    scoped_feature_list_.Reset();
    field_trial_list_.reset();
  }

 private:
  std::unique_ptr<FieldTrialList> field_trial_list_;
  test::ScopedFeatureList scoped_feature_list_;
};

class CheckForLeakedGlobals : public testing::EmptyTestEventListener {
 public:
  CheckForLeakedGlobals() = default;

  // Check for leaks in individual tests.
  void OnTestStart(const testing::TestInfo& test) override {
    feature_list_set_before_test_ = FeatureList::GetInstance();
    thread_pool_set_before_test_ = ThreadPoolInstance::Get();
  }
  void OnTestEnd(const testing::TestInfo& test) override {
    DCHECK_EQ(feature_list_set_before_test_, FeatureList::GetInstance())
        << " in test " << test.test_case_name() << "." << test.name();
    DCHECK_EQ(thread_pool_set_before_test_, ThreadPoolInstance::Get())
        << " in test " << test.test_case_name() << "." << test.name();
  }

  // Check for leaks in test cases (consisting of one or more tests).
  void OnTestCaseStart(const testing::TestCase& test_case) override {
    feature_list_set_before_case_ = FeatureList::GetInstance();
    thread_pool_set_before_case_ = ThreadPoolInstance::Get();
  }
  void OnTestCaseEnd(const testing::TestCase& test_case) override {
    DCHECK_EQ(feature_list_set_before_case_, FeatureList::GetInstance())
        << " in case " << test_case.name();
    DCHECK_EQ(thread_pool_set_before_case_, ThreadPoolInstance::Get())
        << " in case " << test_case.name();
  }

 private:
  FeatureList* feature_list_set_before_test_ = nullptr;
  FeatureList* feature_list_set_before_case_ = nullptr;
  ThreadPoolInstance* thread_pool_set_before_test_ = nullptr;
  ThreadPoolInstance* thread_pool_set_before_case_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CheckForLeakedGlobals);
};

// base::Process is not available on iOS
#if !defined(OS_IOS)
class CheckProcessPriority : public testing::EmptyTestEventListener {
 public:
  CheckProcessPriority() { CHECK(!IsProcessBackgrounded()); }

  void OnTestStart(const testing::TestInfo& test) override {
    EXPECT_FALSE(IsProcessBackgrounded());
  }
  void OnTestEnd(const testing::TestInfo& test) override {
#if !defined(OS_MAC)
    // Flakes are found on Mac OS 10.11. See https://crbug.com/931721#c7.
    EXPECT_FALSE(IsProcessBackgrounded());
#endif
  }

 private:
#if defined(OS_APPLE)
  // Returns the calling process's task port, ignoring its argument.
  class CurrentProcessPortProvider : public PortProvider {
    mach_port_t TaskForPid(ProcessHandle process) const override {
      // This PortProvider implementation only works for the current process.
      CHECK_EQ(process, base::GetCurrentProcessHandle());
      return mach_task_self();
    }
  };
#endif

  bool IsProcessBackgrounded() const {
#if defined(OS_APPLE)
    CurrentProcessPortProvider port_provider;
    return Process::Current().IsProcessBackgrounded(&port_provider);
#else
    return Process::Current().IsProcessBackgrounded();
#endif
  }

  DISALLOW_COPY_AND_ASSIGN(CheckProcessPriority);
};
#endif  // !defined(OS_IOS)

const std::string& GetProfileName() {
  static const NoDestructor<std::string> profile_name([]() {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (command_line.HasSwitch(switches::kProfilingFile))
      return command_line.GetSwitchValueASCII(switches::kProfilingFile);
    else
      return std::string("test-profile-{pid}");
  }());
  return *profile_name;
}

void InitializeLogging() {
  CHECK(logging::InitLogging({.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG |
                                              logging::LOG_TO_STDERR}));

  // We want process and thread IDs because we may have multiple processes.
#if defined(OS_ANDROID)
  // To view log output with IDs and timestamps use "adb logcat -v threadtime".
  logging::SetLogItems(false, false, false, false);
#else
  // We want process and thread IDs because we may have multiple processes.
  logging::SetLogItems(true, true, false, false);
#endif  // !defined(OS_ANDROID)
}

}  // namespace

int RunUnitTestsUsingBaseTestSuite(int argc, char** argv) {
  TestSuite test_suite(argc, argv);
  return LaunchUnitTests(argc, argv,
                         BindOnce(&TestSuite::Run, Unretained(&test_suite)));
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

  // The default death_test_style of "fast" is a frequent source of subtle test
  // flakiness. And on some platforms like macOS, use of system libraries after
  // fork() but before exec() is unsafe. Using the threadsafe style by default
  // alleviates these concerns.
  //
  // However, the threasafe style does not work reliably on Android, so that
  // will keep the default of "fast". See https://crbug.com/815537,
  // https://github.com/google/googletest/issues/1496, and
  // https://github.com/google/googletest/issues/2093.
  // TODO(danakj): Determine if all death tests should be skipped on Android
  // (many already are, such as for DCHECK-death tests).
#if !defined(OS_ANDROID)
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
#endif

#if defined(OS_WIN)
  testing::GTEST_FLAG(catch_exceptions) = false;
#endif
  EnableTerminationOnHeapCorruption();
#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(USE_AURA)
  // When calling native char conversion functions (e.g wrctomb) we need to
  // have the locale set. In the absence of such a call the "C" locale is the
  // default. In the gtk code (below) gtk_init() implicitly sets a locale.
  setlocale(LC_ALL, "");
  // We still need number to string conversions to be locale insensitive.
  setlocale(LC_NUMERIC, "C");
#endif  // (defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(USE_AURA)

  // On Android, AtExitManager is created in
  // testing/android/native_test_wrapper.cc before main() is called.
#if !defined(OS_ANDROID)
  at_exit_manager_ = std::make_unique<AtExitManager>();
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

#if defined(OS_APPLE)
  mac::ScopedNSAutoreleasePool scoped_pool;
#endif

  {
    // Some features are required to be checked as soon as possible. Thus, make
    // sure that the FeatureList is initalized before Initialize() is called so
    // that tests that rely on this call are able to check the enabled and
    // disabled featured passed via a command line.
    //
    // PS: When use_x11 and use_ozone are both true, some test suites need to
    // check if Ozone is being used during the Initialize() call below.
    // However, the feature list isn't initialized until later, when running
    // each test suite inside RUN_ALL_TESTS() below. Eagerly initialize a
    // ScopedFeatureList here to ensure the correct value is set for
    // feature::IsUsingOzonePlatform.
    //
    // TODO(https://crbug.com/1096425): Remove the comment about
    // UseOzonePlatform when USE_X11 is removed.
    std::string enabled =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kEnableFeatures);
    std::string disabled =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kDisableFeatures);
    base::test::ScopedFeatureList feature_list;
    feature_list.InitFromCommandLine(enabled, disabled);
    Initialize();
  }

  std::string client_func =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kTestChildProcess);

  // Check to see if we are being run as a client process.
  if (!client_func.empty())
    return multi_process_function_list::InvokeChildProcessTest(client_func);
#if defined(OS_IOS)
  test_listener_ios::RegisterTestEndListener();
#endif

#if defined(OS_LINUX)
  // There's no standard way to opt processes into MTE on Linux just yet,
  // so this call explicitly opts this test into synchronous MTE mode, where
  // pointer mismatches are detected immediately.
  base::memory::ChangeMemoryTaggingModeForCurrentThread(
      base::memory::TagViolationReportingMode::kSynchronous);
#elif defined(OS_ANDROID)
    // On Android, the tests are opted into synchronous MTE mode by the
    // memtagMode attribute in an AndroidManifest.xml file or via an `am compat`
    // command, so and explicit call to ChangeMemoryTaggingModeForCurrentThread
    // is not needed.
#endif

  int result = RUN_ALL_TESTS();

#if defined(OS_APPLE)
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

void TestSuite::DisableCheckForThreadAndProcessPriority() {
  DCHECK(!is_initialized_);
  check_for_thread_and_process_priority_ = false;
}

void TestSuite::UnitTestAssertHandler(const char* file,
                                      int line,
                                      const StringPiece summary,
                                      const StringPiece stack_trace) {
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
    const std::string summary_str(summary);
    const std::string stack_trace_str = summary_str + std::string(stack_trace);
    printer_->OnAssert(file, line, summary_str, stack_trace_str);
  }

  // The logging system actually prints the message before calling the assert
  // handler. Just exit now to avoid printing too many stack traces.
  _exit(1);
}

#if defined(OS_WIN)
namespace {

// Handlers for invalid parameter, pure call, and abort. They generate a
// breakpoint to ensure that we get a call stack on these failures.
// These functions should be written to be unique in order to avoid confusing
// call stacks from /OPT:ICF function folding. Printing a unique message or
// returning a unique value will do this. Note that for best results they need
// to be unique from *all* functions in Chrome.
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

}  // namespace
#endif

void TestSuite::SuppressErrorDialogs() {
#if defined(OS_WIN)
  UINT new_flags =
      SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX;

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

  test::ScopedRunLoopTimeout::SetAddGTestFailureOnTimeout();

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
#if !defined(OS_IOS)
  if (command_line->HasSwitch(switches::kWaitForDebugger)) {
    debug::WaitForDebugger(60, true);
  }
#endif

#if defined(DCHECK_IS_CONFIGURABLE)
  // Default the configurable DCHECK level to FATAL when running death tests'
  // child process, so that they behave as expected.
  // TODO(crbug.com/1057995): Remove this in favor of the codepath in
  // FeatureList::SetInstance() when/if OnTestStart() TestEventListeners
  // are fixed to be invoked in the child process as expected.
  if (command_line->HasSwitch("gtest_internal_run_death_test"))
    logging::LOGGING_DCHECK = logging::LOG_FATAL;
#endif

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
        BindRepeating(&TestSuite::UnitTestAssertHandler, Unretained(this)));
  }

  test::InitializeICUForTesting();

  // A number of tests only work if the locale is en_US. This can be an issue
  // on all platforms. To fix this we force the default locale to en_US. This
  // does not affect tests that explicitly overrides the locale for testing.
  // TODO(jshin): Should we set the locale via an OS X locale API here?
  i18n::SetICUDefaultLocale("en_US");

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  SetUpFontconfig();
#endif

  // Add TestEventListeners to enforce certain properties across tests.
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new DisableMaybeTests);
  listeners.Append(new ResetCommandLineBetweenTests);
  listeners.Append(new FeatureListScopedToEachTest);
  if (check_for_leaked_globals_)
    listeners.Append(new CheckForLeakedGlobals);
  if (check_for_thread_and_process_priority_) {
#if !defined(OS_IOS)
    listeners.Append(new CheckProcessPriority);
#endif
  }

  AddTestLauncherResultPrinter();

  TestTimeouts::Initialize();

#if BUILDFLAG(ENABLE_BASE_TRACING)
  trace_to_file_.BeginTracingFromCommandLineOptions();
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

  debug::StartProfiling(GetProfileName());

  debug::VerifyDebugger();

  is_initialized_ = true;
}

void TestSuite::Shutdown() {
  DCHECK(is_initialized_);
  debug::StopProfiling();
}

}  // namespace base
