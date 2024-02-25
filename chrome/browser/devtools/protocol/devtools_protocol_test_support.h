// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_TEST_SUPPORT_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_TEST_SUPPORT_H_

#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/test_devtools_protocol_client.h"

class DevToolsProtocolTestBase : public MixinBasedInProcessBrowserTest,
                                 public content::TestDevToolsProtocolClient {
 public:
  DevToolsProtocolTestBase();
  ~DevToolsProtocolTestBase() override;

 protected:
  void Attach();

  // InProcessBrowserTest  interface
  void TearDownOnMainThread() override;

  virtual content::WebContents* web_contents();
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_TEST_SUPPORT_H_
