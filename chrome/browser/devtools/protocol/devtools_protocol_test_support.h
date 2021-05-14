// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_TEST_SUPPORT_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_TEST_SUPPORT_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"

class DevToolsProtocolTestBase : public InProcessBrowserTest,
                                 public content::DevToolsAgentHostClient {
 public:
  DevToolsProtocolTestBase();
  ~DevToolsProtocolTestBase() override;

 protected:
  using NotificationMatcher = base::RepeatingCallback<bool(const base::Value&)>;

  // InProcessBrowserTest  interface
  void TearDownOnMainThread() override;

  // DevToolsAgentHostClient interface
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override;

  void SendCommand(const std::string& method) {
    SendCommand(method, base::Value(), false);
  }

  void SendCommandSync(const std::string& method) {
    SendCommandSync(method, base::Value());
  }

  void SendCommandSync(const std::string& method, base::Value params) {
    SendCommand(method, std::move(params), true);
  }

  void SendCommand(const std::string& method,
                   base::Value params,
                   bool synchronous);
  void WaitForResponse();
  void RunLoopUpdatingQuitClosure();

  void AttachToBrowser();
  void Attach();
  void Detach();

  content::WebContents* web_contents();

  base::Value WaitForNotification(const std::string& notification);
  base::Value WaitForMatchingNotification(const std::string& notification,
                                          const NotificationMatcher& matcher);

  // DevToolsAgentHostClient interface
  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override;

  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  int last_sent_id_ = 0;
  base::OnceClosure run_loop_quit_closure_;
  bool in_dispatch_ = false;
  int waiting_for_command_result_id_ = 0;
  base::Value result_;
  std::vector<std::string> notifications_;
  std::vector<base::Value> notification_params_;
  std::string waiting_for_notification_;
  NotificationMatcher waiting_for_notification_matcher_;
  base::Value waiting_for_notification_params_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_TEST_SUPPORT_H_
