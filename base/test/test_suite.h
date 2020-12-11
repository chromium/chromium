// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TEST_SUITE_H_
#define BASE_TEST_TEST_SUITE_H_

// Defines a basic test suite framework for running gtest based tests.  You can
// instantiate this class in your main function and call its Run method to run
// any gtest based tests that are linked into your executable.

#include <memory>
#include <string>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/macros.h"
#include "base/tracing_buildflags.h"
#include "build/build_config.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)
#include "base/test/trace_to_file.h"
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

namespace logging {
class ScopedLogAssertHandler;
}

namespace testing {
class TestInfo;
}

namespace base {

class XmlUnitTestResultPrinter;

// Instantiates TestSuite, runs it and returns exit code.
int RunUnitTestsUsingBaseTestSuite(int argc, char** argv);

class TestSuite {
 public:
  // Match function used by the GetTestCount method.
  typedef bool (*TestMatch)(const testing::TestInfo&);

  TestSuite(int argc, char** argv);
#if defined(OS_WIN)
  TestSuite(int argc, wchar_t** argv);
#endif  // defined(OS_WIN)
  virtual ~TestSuite();

  int Run();

  // Disables checks for thread and process priority at the beginning and end of
  // each test. Most tests should not use this.
  void DisableCheckForThreadAndProcessPriority();

  // Disables checks for certain global objects being leaked across tests.
  void DisableCheckForLeakedGlobals();

 protected:
  // By default fatal log messages (e.g. from DCHECKs) result in error dialogs
  // which gum up buildbots. Use a minimalistic assert handler which just
  // terminates the process.
  void UnitTestAssertHandler(const char* file,
                             int line,
                             const base::StringPiece summary,
                             const base::StringPiece stack_trace);

  // Disable crash dialogs so that it doesn't gum up the buildbot
  virtual void SuppressErrorDialogs();

  // Override these for custom initialization and shutdown handling.  Use these
  // instead of putting complex code in your constructor/destructor.

  virtual void Initialize();
  virtual void Shutdown();

  // Make sure that we setup an AtExitManager so Singleton objects will be
  // destroyed.
  std::unique_ptr<base::AtExitManager> at_exit_manager_;

 private:
  void AddTestLauncherResultPrinter();

  void InitializeFromCommandLine(int argc, char** argv);
#if defined(OS_WIN)
  void InitializeFromCommandLine(int argc, wchar_t** argv);
#endif  // defined(OS_WIN)

  // Basic initialization for the test suite happens here.
  void PreInitialize();

#if BUILDFLAG(ENABLE_BASE_TRACING)
  test::TraceToFile trace_to_file_;
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

  bool initialized_command_line_ = false;

  XmlUnitTestResultPrinter* printer_ = nullptr;

  std::unique_ptr<logging::ScopedLogAssertHandler> assert_handler_;

  bool check_for_leaked_globals_ = true;
  bool check_for_thread_and_process_priority_ = true;
  bool is_initialized_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestSuite);
};

}  // namespace base

#endif  // BASE_TEST_TEST_SUITE_H_
