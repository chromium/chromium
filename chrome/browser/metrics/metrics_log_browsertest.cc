// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_log.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/variations/hashing.h"
#include "content/public/test/browser_test.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

class MetricsLogBrowserTest : public PlatformBrowserTest {
 public:
  MetricsLogBrowserTest() = default;
  ~MetricsLogBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PlatformBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kNoStartupWindow);
    command_line->AppendSwitch(switches::kFlagSwitchesBegin);
    command_line->AppendSwitch(switches::kFlagSwitchesEnd);
  }
};

// Verify that system profile contains filtered command line keys.
IN_PROC_BROWSER_TEST_F(MetricsLogBrowserTest, CommandLineKeyHash) {
  TestMetricsServiceClient client;
  MetricsLog log("0a94430b-18e5-43c8-a657-580f7e855ce1", 0,
                 MetricsLog::INITIAL_STABILITY_LOG, &client);
  std::string encoded;
  // Don't set the close_time param since this is an initial stability log.
  log.FinalizeLog(/*truncate_events=*/false, client.GetVersionString(),
                  /*close_time=*/std::nullopt, &encoded);
  ChromeUserMetricsExtension uma_proto;
  uma_proto.ParseFromString(encoded);
  const auto hashes = uma_proto.system_profile().command_line_key_hash();

  bool found_startup_window_cmd = false;
  for (const auto hash : hashes) {
    // These two should be filtered out.
    EXPECT_NE(variations::HashName(switches::kFlagSwitchesBegin), hash);
    EXPECT_NE(variations::HashName(switches::kFlagSwitchesEnd), hash);
    // This one should appear.
    if (hash == variations::HashName(switches::kNoStartupWindow)) {
      found_startup_window_cmd = true;
      continue;
    }
  }
  EXPECT_TRUE(found_startup_window_cmd);
}

}  // namespace metrics
