// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
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

}  // namespace

}  // namespace chrome_browser_net
