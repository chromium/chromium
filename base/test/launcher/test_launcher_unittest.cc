// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/test_launcher.h"

#include <stddef.h>

#include <optional>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/gtest_xml_util.h"
#include "base/test/launcher/test_launcher_test_utils.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_logging_settings.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace base {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnPointee;

TestResult GenerateTestResult(const std::string& test_name,
                              TestResult::Status status,
                              TimeDelta elapsed_td = Milliseconds(30),
                              const std::string& output_snippet = "output") {
  TestResult result;
  result.full_name = test_name;
  result.status = status;
  result.elapsed_time = elapsed_td;
  result.output_snippet = output_snippet;
  return result;
}

TestResultPart GenerateTestResultPart(TestResultPart::Type type,
                                      const std::string& file_name,
                                      int line_number,
                                      const std::string& summary,
                                      const std::string& message) {
  TestResultPart test_result_part;
  test_result_part.type = type;
  test_result_part.file_name = file_name;
  test_result_part.line_number = line_number;
  test_result_part.summary = summary;
  test_result_part.message = message;
  return test_result_part;
}

// Mock TestLauncher to mock CreateAndStartThreadPool,
// unit test will provide a TaskEnvironment.
class MockTestLauncher : public TestLauncher {
 public:
  MockTestLauncher(TestLauncherDelegate* launcher_delegate,
                   size_t parallel_jobs)
      : TestLauncher(launcher_delegate, parallel_jobs) {}

  void CreateAndStartThreadPool(size_t parallel_jobs) override {}

  MOCK_METHOD4(LaunchChildGTestProcess,
               void(scoped_refptr<TaskRunner> task_runner,
                    const std::vector<std::string>& test_names,
                    const FilePath& task_temp_dir,
                    const FilePath& child_temp_dir));
};

// Simple TestLauncherDelegate mock to test TestLauncher flow.
class MockTestLauncherDelegate : public TestLauncherDelegate {
 public:
  MOCK_METHOD1(GetTests, bool(std::vector<TestIdentifier>* output));
  MOCK_METHOD2(WillRunTest,
               bool(const std::string& test_case_name,
                    const std::string& test_name));
  MOCK_METHOD2(ProcessTestResults,
               void(std::vector<TestResult>& test_names,
                    TimeDelta elapsed_time));
  MOCK_METHOD3(GetCommandLine,
               CommandLine(const std::vector<std::string>& test_names,
                           const FilePath& temp_dir_,
                           FilePath* output_file_));
  MOCK_METHOD1(IsPreTask, bool(const std::vector<std::string>& test_names));
  MOCK_METHOD0(GetWrapper, std::string());
  MOCK_METHOD0(GetLaunchOptions, int());
  MOCK_METHOD0(GetTimeout, TimeDelta());
  MOCK_METHOD0(GetBatchSize, size_t());
};

class MockResultWatcher : public ResultWatcher {
 public:
  MockResultWatcher(FilePath result_file, size_t num_tests)
      : ResultWatcher(result_file, num_tests) {}

  MOCK_METHOD(bool, WaitWithTimeout, (TimeDelta), (override));
};

// Using MockTestLauncher to test TestLauncher.
// Test TestLauncher filters, and command line switches setup.
class TestLauncherTest : public testing::Test {
 protected:
  TestLauncherTest()
      : command_line(new CommandLine(CommandLine::NO_PROGRAM)),
        test_launcher(&delegate, 10) {}

  // Adds tests to be returned by the delegate.
  void AddMockedTests(std::string test_case_name,
                      const std::vector<std::string>& test_names) {
    for (const std::string& test_name : test_names) {
      TestIdentifier test_data;
      test_data.test_case_name = test_case_name;
      test_data.test_name = test_name;
      test_data.file = "File";
      test_data.line = 100;
      tests_.push_back(test_data);
    }
  }

  // Setup expected delegate calls, and which tests the delegate will return.
  void SetUpExpectCalls(size_t batch_size = 10) {
    EXPECT_CALL(delegate, GetTests(_))
        .WillOnce(::testing::DoAll(testing::SetArgPointee<0>(tests_),
                                   testing::Return(true)));
    EXPECT_CALL(delegate, WillRunTest(_, _))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(delegate, ProcessTestResults(_, _)).Times(0);
    EXPECT_CALL(delegate, GetCommandLine(_, _, _))
        .WillRepeatedly(testing::Return(CommandLine(CommandLine::NO_PROGRAM)));
    EXPECT_CALL(delegate, GetWrapper())
        .WillRepeatedly(testing::Return(std::string()));
    EXPECT_CALL(delegate, IsPreTask(_)).WillRepeatedly(testing::Return(true));
    EXPECT_CALL(delegate, GetLaunchOptions())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(delegate, GetTimeout())
        .WillRepeatedly(testing::Return(TimeDelta()));
    EXPECT_CALL(delegate, GetBatchSize())
        .WillRepeatedly(testing::Return(batch_size));
  }

  std::unique_ptr<CommandLine> command_line;
  MockTestLauncher test_launcher;
  MockTestLauncherDelegate delegate;
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::IO};
  ScopedTempDir dir;

  FilePath CreateFilterFile() {
    FilePath result_file = dir.GetPath().AppendASCII("test.filter");
    WriteFile(result_file, "-Test.firstTest");
    return result_file;
  }

 private:
  std::vector<TestIdentifier> tests_;
};

class ResultWatcherTest : public testing::Test {
 protected:
  ResultWatcherTest() = default;

  FilePath CreateResultFile() {
    FilePath result_file = dir.GetPath().AppendASCII("test_results.xml");
    WriteFile(result_file,
              "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
              "<testsuites>\n"
              "  <testsuite>\n");
    return result_file;
  }

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedTempDir dir;
};

// Action to mock delegate invoking OnTestFinish on test launcher.
ACTION_P3(OnTestResult, launcher, full_name, status) {
  TestResult result = GenerateTestResult(full_name, status);
  arg0->PostTask(FROM_HERE, BindOnce(&TestLauncher::OnTestFinished,
                                     Unretained(launcher), result));
}

// Action to mock delegate invoking OnTestFinish on test launcher.
ACTION_P2(OnTestResult, launcher, result) {
  arg0->PostTask(FROM_HERE, BindOnce(&TestLauncher::OnTestFinished,
                                     Unretained(launcher), result));
}

// A test and a disabled test cannot share a name.
TEST_F(TestLauncherTest, TestNameSharedWithDisabledTest) {
  AddMockedTests("Test", {"firstTest", "DISABLED_firstTest"});
  SetUpExpectCalls();
  EXPECT_FALSE(test_launcher.Run(command_line.get()));
}

// A test case and a disabled test case cannot share a name.
TEST_F(TestLauncherTest, TestNameSharedWithDisabledTestCase) {
  AddMockedTests("DISABLED_Test", {"firstTest"});
  AddMockedTests("Test", {"firstTest"});
  SetUpExpectCalls();
  EXPECT_FALSE(test_launcher.Run(command_line.get()));
}

// Compiled tests should not contain an orphaned pre test.
TEST_F(TestLauncherTest, OrphanePreTest) {
  AddMockedTests("Test", {"firstTest", "PRE_firstTestOrphane"});
  SetUpExpectCalls();
  EXPECT_FALSE(test_launcher.Run(command_line.get()));
}

// When There are no tests, delegate should not be called.
TEST_F(TestLauncherTest, EmptyTestSetPasses) {
  SetUpExpectCalls();
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _)).Times(0);
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher filters DISABLED tests by default.
TEST_F(TestLauncherTest, FilterDisabledTestByDefault) {
  AddMockedTests("DISABLED_TestDisabled", {"firstTest"});
  AddMockedTests("Test",
                 {"firstTest", "secondTest", "DISABLED_firstTestDisabled"});
  SetUpExpectCalls();
  std::vector<std::string> tests_names = {"Test.firstTest", "Test.secondTest"};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .WillOnce(::testing::DoAll(OnTestResult(&test_launcher, "Test.firstTest",
                                              TestResult::TEST_SUCCESS),
                                 OnTestResult(&test_launcher, "Test.secondTest",
                                              TestResult::TEST_SUCCESS)));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher should reorder PRE_ tests before delegate
TEST_F(TestLauncherTest, ReorderPreTests) {
  AddMockedTests("Test", {"firstTest", "PRE_PRE_firstTest", "PRE_firstTest"});
  SetUpExpectCalls();
  std::vector<std::string> tests_names = {
      "Test.PRE_PRE_firstTest", "Test.PRE_firstTest", "Test.firstTest"};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .Times(1);
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher "gtest_filter" switch.
TEST_F(TestLauncherTest, UsingCommandLineFilter) {
  AddMockedTests("Test",
                 {"firstTest", "secondTest", "DISABLED_firstTestDisabled"});
  SetUpExpectCalls();
  command_line->AppendSwitchASCII("gtest_filter", "Test*.first*");
  std::vector<std::string> tests_names = {"Test.firstTest"};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .WillOnce(OnTestResult(&test_launcher, "Test.firstTest",
                             TestResult::TEST_SUCCESS));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher gtest filter will include pre tests
TEST_F(TestLauncherTest, FilterIncludePreTest) {
  AddMockedTests("Test", {"firstTest", "secondTest", "PRE_firstTest"});
  SetUpExpectCalls();
  command_line->AppendSwitchASCII("gtest_filter", "Test.firstTest");
  std::vector<std::string> tests_names = {"Test.PRE_firstTest",
                                          "Test.firstTest"};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .Times(1);
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher gtest filter works when both include and exclude filter
// are defined.
TEST_F(TestLauncherTest, FilterIncludeExclude) {
  AddMockedTests("Test", {"firstTest", "PRE_firstTest", "secondTest",
                          "PRE_secondTest", "thirdTest", "DISABLED_Disable1"});
  SetUpExpectCalls();
  command_line->AppendSwitchASCII("gtest_filter",
                                  "Test.*Test:-Test.secondTest");
  std::vector<std::string> tests_names = {
      "Test.PRE_firstTest",
      "Test.firstTest",
      "Test.thirdTest",
  };
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .Times(1);
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher "gtest_repeat" switch.
TEST_F(TestLauncherTest, RepeatTest) {
  AddMockedTests("Test", {"firstTest"});
  SetUpExpectCalls();
  // Unless --gtest-break-on-failure is specified,
  command_line->AppendSwitchASCII("gtest_repeat", "2");
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _))
      .Times(2)
      .WillRepeatedly(::testing::DoAll(OnTestResult(
          &test_launcher, "Test.firstTest", TestResult::TEST_SUCCESS)));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher --gtest_repeat and --gtest_break_on_failure.
TEST_F(TestLauncherTest, RunningMultipleIterationsUntilFailure) {
  AddMockedTests("Test", {"firstTest"});
  SetUpExpectCalls();
  // Unless --gtest-break-on-failure is specified,
  command_line->AppendSwitchASCII("gtest_repeat", "4");
  command_line->AppendSwitch("gtest_break_on_failure");
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _))
      .WillOnce(::testing::DoAll(OnTestResult(&test_launcher, "Test.firstTest",
                                              TestResult::TEST_SUCCESS)))
      .WillOnce(::testing::DoAll(OnTestResult(&test_launcher, "Test.firstTest",
                                              TestResult::TEST_SUCCESS)))
      .WillOnce(::testing::DoAll(OnTestResult(&test_launcher, "Test.firstTest",
                                              TestResult::TEST_FAILURE)));
  EXPECT_FALSE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher will retry failed test, and stop on success.
TEST_F(TestLauncherTest, SuccessOnRetryTests) {
  AddMockedTests("Test", {"firstTest"});
  SetUpExpectCalls();
  command_line->AppendSwitchASCII("test-launcher-retry-limit", "2");
  std::vector<std::string> tests_names = {"Test.firstTest"};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .WillOnce(OnTestResult(&test_launcher, "Test.firstTest",
                             TestResult::TEST_FAILURE))
      .WillOnce(OnTestResult(&test_launcher, "Test.firstTest",
                             TestResult::TEST_SUCCESS));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher will retry continuing failing test up to retry limit,
// before eventually failing and returning false.
TEST_F(TestLauncherTest, FailOnRetryTests) {
  AddMockedTests("Test", {"firstTest"});
  SetUpExpectCalls();
  command_line->AppendSwitchASCII("test-launcher-retry-limit", "2");
  std::vector<std::string> tests_names = {"Test.firstTest"};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .Times(3)
      .WillRepeatedly(OnTestResult(&test_launcher, "Test.firstTest",
                                   TestResult::TEST_FAILURE));
  EXPECT_FALSE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher should retry all PRE_ chained tests
TEST_F(TestLauncherTest, RetryPreTests) {
  AddMockedTests("Test", {"firstTest", "PRE_PRE_firstTest", "PRE_firstTest"});
  SetUpExpectCalls();
  command_line->AppendSwitchASCII("test-launcher-retry-limit", "2");
  std::vector<TestResult> results = {
      GenerateTestResult("Test.PRE_PRE_firstTest", TestResult::TEST_SUCCESS),
      GenerateTestResult("Test.PRE_firstTest", TestResult::TEST_FAILURE),
      GenerateTestResult("Test.firstTest", TestResult::TEST_SUCCESS)};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _))
      .WillOnce(::testing::DoAll(
          OnTestResult(&test_launcher, "Test.PRE_PRE_firstTest",
                       TestResult::TEST_SUCCESS),
          OnTestResult(&test_launcher, "Test.PRE_firstTest",
                       TestResult::TEST_FAILURE),
          OnTestResult(&test_launcher, "Test.firstTest",
                       TestResult::TEST_SUCCESS)));
  std::vector<std::string> tests_names = {"Test.PRE_PRE_firstTest"};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .WillOnce(OnTestResult(&test_launcher, "Test.PRE_PRE_firstTest",
                             TestResult::TEST_SUCCESS));
  tests_names = {"Test.PRE_firstTest"};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .WillOnce(OnTestResult(&test_launcher, "Test.PRE_firstTest",
                             TestResult::TEST_SUCCESS));
  tests_names = {"Test.firstTest"};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .WillOnce(OnTestResult(&test_launcher, "Test.firstTest",
                             TestResult::TEST_SUCCESS));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher should fail if a PRE test fails but its non-PRE test passes
TEST_F(TestLauncherTest, PreTestFailure) {
  AddMockedTests("Test", {"FirstTest", "PRE_FirstTest"});
  SetUpExpectCalls();
  std::vector<TestResult> results = {
      GenerateTestResult("Test.PRE_FirstTest", TestResult::TEST_FAILURE),
      GenerateTestResult("Test.FirstTest", TestResult::TEST_SUCCESS)};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _))
      .WillOnce(
          ::testing::DoAll(OnTestResult(&test_launcher, "Test.PRE_FirstTest",
                                        TestResult::TEST_FAILURE),
                           OnTestResult(&test_launcher, "Test.FirstTest",
                                        TestResult::TEST_SUCCESS)));
  EXPECT_CALL(test_launcher,
              LaunchChildGTestProcess(
                  _, testing::ElementsAre("Test.PRE_FirstTest"), _, _))
      .WillOnce(OnTestResult(&test_launcher, "Test.PRE_FirstTest",
                             TestResult::TEST_FAILURE));
  EXPECT_CALL(
      test_launcher,
      LaunchChildGTestProcess(_, testing::ElementsAre("Test.FirstTest"), _, _))
      .WillOnce(OnTestResult(&test_launcher, "Test.FirstTest",
                             TestResult::TEST_SUCCESS));
  EXPECT_FALSE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher run disabled unit tests switch.
TEST_F(TestLauncherTest, RunDisabledTests) {
  AddMockedTests("DISABLED_TestDisabled", {"firstTest"});
  AddMockedTests("Test",
                 {"firstTest", "secondTest", "DISABLED_firstTestDisabled"});
  SetUpExpectCalls();
  command_line->AppendSwitch("gtest_also_run_disabled_tests");
  command_line->AppendSwitchASCII("gtest_filter", "Test*.first*");
  std::vector<std::string> tests_names = {"DISABLED_TestDisabled.firstTest",
                                          "Test.firstTest",
                                          "Test.DISABLED_firstTestDisabled"};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .WillOnce(::testing::DoAll(
          OnTestResult(&test_launcher, "Test.firstTest",
                       TestResult::TEST_SUCCESS),
          OnTestResult(&test_launcher, "DISABLED_TestDisabled.firstTest",
                       TestResult::TEST_SUCCESS),
          OnTestResult(&test_launcher, "Test.DISABLED_firstTestDisabled",
                       TestResult::TEST_SUCCESS)));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher does not run negative tests filtered under
// testing/buildbot/filters.
TEST_F(TestLauncherTest, DoesRunFilteredTests) {
  AddMockedTests("Test", {"firstTest", "secondTest"});
  SetUpExpectCalls();
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  // filter file content is "-Test.firstTest"
  FilePath path = CreateFilterFile();
  command_line->AppendSwitchPath("test-launcher-filter-file", path);
  std::vector<std::string> tests_names = {"Test.secondTest"};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .WillOnce(::testing::DoAll(OnTestResult(&test_launcher, "Test.secondTest",
                                              TestResult::TEST_SUCCESS)));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher run disabled tests and negative tests filtered under
// testing/buildbot/filters, when gtest_also_run_disabled_tests is set.
TEST_F(TestLauncherTest, RunDisabledTestsWithFilteredTests) {
  AddMockedTests("DISABLED_TestDisabled", {"firstTest"});
  AddMockedTests("Test", {"firstTest", "DISABLED_firstTestDisabled"});
  SetUpExpectCalls();
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  // filter file content is "-Test.firstTest", but Test.firstTest will still
  // run due to gtest_also_run_disabled_tests is set.
  FilePath path = CreateFilterFile();
  command_line->AppendSwitchPath("test-launcher-filter-file", path);
  command_line->AppendSwitch("gtest_also_run_disabled_tests");
  std::vector<std::string> tests_names = {"DISABLED_TestDisabled.firstTest",
                                          "Test.firstTest",
                                          "Test.DISABLED_firstTestDisabled"};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .WillOnce(::testing::DoAll(
          OnTestResult(&test_launcher, "Test.firstTest",
                       TestResult::TEST_SUCCESS),
          OnTestResult(&test_launcher, "DISABLED_TestDisabled.firstTest",
                       TestResult::TEST_SUCCESS),
          OnTestResult(&test_launcher, "Test.DISABLED_firstTestDisabled",
                       TestResult::TEST_SUCCESS)));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Disabled test should disable all pre tests
TEST_F(TestLauncherTest, DisablePreTests) {
  AddMockedTests("Test", {"DISABLED_firstTest", "PRE_PRE_firstTest",
                          "PRE_firstTest", "secondTest"});
  SetUpExpectCalls();
  std::vector<std::string> tests_names = {"Test.secondTest"};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .Times(1);
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher enforce to run tests in the exact positive filter.
TEST_F(TestLauncherTest, EnforceRunTestsInExactPositiveFilter) {
  AddMockedTests("Test", {"firstTest", "secondTest", "thirdTest"});
  SetUpExpectCalls();

  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath path = dir.GetPath().AppendASCII("test.filter");
  WriteFile(path, "Test.firstTest\nTest.thirdTest");
  command_line->AppendSwitchPath("test-launcher-filter-file", path);
  command_line->AppendSwitch("enforce-exact-positive-filter");
  command_line->AppendSwitchASCII("test-launcher-total-shards", "2");
  command_line->AppendSwitchASCII("test-launcher-shard-index", "0");

  // Test.firstTest is in the exact positive filter, so expected to run.
  // Test.thirdTest is launched in another shard.
  std::vector<std::string> tests_names = {"Test.firstTest"};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .WillOnce(::testing::DoAll(OnTestResult(&test_launcher, "Test.firstTest",
                                              TestResult::TEST_SUCCESS)));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher should fail if enforce-exact-positive-filter and
// gtest_filter both presented.
TEST_F(TestLauncherTest,
       EnforceRunTestsInExactPositiveFailWithGtestFilterFlag) {
  command_line->AppendSwitch("enforce-exact-positive-filter");
  command_line->AppendSwitchASCII("gtest_filter", "Test.firstTest;-Test.*");
  EXPECT_FALSE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher should fail if enforce-exact-positive-filter is set
// with negative test filters.
TEST_F(TestLauncherTest, EnforceRunTestsInExactPositiveFailWithNegativeFilter) {
  command_line->AppendSwitch("enforce-exact-positive-filter");
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath path = CreateFilterFile();
  command_line->AppendSwitchPath("test-launcher-filter-file", path);
  EXPECT_FALSE(test_launcher.Run(command_line.get()));
}

// Test TestLauncher should fail if enforce-exact-positive-filter is set
// with wildcard positive filters.
TEST_F(TestLauncherTest,
       EnforceRunTestsInExactPositiveFailWithWildcardPositiveFilter) {
  command_line->AppendSwitch("enforce-exact-positive-filter");
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath path = dir.GetPath().AppendASCII("test.filter");
  WriteFile(path, "Test.*");
  command_line->AppendSwitchPath("test-launcher-filter-file", path);
  EXPECT_FALSE(test_launcher.Run(command_line.get()));
}

// Tests fail if they produce too much output.
TEST_F(TestLauncherTest, ExcessiveOutput) {
  AddMockedTests("Test", {"firstTest"});
  SetUpExpectCalls();
  command_line->AppendSwitchASCII("test-launcher-retry-limit", "0");
  command_line->AppendSwitchASCII("test-launcher-print-test-stdio", "never");
  TestResult test_result =
      GenerateTestResult("Test.firstTest", TestResult::TEST_SUCCESS,
                         Milliseconds(30), std::string(500000, 'a'));
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _))
      .WillOnce(OnTestResult(&test_launcher, test_result));
  EXPECT_FALSE(test_launcher.Run(command_line.get()));
}

// Use command-line switch to allow more output.
TEST_F(TestLauncherTest, OutputLimitSwitch) {
  AddMockedTests("Test", {"firstTest"});
  SetUpExpectCalls();
  command_line->AppendSwitchASCII("test-launcher-print-test-stdio", "never");
  command_line->AppendSwitchASCII("test-launcher-output-bytes-limit", "800000");
  TestResult test_result =
      GenerateTestResult("Test.firstTest", TestResult::TEST_SUCCESS,
                         Milliseconds(30), std::string(500000, 'a'));
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _))
      .WillOnce(OnTestResult(&test_launcher, test_result));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Shard index must be lesser than total shards
TEST_F(TestLauncherTest, FaultyShardSetup) {
  command_line->AppendSwitchASCII("test-launcher-total-shards", "2");
  command_line->AppendSwitchASCII("test-launcher-shard-index", "2");
  EXPECT_FALSE(test_launcher.Run(command_line.get()));
}

// Shard index must be lesser than total shards
TEST_F(TestLauncherTest, RedirectStdio) {
  AddMockedTests("Test", {"firstTest"});
  SetUpExpectCalls();
  command_line->AppendSwitchASCII("test-launcher-print-test-stdio", "always");
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _))
      .WillOnce(OnTestResult(&test_launcher, "Test.firstTest",
                             TestResult::TEST_SUCCESS));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Sharding should be stable and always selecting the same tests.
TEST_F(TestLauncherTest, StableSharding) {
  AddMockedTests("Test", {"firstTest", "secondTest", "thirdTest"});
  SetUpExpectCalls();
  command_line->AppendSwitchASCII("test-launcher-total-shards", "2");
  command_line->AppendSwitchASCII("test-launcher-shard-index", "0");
  command_line->AppendSwitch("test-launcher-stable-sharding");
  std::vector<std::string> tests_names = {"Test.firstTest", "Test.secondTest"};
  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(
                                 _,
                                 testing::ElementsAreArray(tests_names.cbegin(),
                                                           tests_names.cend()),
                                 _, _))
      .WillOnce(::testing::DoAll(OnTestResult(&test_launcher, "Test.firstTest",
                                              TestResult::TEST_SUCCESS),
                                 OnTestResult(&test_launcher, "Test.secondTest",
                                              TestResult::TEST_SUCCESS)));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));
}

// Validate |iteration_data| contains one test result matching |result|.
bool ValidateTestResultObject(const Value::Dict& iteration_data,
                              TestResult& test_result) {
  const Value::List* results = iteration_data.FindList(test_result.full_name);
  if (!results) {
    ADD_FAILURE() << "Results not found";
    return false;
  }
  if (1u != results->size()) {
    ADD_FAILURE() << "Expected one result, actual: " << results->size();
    return false;
  }
  const Value::Dict* dict = (*results)[0].GetIfDict();
  if (!dict) {
    ADD_FAILURE() << "Unexpected type";
    return false;
  }

  using test_launcher_utils::ValidateKeyValue;
  bool result = ValidateKeyValue(*dict, "elapsed_time_ms",
                                 test_result.elapsed_time.InMilliseconds());

  if (!dict->FindBool("losless_snippet").value_or(false)) {
    ADD_FAILURE() << "losless_snippet expected to be true";
    result = false;
  }

  result &=
      ValidateKeyValue(*dict, "output_snippet", test_result.output_snippet);

  std::string base64_output_snippet =
      base::Base64Encode(test_result.output_snippet);
  result &=
      ValidateKeyValue(*dict, "output_snippet_base64", base64_output_snippet);

  result &= ValidateKeyValue(*dict, "status", test_result.StatusAsString());

  const Value::List* list = dict->FindList("result_parts");
  if (test_result.test_result_parts.size() != list->size()) {
    ADD_FAILURE() << "test_result_parts count is not valid";
    return false;
  }

  for (unsigned i = 0; i < test_result.test_result_parts.size(); i++) {
    TestResultPart result_part = test_result.test_result_parts.at(i);
    const Value::Dict& part_dict = (*list)[i].GetDict();

    result &= ValidateKeyValue(part_dict, "type", result_part.TypeAsString());
    result &= ValidateKeyValue(part_dict, "file", result_part.file_name);
    result &= ValidateKeyValue(part_dict, "line", result_part.line_number);
    result &= ValidateKeyValue(part_dict, "summary", result_part.summary);
    result &= ValidateKeyValue(part_dict, "message", result_part.message);
  }
  return result;
}

// Validate |root| dictionary value contains a list with |values|
// at |key| value.
bool ValidateStringList(const std::optional<Value::Dict>& root,
                        const std::string& key,
                        std::vector<const char*> values) {
  const Value::List* list = root->FindList(key);
  if (!list) {
    ADD_FAILURE() << "|root| has no list_value in key: " << key;
    return false;
  }

  if (values.size() != list->size()) {
    ADD_FAILURE() << "expected size: " << values.size()
                  << ", actual size:" << list->size();
    return false;
  }

  for (unsigned i = 0; i < values.size(); i++) {
    if (!(*list)[i].is_string() &&
        (*list)[i].GetString().compare(values.at(i))) {
      ADD_FAILURE() << "Expected list values do not match actual list";
      return false;
    }
  }
  return true;
}

// Unit tests to validate TestLauncher outputs the correct JSON file.
TEST_F(TestLauncherTest, JsonSummary) {
  AddMockedTests("DISABLED_TestDisabled", {"firstTest"});
  AddMockedTests("Test",
                 {"firstTest", "secondTest", "DISABLED_firstTestDisabled"});
  SetUpExpectCalls();

  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath path = dir.GetPath().AppendASCII("SaveSummaryResult.json");
  command_line->AppendSwitchPath("test-launcher-summary-output", path);
  command_line->AppendSwitchASCII("gtest_repeat", "2");
  // Force the repeats to run sequentially.
  command_line->AppendSwitch("gtest_break_on_failure");

  // Setup results to be returned by the test launcher delegate.
  TestResult first_result =
      GenerateTestResult("Test.firstTest", TestResult::TEST_SUCCESS,
                         Milliseconds(30), "output_first");
  first_result.test_result_parts.push_back(GenerateTestResultPart(
      TestResultPart::kSuccess, "TestFile", 110, "summary", "message"));
  TestResult second_result =
      GenerateTestResult("Test.secondTest", TestResult::TEST_SUCCESS,
                         Milliseconds(50), "output_second");

  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _))
      .Times(2)
      .WillRepeatedly(
          ::testing::DoAll(OnTestResult(&test_launcher, first_result),
                           OnTestResult(&test_launcher, second_result)));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));

  // Validate the resulting JSON file is the expected output.
  std::optional<Value::Dict> root = test_launcher_utils::ReadSummary(path);
  ASSERT_TRUE(root);
  EXPECT_TRUE(
      ValidateStringList(root, "all_tests",
                         {"Test.firstTest", "Test.firstTestDisabled",
                          "Test.secondTest", "TestDisabled.firstTest"}));
  EXPECT_TRUE(
      ValidateStringList(root, "disabled_tests",
                         {"Test.firstTestDisabled", "TestDisabled.firstTest"}));

  const Value::Dict* dict = root->FindDict("test_locations");
  ASSERT_TRUE(dict);
  EXPECT_EQ(2u, dict->size());
  ASSERT_TRUE(test_launcher_utils::ValidateTestLocation(*dict, "Test.firstTest",
                                                        "File", 100));
  ASSERT_TRUE(test_launcher_utils::ValidateTestLocation(
      *dict, "Test.secondTest", "File", 100));

  const Value::List* list = root->FindList("per_iteration_data");
  ASSERT_TRUE(list);
  ASSERT_EQ(2u, list->size());
  for (const auto& iteration_val : *list) {
    ASSERT_TRUE(iteration_val.is_dict());
    const base::Value::Dict& iteration_dict = iteration_val.GetDict();
    EXPECT_EQ(2u, iteration_dict.size());
    EXPECT_TRUE(ValidateTestResultObject(iteration_dict, first_result));
    EXPECT_TRUE(ValidateTestResultObject(iteration_dict, second_result));
  }
}

// Validate TestLauncher outputs the correct JSON file
// when running disabled tests.
TEST_F(TestLauncherTest, JsonSummaryWithDisabledTests) {
  AddMockedTests("Test", {"DISABLED_Test"});
  SetUpExpectCalls();

  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath path = dir.GetPath().AppendASCII("SaveSummaryResult.json");
  command_line->AppendSwitchPath("test-launcher-summary-output", path);
  command_line->AppendSwitch("gtest_also_run_disabled_tests");

  // Setup results to be returned by the test launcher delegate.
  TestResult test_result =
      GenerateTestResult("Test.DISABLED_Test", TestResult::TEST_SUCCESS,
                         Milliseconds(50), "output_second");

  EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _))
      .WillOnce(OnTestResult(&test_launcher, test_result));
  EXPECT_TRUE(test_launcher.Run(command_line.get()));

  // Validate the resulting JSON file is the expected output.
  std::optional<Value::Dict> root = test_launcher_utils::ReadSummary(path);
  ASSERT_TRUE(root);
  Value::Dict* dict = root->FindDict("test_locations");
  ASSERT_TRUE(dict);
  EXPECT_EQ(1u, dict->size());
  EXPECT_TRUE(test_launcher_utils::ValidateTestLocation(
      *dict, "Test.DISABLED_Test", "File", 100));

  Value::List* list = root->FindList("per_iteration_data");
  ASSERT_TRUE(list);
  ASSERT_EQ(1u, list->size());

  Value::Dict* iteration_dict = (*list)[0].GetIfDict();
  ASSERT_TRUE(iteration_dict);
  EXPECT_EQ(1u, iteration_dict->size());
  // We expect the result to be stripped of disabled prefix.
  test_result.full_name = "Test.Test";
  EXPECT_TRUE(ValidateTestResultObject(*iteration_dict, test_result));
}

// Matches a std::tuple<const FilePath&, const FilePath&> where the first
// item is a parent of the second.
MATCHER(DirectoryIsParentOf, "") {
  return std::get<0>(arg).IsParent(std::get<1>(arg));
}

// Test that the launcher creates a dedicated temp dir for a child proc and
// cleans it up.
TEST_F(TestLauncherTest, TestChildTempDir) {
  AddMockedTests("Test", {"firstTest"});
  SetUpExpectCalls();
  ON_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, _))
      .WillByDefault(OnTestResult(&test_launcher, "Test.firstTest",
                                  TestResult::TEST_SUCCESS));

  FilePath task_temp;
  if (TestLauncher::SupportsPerChildTempDirs()) {
    // Platforms that support child proc temp dirs must get a |child_temp_dir|
    // arg that exists and is within |task_temp_dir|.
    EXPECT_CALL(
        test_launcher,
        LaunchChildGTestProcess(
            _, _, _, ::testing::ResultOf(DirectoryExists, ::testing::IsTrue())))
        .With(::testing::Args<2, 3>(DirectoryIsParentOf()))
        .WillOnce(::testing::SaveArg<2>(&task_temp));
  } else {
    // Platforms that don't support child proc temp dirs must get an empty
    // |child_temp_dir| arg.
    EXPECT_CALL(test_launcher, LaunchChildGTestProcess(_, _, _, FilePath()))
        .WillOnce(::testing::SaveArg<2>(&task_temp));
  }

  EXPECT_TRUE(test_launcher.Run(command_line.get()));

  // The task's temporary directory should have been deleted.
  EXPECT_FALSE(DirectoryExists(task_temp));
}

#if BUILDFLAG(IS_FUCHSIA)
// Verifies that test processes have /data, /cache and /tmp available.
TEST_F(TestLauncherTest, ProvidesDataCacheAndTmpDirs) {
  EXPECT_TRUE(base::DirectoryExists(base::FilePath("/data")));
  EXPECT_TRUE(base::DirectoryExists(base::FilePath("/cache")));
  EXPECT_TRUE(base::DirectoryExists(base::FilePath("/tmp")));
}
#endif  // BUILDFLAG(IS_FUCHSIA)

// Unit tests to validate UnitTestLauncherDelegate implementation.
class UnitTestLauncherDelegateTester : public testing::Test {
 protected:
  DefaultUnitTestPlatformDelegate defaultPlatform;
  ScopedTempDir dir;

 private:
  base::test::TaskEnvironment task_environment;
};

// Validate delegate produces correct command line.
TEST_F(UnitTestLauncherDelegateTester, GetCommandLine) {
  UnitTestLauncherDelegate launcher_delegate(&defaultPlatform, 10u, true,
                                             DoNothing());
  TestLauncherDelegate* delegate_ptr = &launcher_delegate;

  std::vector<std::string> test_names(5, "Tests");
  base::FilePath temp_dir;
  base::FilePath result_file;
  CreateNewTempDirectory(FilePath::StringType(), &temp_dir);

  // Make sure that `--gtest_filter` from the original command line is dropped
  // from a command line passed to the child process, since `--gtest_filter` is
  // also specified in the flagfile.
  CommandLine::ForCurrentProcess()->AppendSwitchASCII("gtest_filter", "*");
  // But other random flags should be preserved.
  CommandLine::ForCurrentProcess()->AppendSwitch("mochi-are-delicious");
  CommandLine cmd_line =
      delegate_ptr->GetCommandLine(test_names, temp_dir, &result_file);
  EXPECT_TRUE(cmd_line.HasSwitch("single-process-tests"));
  EXPECT_FALSE(cmd_line.HasSwitch("gtest_filter"));
  EXPECT_TRUE(cmd_line.HasSwitch("mochi-are-delicious"));
  EXPECT_EQ(cmd_line.GetSwitchValuePath("test-launcher-output"), result_file);

  const int size = 2048;
  std::string content;
  ASSERT_TRUE(ReadFileToStringWithMaxSize(
      cmd_line.GetSwitchValuePath("gtest_flagfile"), &content, size));
  EXPECT_EQ(content.find("--gtest_filter="), 0u);
  base::ReplaceSubstringsAfterOffset(&content, 0, "--gtest_filter=", "");
  std::vector<std::string> gtest_filter_tests =
      SplitString(content, ":", TRIM_WHITESPACE, SPLIT_WANT_ALL);
  ASSERT_EQ(gtest_filter_tests.size(), test_names.size());
  for (unsigned i = 0; i < test_names.size(); i++) {
    EXPECT_EQ(gtest_filter_tests.at(i), test_names.at(i));
  }
}

// Verify that a result watcher can stop polling early when all tests complete.
TEST_F(ResultWatcherTest, PollCompletesQuickly) {
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath result_file = CreateResultFile();
  ASSERT_TRUE(AppendToFile(
      result_file,
      StrCat({"    <x-teststart name=\"B\" classname=\"A\" timestamp=\"",
              TimeFormatAsIso8601(Time::Now()).c_str(), "\" />\n",
              "    <testcase name=\"B\" status=\"run\" time=\"0.500\" "
              "classname=\"A\" timestamp=\"",
              TimeFormatAsIso8601(Time::Now()).c_str(), "\">\n",
              "    </testcase>\n",
              "    <x-teststart name=\"C\" classname=\"A\" timestamp=\"",
              TimeFormatAsIso8601(Time::Now() + Milliseconds(500)).c_str(),
              "\" />\n",
              "    <testcase name=\"C\" status=\"run\" time=\"0.500\" "
              "classname=\"A\" timestamp=\"",
              TimeFormatAsIso8601(Time::Now() + Milliseconds(500)).c_str(),
              "\">\n", "    </testcase>\n", "  </testsuite>\n",
              "</testsuites>\n"})));

  MockResultWatcher result_watcher(result_file, 2);
  EXPECT_CALL(result_watcher, WaitWithTimeout(_))
      .WillOnce(DoAll(InvokeWithoutArgs([&] {
                        task_environment.AdvanceClock(Milliseconds(1500));
                      }),
                      Return(true)));

  Time start = Time::Now();
  ASSERT_TRUE(result_watcher.PollUntilDone(Seconds(45)));
  ASSERT_EQ(Time::Now() - start, Milliseconds(1500));
}

// Verify that a result watcher repeatedly checks the file for a batch of slow
// tests. Each test completes in 40s, which is just under the timeout of 45s.
TEST_F(ResultWatcherTest, PollCompletesSlowly) {
  SCOPED_TRACE(::testing::Message() << "Start ticks: " << TimeTicks::Now());

  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath result_file = CreateResultFile();
  const Time start = Time::Now();
  ASSERT_TRUE(AppendToFile(
      result_file,
      StrCat({"    <x-teststart name=\"B\" classname=\"A\" timestamp=\"",
              TimeFormatAsIso8601(start).c_str(), "\" />\n"})));

  MockResultWatcher result_watcher(result_file, 10);
  size_t checks = 0;
  bool done = false;
  EXPECT_CALL(result_watcher, WaitWithTimeout(_))
      .Times(10)
      .WillRepeatedly(
          DoAll(Invoke([&](TimeDelta timeout) {
                  task_environment.AdvanceClock(timeout);
                  // Append a result with "time" (duration) as 40.000s and
                  // "timestamp" (test start) as `Now()` - 45s.
                  AppendToFile(
                      result_file,
                      StrCat({"    <testcase name=\"B\" status=\"run\" "
                              "time=\"40.000\" classname=\"A\" timestamp=\"",
                              TimeFormatAsIso8601(Time::Now() - Seconds(45))
                                  .c_str(),
                              "\">\n", "    </testcase>\n"}));
                  checks++;
                  if (checks == 10) {
                    AppendToFile(result_file,
                                 "  </testsuite>\n"
                                 "</testsuites>\n");
                    done = true;
                  } else {
                    // Append a preliminary result for the next test that
                    // started when the last test completed (i.e., `Now()` - 45s
                    // + 40s).
                    AppendToFile(
                        result_file,
                        StrCat({"    <x-teststart name=\"B\" classname=\"A\" "
                                "timestamp=\"",
                                TimeFormatAsIso8601(Time::Now() - Seconds(5))
                                    .c_str(),
                                "\" />\n"}));
                  }
                }),
                ReturnPointee(&done)));

  ASSERT_TRUE(result_watcher.PollUntilDone(Seconds(45)));
  // The first check occurs 45s after the batch starts, so the sequence of
  // events looks like:
  //   00:00 - Test 1 starts
  //   00:40 - Test 1 completes, test 2 starts
  //   00:45 - Check 1 occurs
  //   01:20 - Test 2 completes, test 3 starts
  //   01:25 - Check 2 occurs
  //   02:00 - Test 3 completes, test 4 starts
  //   02:05 - Check 3 occurs
  //   ...
  ASSERT_EQ(Time::Now() - start, Seconds(45 + 40 * 9));
}

// Verify that the result watcher identifies when a test times out.
TEST_F(ResultWatcherTest, PollTimeout) {
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath result_file = CreateResultFile();
  ASSERT_TRUE(AppendToFile(
      result_file,
      StrCat({"    <x-teststart name=\"B\" classname=\"A\" timestamp=\"",
              TimeFormatAsIso8601(Time::Now()).c_str(), "\" />\n"})));

  MockResultWatcher result_watcher(result_file, 10);
  EXPECT_CALL(result_watcher, WaitWithTimeout(_))
      .Times(2)
      .WillRepeatedly(
          DoAll(Invoke(&task_environment, &test::TaskEnvironment::AdvanceClock),
                Return(false)));

  Time start = Time::Now();
  ASSERT_FALSE(result_watcher.PollUntilDone(Seconds(45)));
  // Include a small grace period.
  ASSERT_EQ(Time::Now() - start, Seconds(45) + TestTimeouts::tiny_timeout());
}

// Verify that the result watcher retries incomplete reads.
TEST_F(ResultWatcherTest, RetryIncompleteResultRead) {
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath result_file = CreateResultFile();
  // Opening "<summary>" tag is not closed.
  ASSERT_TRUE(AppendToFile(
      result_file,
      StrCat({"    <x-teststart name=\"B\" classname=\"A\" timestamp=\"",
              TimeFormatAsIso8601(Time::Now()).c_str(), "\" />\n",
              "    <testcase name=\"B\" status=\"run\" time=\"40.000\" "
              "classname=\"A\" timestamp=\"",
              TimeFormatAsIso8601(Time::Now()).c_str(), "\">\n",
              "      <summary>"})));

  MockResultWatcher result_watcher(result_file, 2);
  size_t attempts = 0;
  bool done = false;
  EXPECT_CALL(result_watcher, WaitWithTimeout(_))
      .Times(5)
      .WillRepeatedly(DoAll(Invoke([&](TimeDelta timeout) {
                              task_environment.AdvanceClock(timeout);
                              // Don't bother writing the rest of the file when
                              // this test completes.
                              done = ++attempts >= 5;
                            }),
                            ReturnPointee(&done)));

  Time start = Time::Now();
  ASSERT_TRUE(result_watcher.PollUntilDone(Seconds(45)));
  ASSERT_EQ(Time::Now() - start,
            Seconds(45) + 4 * TestTimeouts::tiny_timeout());
}

// Verify that the result watcher continues polling with the base timeout when
// the clock jumps backward.
TEST_F(ResultWatcherTest, PollWithClockJumpBackward) {
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath result_file = CreateResultFile();
  // Cannot move the mock time source backward, so write future timestamps into
  // the result file instead.
  Time time_before_change = Time::Now() + Hours(1);
  ASSERT_TRUE(AppendToFile(
      result_file,
      StrCat(
          {"    <x-teststart name=\"B\" classname=\"A\" timestamp=\"",
           TimeFormatAsIso8601(time_before_change).c_str(), "\" />\n",
           "    <testcase name=\"B\" status=\"run\" time=\"0.500\" "
           "classname=\"A\" timestamp=\"",
           TimeFormatAsIso8601(time_before_change).c_str(), "\">\n",
           "    </testcase>\n",
           "    <x-teststart name=\"C\" classname=\"A\" timestamp=\"",
           TimeFormatAsIso8601(time_before_change + Milliseconds(500)).c_str(),
           "\" />\n"})));

  MockResultWatcher result_watcher(result_file, 2);
  EXPECT_CALL(result_watcher, WaitWithTimeout(_))
      .WillOnce(
          DoAll(Invoke(&task_environment, &test::TaskEnvironment::AdvanceClock),
                Return(false)))
      .WillOnce(
          DoAll(Invoke(&task_environment, &test::TaskEnvironment::AdvanceClock),
                Return(true)));

  Time start = Time::Now();
  ASSERT_TRUE(result_watcher.PollUntilDone(Seconds(45)));
  ASSERT_EQ(Time::Now() - start, Seconds(90));
}

// Verify that the result watcher continues polling with the base timeout when
// the clock jumps forward.
TEST_F(ResultWatcherTest, PollWithClockJumpForward) {
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath result_file = CreateResultFile();
  ASSERT_TRUE(AppendToFile(
      result_file,
      StrCat({"    <x-teststart name=\"B\" classname=\"A\" timestamp=\"",
              TimeFormatAsIso8601(Time::Now()).c_str(), "\" />\n",
              "    <testcase name=\"B\" status=\"run\" time=\"0.500\" "
              "classname=\"A\" timestamp=\"",
              TimeFormatAsIso8601(Time::Now()).c_str(), "\">\n",
              "    </testcase>\n",
              "    <x-teststart name=\"C\" classname=\"A\" timestamp=\"",
              TimeFormatAsIso8601(Time::Now() + Milliseconds(500)).c_str(),
              "\" />\n"})));
  task_environment.AdvanceClock(Hours(1));

  MockResultWatcher result_watcher(result_file, 2);
  EXPECT_CALL(result_watcher, WaitWithTimeout(_))
      .WillOnce(
          DoAll(Invoke(&task_environment, &test::TaskEnvironment::AdvanceClock),
                Return(false)))
      .WillOnce(
          DoAll(Invoke(&task_environment, &test::TaskEnvironment::AdvanceClock),
                Return(true)));

  Time start = Time::Now();
  ASSERT_TRUE(result_watcher.PollUntilDone(Seconds(45)));
  ASSERT_EQ(Time::Now() - start, Seconds(90));
}

// Validate delegate sets batch size correctly.
TEST_F(UnitTestLauncherDelegateTester, BatchSize) {
  UnitTestLauncherDelegate launcher_delegate(&defaultPlatform, 15u, true,
                                             DoNothing());
  TestLauncherDelegate* delegate_ptr = &launcher_delegate;
  EXPECT_EQ(delegate_ptr->GetBatchSize(), 15u);
}

// The following 4 tests are disabled as they are meant to only run from
// |RunMockTests| to validate tests launcher output for known results. The tests
// are expected to run in order within a same batch.

// Basic test to pass
TEST(MockUnitTests, DISABLED_PassTest) {
  ASSERT_TRUE(true);
}
// Basic test to fail
TEST(MockUnitTests, DISABLED_FailTest) {
  ASSERT_TRUE(false);
}
// Basic test to crash
TEST(MockUnitTests, DISABLED_CrashTest) {
  ImmediateCrash();
}
// Basic test will not be reached, due to the preceding crash in the same batch.
TEST(MockUnitTests, DISABLED_NoRunTest) {
  ASSERT_TRUE(true);
}

// Using TestLauncher to launch 3 basic unitests
// and validate the resulting json file.
TEST_F(UnitTestLauncherDelegateTester, RunMockTests) {
  CommandLine command_line(CommandLine::ForCurrentProcess()->GetProgram());
  command_line.AppendSwitchASCII("gtest_filter", "MockUnitTests.DISABLED_*");

  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath path = dir.GetPath().AppendASCII("SaveSummaryResult.json");
  command_line.AppendSwitchPath("test-launcher-summary-output", path);
  command_line.AppendSwitch("gtest_also_run_disabled_tests");
  command_line.AppendSwitchASCII("test-launcher-retry-limit", "0");

  std::string output;
  GetAppOutputAndError(command_line, &output);

  // Validate the resulting JSON file is the expected output.
  std::optional<Value::Dict> root = test_launcher_utils::ReadSummary(path);
  ASSERT_TRUE(root);

  const Value::Dict* dict = root->FindDict("test_locations");
  ASSERT_TRUE(dict);
  EXPECT_EQ(4u, dict->size());

  EXPECT_TRUE(
      test_launcher_utils::ValidateTestLocations(*dict, "MockUnitTests"));

  const Value::List* list = root->FindList("per_iteration_data");
  ASSERT_TRUE(list);
  ASSERT_EQ(1u, list->size());

  const Value::Dict* iteration_dict = (*list)[0].GetIfDict();
  ASSERT_TRUE(iteration_dict);
  EXPECT_EQ(4u, iteration_dict->size());
  // We expect the result to be stripped of disabled prefix.
  EXPECT_TRUE(test_launcher_utils::ValidateTestResult(
      *iteration_dict, "MockUnitTests.PassTest", "SUCCESS", 0u));
  EXPECT_TRUE(test_launcher_utils::ValidateTestResult(
      *iteration_dict, "MockUnitTests.FailTest", "FAILURE", 1u));
  EXPECT_TRUE(test_launcher_utils::ValidateTestResult(
      *iteration_dict, "MockUnitTests.CrashTest", "CRASH", 0u));
  EXPECT_TRUE(test_launcher_utils::ValidateTestResult(
      *iteration_dict, "MockUnitTests.NoRunTest", "NOTRUN", 0u,
      /*have_running_info=*/false));
}

TEST(ProcessGTestOutputTest, RunMockTests) {
  ScopedTempDir dir;
  CommandLine command_line(CommandLine::ForCurrentProcess()->GetProgram());
  command_line.AppendSwitchASCII("gtest_filter", "MockUnitTests.DISABLED_*");

  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath path = dir.GetPath().AppendASCII("SaveSummaryResult.xml");
  command_line.AppendSwitchPath("test-launcher-output", path);
  command_line.AppendSwitch("gtest_also_run_disabled_tests");
  command_line.AppendSwitch("single-process-tests");

  std::string output;
  GetAppOutputAndError(command_line, &output);

  std::vector<TestResult> test_results;
  bool crashed = false;
  bool have_test_results = ProcessGTestOutput(path, &test_results, &crashed);

  EXPECT_TRUE(have_test_results);
  EXPECT_TRUE(crashed);
  ASSERT_EQ(test_results.size(), 3u);

  EXPECT_EQ(test_results[0].full_name, "MockUnitTests.DISABLED_PassTest");
  EXPECT_EQ(test_results[0].status, TestResult::TEST_SUCCESS);
  EXPECT_EQ(test_results[0].test_result_parts.size(), 0u);
  ASSERT_TRUE(test_results[0].timestamp.has_value());
  EXPECT_GT(*test_results[0].timestamp, Time());
  EXPECT_FALSE(test_results[0].thread_id);
  EXPECT_FALSE(test_results[0].process_num);

  EXPECT_EQ(test_results[1].full_name, "MockUnitTests.DISABLED_FailTest");
  EXPECT_EQ(test_results[1].status, TestResult::TEST_FAILURE);
  EXPECT_EQ(test_results[1].test_result_parts.size(), 1u);
  ASSERT_TRUE(test_results[1].timestamp.has_value());
  EXPECT_GT(*test_results[1].timestamp, Time());

  EXPECT_EQ(test_results[2].full_name, "MockUnitTests.DISABLED_CrashTest");
  EXPECT_EQ(test_results[2].status, TestResult::TEST_CRASH);
  EXPECT_EQ(test_results[2].test_result_parts.size(), 0u);
  ASSERT_TRUE(test_results[2].timestamp.has_value());
  EXPECT_GT(*test_results[2].timestamp, Time());
}

// TODO(crbug.com/40287376): Enable the test once GetAppOutputAndError
// can collect stdout and stderr on Fuchsia.
#if !BUILDFLAG(IS_FUCHSIA)
TEST(ProcessGTestOutputTest, FoundTestCaseNotEnforced) {
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath path = dir.GetPath().AppendASCII("test.filter");
  WriteFile(path, "Test.firstTest\nTest.secondTest");
  CommandLine command_line(CommandLine::ForCurrentProcess()->GetProgram());
  command_line.AppendSwitchPath("test-launcher-filter-file", path);
  command_line.AppendSwitch("enforce-exact-positive-filter");
  std::string output;
  // Test cases in the filter do not exist, hence test launcher should
  // fail and print their names.
  EXPECT_FALSE(GetAppOutputAndError(command_line, &output));
  // Banner should appear in the output.
  const char kBanner[] = "Found exact positive filter not enforced:";
  EXPECT_TRUE(Contains(output, kBanner));
  std::vector<std::string> lines = base::SplitString(
      output, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  std::unordered_set<std::string> tests_not_enforced;
  bool banner_has_printed = false;
  for (size_t i = 0; i < lines.size(); i++) {
    if (Contains(lines[i], kBanner)) {
      // The following two lines should have the test cases not enforced
      // and the third line for the check failure message.
      EXPECT_LT(i + 3, lines.size());
      // Banner should only appear once.
      EXPECT_FALSE(banner_has_printed);
      banner_has_printed = true;
      continue;
    }
    if (banner_has_printed && tests_not_enforced.size() < 2) {
      // Note, gtest prints the error with datetime and file line info
      // ahead to the test names, e.g. below:
      // [1030/220237.425678:ERROR:test_launcher.cc(2123)] Test.secondTest
      // [1030/220237.425682:ERROR:test_launcher.cc(2123)] Test.firstTest
      std::vector<std::string> line_vec = base::SplitString(
          lines[i], "]", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      ASSERT_EQ(line_vec.size(), 2u);
      tests_not_enforced.insert(line_vec[1]);
      continue;
    }
    if (banner_has_printed && tests_not_enforced.size() == 2) {
// For official builds, they discard logs from CHECK failures, hence
// the test case cannot catch the "Check failed" line.
#if !defined(OFFICIAL_BUILD) || DCHECK_IS_ON()
      EXPECT_TRUE(Contains(lines[i],
                           "Check failed: "
                           "!found_exact_positive_filter_not_enforced."));
#endif  // !defined(OFFICIAL_BUILD) || DCHECK_IS_ON()
      break;
    }
  }
  // The test case printed is not ordered, hence need UnorderedElementsAre
  // to compare.
  EXPECT_THAT(tests_not_enforced, testing::UnorderedElementsAre(
                                      "Test.firstTest", "Test.secondTest"));
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

// TODO(crbug.com/40135391): Enable leaked-child checks on other platforms.
#if BUILDFLAG(IS_FUCHSIA)

// Test that leaves a child process running. The test is DISABLED_, so it can
// be launched explicitly by RunMockLeakProcessTest

MULTIPROCESS_TEST_MAIN(LeakChildProcess) {
  while (true)
    PlatformThread::Sleep(base::Seconds(1));
}

TEST(LeakedChildProcessTest, DISABLED_LeakChildProcess) {
  Process child_process = SpawnMultiProcessTestChild(
      "LeakChildProcess", GetMultiProcessTestChildBaseCommandLine(),
      LaunchOptions());
  ASSERT_TRUE(child_process.IsValid());
  // Don't wait for the child process to exit.
}

// Validate that a test that leaks a process causes the batch to have an
// error exit_code.
TEST_F(UnitTestLauncherDelegateTester, LeakedChildProcess) {
  CommandLine command_line(CommandLine::ForCurrentProcess()->GetProgram());
  command_line.AppendSwitchASCII(
      "gtest_filter", "LeakedChildProcessTest.DISABLED_LeakChildProcess");

  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath path = dir.GetPath().AppendASCII("SaveSummaryResult.json");
  command_line.AppendSwitchPath("test-launcher-summary-output", path);
  command_line.AppendSwitch("gtest_also_run_disabled_tests");
  command_line.AppendSwitchASCII("test-launcher-retry-limit", "0");

  std::string output;
  int exit_code = 0;
  GetAppOutputWithExitCode(command_line, &output, &exit_code);

  // Validate that we actually ran a test.
  std::optional<Value::Dict> root = test_launcher_utils::ReadSummary(path);
  ASSERT_TRUE(root);

  Value::Dict* dict = root->FindDict("test_locations");
  ASSERT_TRUE(dict);
  EXPECT_EQ(1u, dict->size());

  EXPECT_TRUE(test_launcher_utils::ValidateTestLocations(
      *dict, "LeakedChildProcessTest"));

  // Validate that the leaked child caused the batch to error-out.
  EXPECT_EQ(exit_code, 1);
}
#endif  // BUILDFLAG(IS_FUCHSIA)

// Validate GetTestOutputSnippetTest assigns correct output snippet.
TEST(TestLauncherTools, GetTestOutputSnippetTest) {
  const std::string output =
      "[ RUN      ] TestCase.FirstTest\n"
      "[       OK ] TestCase.FirstTest (0 ms)\n"
      "Post first test output\n"
      "[ RUN      ] TestCase.SecondTest\n"
      "[  FAILED  ] TestCase.SecondTest (0 ms)\n"
      "[ RUN      ] TestCase.ThirdTest\n"
      "[  SKIPPED ] TestCase.ThirdTest (0 ms)\n"
      "Post second test output";
  TestResult result;

  // test snippet of a successful test
  result.full_name = "TestCase.FirstTest";
  result.status = TestResult::TEST_SUCCESS;
  EXPECT_EQ(GetTestOutputSnippet(result, output),
            "[ RUN      ] TestCase.FirstTest\n"
            "[       OK ] TestCase.FirstTest (0 ms)\n");

  // test snippet of a failure on exit tests should include output
  // after test concluded, but not subsequent tests output.
  result.status = TestResult::TEST_FAILURE_ON_EXIT;
  EXPECT_EQ(GetTestOutputSnippet(result, output),
            "[ RUN      ] TestCase.FirstTest\n"
            "[       OK ] TestCase.FirstTest (0 ms)\n"
            "Post first test output\n");

  // test snippet of a failed test
  result.full_name = "TestCase.SecondTest";
  result.status = TestResult::TEST_FAILURE;
  EXPECT_EQ(GetTestOutputSnippet(result, output),
            "[ RUN      ] TestCase.SecondTest\n"
            "[  FAILED  ] TestCase.SecondTest (0 ms)\n");

  // test snippet of a skipped test. Note that the status is SUCCESS because
  // the gtest XML format doesn't make a difference between SUCCESS and SKIPPED
  result.full_name = "TestCase.ThirdTest";
  result.status = TestResult::TEST_SUCCESS;
  EXPECT_EQ(GetTestOutputSnippet(result, output),
            "[ RUN      ] TestCase.ThirdTest\n"
            "[  SKIPPED ] TestCase.ThirdTest (0 ms)\n");
}

MATCHER(CheckTruncationPreservesMessage, "") {
  // Ensure the inserted message matches the expected pattern.
  constexpr char kExpected[] = R"(FATAL.*message\n)";
  EXPECT_THAT(arg, ::testing::ContainsRegex(kExpected));

  const std::string snippet =
      base::StrCat({"[ RUN      ] SampleTestSuite.SampleTestName\n"
                    "Padding log message added for testing purposes\n"
                    "Padding log message added for testing purposes\n"
                    "Padding log message added for testing purposes\n"
                    "Padding log message added for testing purposes\n"
                    "Padding log message added for testing purposes\n"
                    "Padding log message added for testing purposes\n",
                    arg,
                    "Padding log message added for testing purposes\n"
                    "Padding log message added for testing purposes\n"
                    "Padding log message added for testing purposes\n"
                    "Padding log message added for testing purposes\n"
                    "Padding log message added for testing purposes\n"
                    "Padding log message added for testing purposes\n"});

  // Strip the stack trace off the end of message.
  size_t line_end_pos = arg.find("\n");
  std::string first_line = arg.substr(0, line_end_pos + 1);

  const std::string result = TruncateSnippetFocused(snippet, 300);
  EXPECT_TRUE(result.find(first_line) > 0);
  EXPECT_EQ(result.length(), 300UL);
  return true;
}

void MatchesFatalMessagesTest() {
  // Different Chrome test suites have different settings for their logs.
  // E.g. unit tests may not show the process ID (as they are single process),
  // whereas browser tests usually do (as they are multi-process). This
  // affects how log messages are formatted and hence how the log criticality
  // i.e. "FATAL", appears in the log message. We test the two extremes --
  // all process IDs, timestamps present, and all not present. We also test
  // the presence/absence of an extra logging prefix.
  {
    // Process ID, Thread ID, Timestamp and Tickcount.
    logging::SetLogItems(true, true, true, true);
    logging::SetLogPrefix(nullptr);
    EXPECT_DEATH_IF_SUPPORTED(LOG(FATAL) << "message",
                              CheckTruncationPreservesMessage());
  }
  {
    logging::SetLogItems(false, false, false, false);
    logging::SetLogPrefix(nullptr);
    EXPECT_DEATH_IF_SUPPORTED(LOG(FATAL) << "message",
                              CheckTruncationPreservesMessage());
  }
  {
    // Process ID, Thread ID, Timestamp and Tickcount.
    logging::SetLogItems(true, true, true, true);
    logging::SetLogPrefix("mylogprefix");
    EXPECT_DEATH_IF_SUPPORTED(LOG(FATAL) << "message",
                              CheckTruncationPreservesMessage());
  }
  {
    logging::SetLogItems(false, false, false, false);
    logging::SetLogPrefix("mylogprefix");
    EXPECT_DEATH_IF_SUPPORTED(LOG(FATAL) << "message",
                              CheckTruncationPreservesMessage());
  }
}

// Validates TestSnippetFocused correctly identifies fatal messages to
// retain during truncation.
TEST(TestLauncherTools, TruncateSnippetFocusedMatchesFatalMessagesTest) {
  logging::ScopedLoggingSettings scoped_logging_settings;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  scoped_logging_settings.SetLogFormat(logging::LogFormat::LOG_FORMAT_SYSLOG);
#endif
  MatchesFatalMessagesTest();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Validates TestSnippetFocused correctly identifies fatal messages to
// retain during truncation, for ChromeOS Ash.
TEST(TestLauncherTools, TruncateSnippetFocusedMatchesFatalMessagesCrosAshTest) {
  logging::ScopedLoggingSettings scoped_logging_settings;
  scoped_logging_settings.SetLogFormat(logging::LogFormat::LOG_FORMAT_CHROME);
  MatchesFatalMessagesTest();
}
#endif

// Validate TestSnippetFocused truncates snippets correctly, regardless of
// whether fatal messages appear at the start, middle or end of the snippet.
TEST(TestLauncherTools, TruncateSnippetFocusedTest) {
  // Test where FATAL message appears in the start of the log.
  const std::string snippet =
      "[ RUN      ] "
      "EndToEndTests/"
      "EndToEndTest.WebTransportSessionUnidirectionalStreamSentEarly/"
      "draft29_QBIC\n"
      "[26219:26368:FATAL:tls_handshaker.cc(293)] 1-RTT secret(s) not set "
      "yet.\n"
      "#0 0x55619ad1fcdb in backtrace "
      "/b/s/w/ir/cache/builder/src/third_party/llvm/compiler-rt/lib/asan/../"
      "sanitizer_common/sanitizer_common_interceptors.inc:4205:13\n"
      "#1 0x5561a6bdf519 in base::debug::CollectStackTrace(void**, unsigned "
      "long) ./../../base/debug/stack_trace_posix.cc:845:39\n"
      "#2 0x5561a69a1293 in StackTrace "
      "./../../base/debug/stack_trace.cc:200:12\n"
      "...\n";
  const std::string result = TruncateSnippetFocused(snippet, 300);
  EXPECT_EQ(
      result,
      "[ RUN      ] EndToEndTests/EndToEndTest.WebTransportSessionUnidirection"
      "alStreamSentEarly/draft29_QBIC\n"
      "[26219:26368:FATAL:tls_handshaker.cc(293)] 1-RTT secret(s) not set "
      "yet.\n"
      "#0 0x55619ad1fcdb in backtrace /b/s/w/ir/cache/bui\n"
      "<truncated (358 bytes)>\n"
      "Trace ./../../base/debug/stack_trace.cc:200:12\n"
      "...\n");
  EXPECT_EQ(result.length(), 300UL);

  // Test where FATAL message appears in the middle of the log.
  const std::string snippet_two =
      "[ RUN      ] NetworkingPrivateApiTest.CreateSharedNetwork\n"
      "Padding log information added for testing purposes\n"
      "Padding log information added for testing purposes\n"
      "Padding log information added for testing purposes\n"
      "FATAL extensions_unittests[12666:12666]: [managed_network_configuration"
      "_handler_impl.cc(525)] Check failed: !guid_str && !guid_str->empty().\n"
      "#0 0x562f31dba779 base::debug::CollectStackTrace()\n"
      "#1 0x562f31cdf2a3 base::debug::StackTrace::StackTrace()\n"
      "#2 0x562f31cf4380 logging::LogMessage::~LogMessage()\n"
      "#3 0x562f31cf4d3e logging::LogMessage::~LogMessage()\n";
  const std::string result_two = TruncateSnippetFocused(snippet_two, 300);
  EXPECT_EQ(
      result_two,
      "[ RUN      ] NetworkingPriv\n"
      "<truncated (210 bytes)>\n"
      " added for testing purposes\n"
      "FATAL extensions_unittests[12666:12666]: [managed_network_configuration"
      "_handler_impl.cc(525)] Check failed: !guid_str && !guid_str->empty().\n"
      "#0 0x562f31dba779 base::deb\n"
      "<truncated (213 bytes)>\n"
      ":LogMessage::~LogMessage()\n");
  EXPECT_EQ(result_two.length(), 300UL);

  // Test where FATAL message appears at end of the log.
  const std::string snippet_three =
      "[ RUN      ] All/PDFExtensionAccessibilityTreeDumpTest.Highlights/"
      "linux\n"
      "[6741:6741:0716/171816.818448:ERROR:power_monitor_device_source_stub.cc"
      "(11)] Not implemented reached in virtual bool base::PowerMonitorDevice"
      "Source::IsOnBatteryPower()\n"
      "[6741:6741:0716/171816.818912:INFO:content_main_runner_impl.cc(1082)]"
      " Chrome is running in full browser mode.\n"
      "libva error: va_getDriverName() failed with unknown libva error,driver"
      "_name=(null)\n"
      "[6741:6741:0716/171817.688633:FATAL:agent_scheduling_group_host.cc(290)"
      "] Check failed: message->routing_id() != MSG_ROUTING_CONTROL "
      "(2147483647 vs. 2147483647)\n";
  const std::string result_three = TruncateSnippetFocused(snippet_three, 300);
  EXPECT_EQ(
      result_three,
      "[ RUN      ] All/PDFExtensionAccessibilityTreeDumpTest.Hi\n"
      "<truncated (432 bytes)>\n"
      "Name() failed with unknown libva error,driver_name=(null)\n"
      "[6741:6741:0716/171817.688633:FATAL:agent_scheduling_group_host.cc(290)"
      "] Check failed: message->routing_id() != MSG_ROUTING_CONTROL "
      "(2147483647 vs. 2147483647)\n");
  EXPECT_EQ(result_three.length(), 300UL);

  // Test where FATAL message does not appear.
  const std::string snippet_four =
      "[ RUN      ] All/PassingTest/linux\n"
      "Padding log line 1 added for testing purposes\n"
      "Padding log line 2 added for testing purposes\n"
      "Padding log line 3 added for testing purposes\n"
      "Padding log line 4 added for testing purposes\n"
      "Padding log line 5 added for testing purposes\n"
      "Padding log line 6 added for testing purposes\n";
  const std::string result_four = TruncateSnippetFocused(snippet_four, 300);
  EXPECT_EQ(result_four,
            "[ RUN      ] All/PassingTest/linux\n"
            "Padding log line 1 added for testing purposes\n"
            "Padding log line 2 added for testing purposes\n"
            "Padding lo\n<truncated (311 bytes)>\n"
            "Padding log line 4 added for testing purposes\n"
            "Padding log line 5 added for testing purposes\n"
            "Padding log line 6 added for testing purposes\n");
  EXPECT_EQ(result_four.length(), 300UL);
}

}  // namespace

}  // namespace base
