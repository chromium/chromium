// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_ACTIVE_USER_TEST_MIXIN_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_ACTIVE_USER_TEST_MIXIN_H_

#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "google_apis/gaia/fake_gaia.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace enterprise_connectors::test {

// Helper class that allows a simple setup for active content area user
// identity. This should be used to help validate that DLP requests and reported
// events are detecting the correct user.
class ActiveUserTestMixin : public InProcessBrowserTestMixin {
 public:
  ActiveUserTestMixin(InProcessBrowserTestMixinHost* host,
                      InProcessBrowserTest* test,
                      net::EmbeddedTestServer* test_server,
                      std::vector<const char*> emails);
  ~ActiveUserTestMixin() override;

  void SetFakeCookieValue();

 protected:
  // InProcessBrowserTestMixin:
  void SetUp() override;
  void SetUpOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;

 private:
  const raw_ptr<InProcessBrowserTest> test_;
  const raw_ptr<net::EmbeddedTestServer> test_server_;
  std::vector<const char*> emails_;

  FakeGaia fake_gaia_;
};

}  // namespace enterprise_connectors::test

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_ACTIVE_USER_TEST_MIXIN_H_
