// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_TEST_SUPPORT_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_TEST_SUPPORT_H_

#include <string>
#include <utility>
#include <vector>

#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class DevToolsProtocolTestBase : public InProcessBrowserTest,
                                 public content::TestDevToolsProtocolClient {
 public:
  DevToolsProtocolTestBase();
  ~DevToolsProtocolTestBase() override;

 protected:
  void Attach();

  // InProcessBrowserTest  interface
  void TearDownOnMainThread() override;

  // DEPRECATED! Use TestDevToolsProtocolClient::SendCommand() & co.
  // These are compatibility wrappers for existent code.
  const base::Value::Dict* SendCommandSync(std::string method) {
    return SendCommand(std::move(method), base::Value::Dict(), true);
  }
  const base::Value::Dict* SendCommandSync(std::string method,
                                           base::Value params) {
    return SendCommand(std::move(method), std::move(params.GetDict()), true);
  }

  virtual content::WebContents* web_contents();
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_TEST_SUPPORT_H_
