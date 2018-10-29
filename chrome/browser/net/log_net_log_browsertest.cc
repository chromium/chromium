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

// Tests for the --log-net-log command line flag.
class LogNetLogTest : public InProcessBrowserTest {
 public:
  LogNetLogTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    net_log_path_ = tmp_dir_.GetPath().AppendASCII("netlog.json");

    command_line->AppendSwitchPath(network::switches::kLogNetLog,
                                   net_log_path_);
  }

  void TearDownInProcessBrowserTestFixture() override { VerifyNetLog(); }

 private:
  // Verify that the netlog file was written and appears to be well formed.
  void VerifyNetLog() {
    // Read the netlog from disk.
    std::string file_contents;
    ASSERT_TRUE(base::ReadFileToString(net_log_path_, &file_contents))
        << "Could not read: " << net_log_path_;

    // Parse it as JSON.
    auto parsed = base::JSONReader::Read(file_contents);
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
  }

  base::FilePath net_log_path_;
  base::ScopedTempDir tmp_dir_;

  DISALLOW_COPY_AND_ASSIGN(LogNetLogTest);
};

IN_PROC_BROWSER_TEST_F(LogNetLogTest, Basic) {
  // Do an action that will result in the output of netlog events. This isn't
  // strictly necessary since there is other networking that will happen
  // implicitly to generate events.
  ui_test_utils::NavigateToURL(browser(), GURL("http://127.0.0.1/foo"));
}

}  // namespace

}  // namespace chrome_browser_net
