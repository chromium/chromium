// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_EMBEDDED_POLICY_TEST_SERVER_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_EMBEDDED_POLICY_TEST_SERVER_MIXIN_H_

#include <memory>
#include <string>

#include "base/command_line.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/test_support/embedded_policy_test_server.h"

namespace ash {

// This test mixin covers setting up EmbeddedPolicyTestServer and adding a
// command-line flag to use it. Please see SetUp function for default settings.
// Server is started after SetUp execution.
class EmbeddedPolicyTestServerMixin : public InProcessBrowserTestMixin {
 public:
  explicit EmbeddedPolicyTestServerMixin(InProcessBrowserTestMixinHost* host);

  EmbeddedPolicyTestServerMixin(const EmbeddedPolicyTestServerMixin&) = delete;
  EmbeddedPolicyTestServerMixin& operator=(
      const EmbeddedPolicyTestServerMixin&) = delete;

  ~EmbeddedPolicyTestServerMixin() override;

  policy::EmbeddedPolicyTestServer* server() {
    return policy_test_server_.get();
  }

  // InProcessBrowserTestMixin:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // Updates user policy blob served by the embedded policy test server.
  // `policy_user` - the policy user's email.
  void UpdateUserPolicy(
      const enterprise_management::CloudPolicySettings& policy,
      const std::string& policy_user);

 private:
  std::unique_ptr<policy::EmbeddedPolicyTestServer> policy_test_server_;
  base::Value server_config_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_EMBEDDED_POLICY_TEST_SERVER_MIXIN_H_
