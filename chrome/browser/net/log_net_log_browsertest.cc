// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chrome_browser_net {
namespace {

// Test fixture for running tests with --log-net-log, and a parameterized value
// for --net-log-capture-mode.
//
// Asserts that a netlog file was created, appears valid, and stripped cookies
// in accordance to the --net-log-capture-mode flag.
class LogNetLogTest : public InProcessBrowserTest,
                      public testing::WithParamInterface<const char*> {
 public:
  LogNetLogTest() = default;

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
    auto parsed = base::JSONReader::ReadDeprecated(file_contents);
    ASSERT_TRUE(parsed);

    // Ensure the root value is a dictionary.
    base::DictionaryValue* main;
    ASSERT_TRUE(parsed->GetAsDictionary(&main));

    // Ensure it has a "constants" property.
    base::DictionaryValue* constants;
    ASSERT_TRUE(main->GetDictionary("constants", &constants));
    ASSERT_FALSE(constants->empty());

    // Ensure it has an "events" property.
    base::ListValue* events;
    ASSERT_TRUE(main->GetList("events", &events));
    ASSERT_FALSE(events->empty());

    // Verify that cookies were stripped when the --net-log-capture-mode flag
    // was omitted, and not stripped when it was given a value of
    // IncludeSensitive
    bool include_cookies =
        GetParam() && base::StringPiece(GetParam()) == "IncludeSensitive";

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

  DISALLOW_COPY_AND_ASSIGN(LogNetLogTest);
};

INSTANTIATE_TEST_SUITE_P(,
                         LogNetLogTest,
                         ::testing::Values(nullptr, "IncludeSensitive"));

IN_PROC_BROWSER_TEST_P(LogNetLogTest, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/set_cookie_header.html"));
  ui_test_utils::NavigateToURL(browser(), url);
}

}  // namespace

}  // namespace chrome_browser_net
