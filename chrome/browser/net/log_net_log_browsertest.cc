// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_browser_net {
namespace {

// Test fixture for running tests with --log-net-log with no explicit file
// specified.
class LogNetLogTest : public InProcessBrowserTest {
 public:
  LogNetLogTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(network::switches::kLogNetLog);
  }

  void TearDownInProcessBrowserTestFixture() override { VerifyNetLog(); }

 private:
  // Verify that the netlog file was written to the user data dir.
  void VerifyNetLog() {
    base::FilePath user_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
    auto net_log_path = user_data_dir.AppendASCII("netlog.json");

    // Read the netlog from disk.
    std::string file_contents;
    ASSERT_TRUE(base::ReadFileToString(net_log_path, &file_contents))
        << "Could not read: " << net_log_path;

    // Parse it as JSON.
    auto parsed = base::JSONReader::Read(file_contents);
    EXPECT_TRUE(parsed);

    // Detailed checking is done by LogNetLogExplicitFileTest, so this test just
    // accepts any valid JSON.
  }
};

IN_PROC_BROWSER_TEST_F(LogNetLogTest, Exists) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/simple.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
}

// Test fixture for running tests with --log-net-log, and a parameterized value
// for --net-log-capture-mode.
//
// Asserts that a netlog file was created, appears valid, and stripped cookies
// in accordance to the --net-log-capture-mode flag.
class LogNetLogExplicitFileTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<const char*> {
 public:
  LogNetLogExplicitFileTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    net_log_path_ = tmp_dir_.GetPath().AppendASCII("netlog.json");

    command_line->AppendSwitchPath(network::switches::kLogNetLog,
                                   net_log_path_);

    if (GetParam()) {
      command_line->AppendSwitchASCII(network::switches::kNetLogCaptureMode,
                                      GetParam());
    }
  }

  void TearDownInProcessBrowserTestFixture() override { VerifyNetLog(); }

 private:
  // Verify that the netlog file was written, appears to be well formed, and
  // includes the requested level of data.
  void VerifyNetLog() {
    // Read the netlog from disk.
    std::string file_contents;
    ASSERT_TRUE(base::ReadFileToString(net_log_path_, &file_contents))
        << "Could not read: " << net_log_path_;

    // Parse it as JSON.
    auto parsed = base::JSONReader::Read(file_contents);
    ASSERT_TRUE(parsed);

    // Ensure the root value is a dictionary.
    ASSERT_TRUE(parsed->is_dict());
    const base::Value::Dict& main = parsed->GetDict();

    // Ensure it has a "constants" property.
    const base::Value::Dict* constants = main.FindDict("constants");
    ASSERT_TRUE(constants);
    ASSERT_FALSE(constants->empty());

    // Ensure it has an "events" property.
    const base::Value::List* events = main.FindList("events");
    ASSERT_TRUE(events);
    ASSERT_FALSE(events->empty());

    // Verify that cookies were stripped when the --net-log-capture-mode flag
    // was omitted, and not stripped when it was given a value of
    // IncludeSensitive
    bool include_cookies =
        GetParam() && std::string_view(GetParam()) == "IncludeSensitive";

    if (include_cookies) {
      EXPECT_TRUE(file_contents.find("Set-Cookie: name=Good;Max-Age=3600") !=
                  std::string::npos);
    } else {
      EXPECT_TRUE(file_contents.find("Set-Cookie: [22 bytes were stripped]") !=
                  std::string::npos);
    }
  }

  base::FilePath net_log_path_;
  base::ScopedTempDir tmp_dir_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LogNetLogExplicitFileTest,
                         ::testing::Values(nullptr, "IncludeSensitive"));

IN_PROC_BROWSER_TEST_P(LogNetLogExplicitFileTest, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/set_cookie_header.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
}

// Test fixture for NetLog with invalid duration values.
//
// Tests handling of invalid values for the --net-log-duration flag.
// This ensures that when invalid duration values are provided,browser continues
// to function properly by:
// 1. Successfully creating a NetLog file
// 2. Continuing to log network activity throughout the browser session
// 3. Generating a properly formatted JSON log file
//
// The test operates by setting various invalid duration values, performing
// network operations, and verifying the NetLog continues to function rather
// than failing or stopping prematurely.
class LogNetLogInvalidDurationTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<const char*> {
 public:
  LogNetLogInvalidDurationTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    net_log_path_ =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("netlog.json"));

    // Add the NetLog path
    command_line->AppendSwitchPath(network::switches::kLogNetLog,
                                   net_log_path_);
    command_line->AppendSwitchASCII(network::switches::kLogNetLogDuration,
                                    GetParam());
    command_line->AppendSwitchASCII(network::switches::kNetLogCaptureMode,
                                    "Default");
  }

  void TearDownInProcessBrowserTestFixture() override {
    // Verify the log file exists and is valid
    std::string file_contents;
    ASSERT_TRUE(base::ReadFileToString(net_log_path_, &file_contents))
        << "Could not read: " << net_log_path_;

    // Parse it as JSON
    std::optional<base::Value> log_value =
        base::JSONReader::Read(file_contents);
    ASSERT_TRUE(log_value.has_value());
    EXPECT_TRUE(log_value->is_dict());
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::FilePath net_log_path_;
};

// Test cases: empty string, non-integer value, zero value, negative value
INSTANTIATE_TEST_SUITE_P(InvalidDurations,
                         LogNetLogInvalidDurationTest,
                         ::testing::Values("", "abc", "0", "-5"));

IN_PROC_BROWSER_TEST_P(LogNetLogInvalidDurationTest, InvalidDurationHandling) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Generate some network activity
  GURL url(embedded_test_server()->GetURL("/simple.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Generate more traffic to verify NetLog is still active
  url = embedded_test_server()->GetURL("/title1.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // NetLog should continue until browser shutdown
  // Verification of log file happens in TearDownInProcessBrowserTestFixture
}

// Test fixture for NetLog with a valid duration value.
//
// Tests the --net-log-duration flag with a valid positive integer value.
// This ensures that when a valid duration is provided, the NetLog system:
// 1. Properly starts capturing network events
// 2. Automatically stops capturing after the specified duration (1 second)
// 3. Generates a valid JSON file containing the captured events
//
// The test operates by:
// - Setting up a 1-second NetLog duration
// - Performing network operations to generate capturable events
// - Waiting for the duration to complete
// - Polling to verify a valid JSON file appears
class LogNetLogValidDurationTest : public InProcessBrowserTest {
 public:
  LogNetLogValidDurationTest() = default;
  // Polling interval when waiting for the NetLog file to be written
  static constexpr base::TimeDelta kPollInterval = base::Milliseconds(10);
  // Maximum number of polling attempts (total wait time = kPollInterval *
  // kMaxPollAttempts)
  static constexpr int kMaxPollAttempts = 200;  // 2 seconds total

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Create a temp directory for storing our netlog file.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    net_log_path_ =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("netlog_valid.json"));

    // Specify a 1-second NetLog duration.
    command_line->AppendSwitchPath(network::switches::kLogNetLog,
                                   net_log_path_);
    command_line->AppendSwitchASCII(network::switches::kLogNetLogDuration, "1");
    command_line->AppendSwitchASCII(network::switches::kNetLogCaptureMode,
                                    "Default");
  }

  bool LogFileExistsAndIsValidJson() {
    // Allow blocking file I/O in this test method:
    base::ScopedAllowBlockingForTesting allow_blocking;

    if (!base::PathExists(net_log_path_)) {
      return false;
    }

    std::string file_contents;
    if (!base::ReadFileToString(net_log_path_, &file_contents)) {
      return false;
    }

    std::optional<base::Value> parsed_json =
        base::JSONReader::Read(file_contents);
    if (!parsed_json.has_value() || !parsed_json->is_dict()) {
      return false;
    }
    return true;
  }
  void RunLoopFor(base::TimeDelta duration) {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), duration);
    run_loop.Run();
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::FilePath net_log_path_;
};

// This test confirms that the NetLog stops after 1 second and a valid JSON file
// eventually appears on disk.
IN_PROC_BROWSER_TEST_F(LogNetLogValidDurationTest, SucceedsWithOneSecond) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Wait ~1 second (the NetLog's duration).
  RunLoopFor(base::Seconds(1));

  // Now poll until the file is written and valid JSON, or we time out.
  bool success = false;
  for (int i = 0; i < kMaxPollAttempts; ++i) {
    if (LogFileExistsAndIsValidJson()) {
      success = true;
      break;
    }
    RunLoopFor(kPollInterval);
  }

  EXPECT_TRUE(success);
}
}  // namespace

}  // namespace chrome_browser_net
