// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_builder.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

using net::test_server::BasicHttpResponse;
using net::test_server::HttpResponse;
using net::test_server::HttpRequest;

namespace extensions {

class ActivityLogApiTest : public ExtensionApiTest {
 public:
  ActivityLogApiTest() : saved_cmdline_(base::CommandLine::NO_PROGRAM) {
    // TODO(crbug.com/40937027): Convert test to use HTTPS and then re-enable.
    feature_list_.InitAndDisableFeature(features::kHttpsFirstModeIncognito);
  }

  ~ActivityLogApiTest() override {
    *base::CommandLine::ForCurrentProcess() = saved_cmdline_;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    saved_cmdline_ = *base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kEnableExtensionActivityLogging);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    std::unique_ptr<BasicHttpResponse> response(new BasicHttpResponse);
    response->set_code(net::HTTP_OK);
    response->set_content("<html><head><title>ActivityLogTest</title>"
                          "</head><body>Hello World</body></html>");
    return std::move(response);
  }

 private:
  base::CommandLine saved_cmdline_;
  base::test::ScopedFeatureList feature_list_;
};

// The test extension sends a message to its 'friend'. The test completes
// if it successfully sees the 'friend' receive the message.
// TODO(crbug.com/40334711): This test is very long and can time out.
IN_PROC_BROWSER_TEST_F(ActivityLogApiTest, DISABLED_TriggerEvent) {
  ActivityLog::GetInstance(profile())->SetWatchdogAppActiveForTesting(true);

  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &ActivityLogApiTest::HandleRequest, base::Unretained(this)));
  ASSERT_TRUE(StartEmbeddedTestServer());

  const Extension* friend_extension =
      LoadExtension(test_data_dir_.AppendASCII("activity_log_private/friend"),
                    {.allow_in_incognito = true});
  ASSERT_TRUE(friend_extension);
  ASSERT_TRUE(RunExtensionTest("activity_log_private/test"));
  ActivityLog::GetInstance(profile())->SetWatchdogAppActiveForTesting(false);
}

}  // namespace extensions
