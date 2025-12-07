// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTROLLED_FRAME_SCOPED_TEST_DRIVER_PROXY_H_
#define CHROME_BROWSER_CONTROLLED_FRAME_SCOPED_TEST_DRIVER_PROXY_H_

#include "base/memory/raw_ptr.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_devtools_protocol_client.h"

namespace content {
class DOMMessageQueue;
class RenderFrameHost;
}  // namespace content

namespace controlled_frame {

// Listens to the given RenderFrameHost for messages corresponding to
// testdriver.js actions. These messages come from the Javascript returned by
// `testdriver_override_script_src()`, which should be injected in the page
// being tested.
//
// This is essentially a third partial implementation of testdriver.js, the
// first two being Chromedriver and the Blink internals object used in Web
// Tests. See //third_party/blink/web_tests/resources/testdriver-vendor.js.
//
// This is needed because neither of the aforementioned testdriver.js
// implementations are available to //chrome browser tests. This class should
// be removed when IWAs are natively supported in the WPT infrastructure, but
// this is the simplest way to use testdriver.js until that is done. If this
// class ends up implementing a significant portion of the testdriver.js API,
// consider refactoring the Blink internals implementation so it can be
// exposed to //chrome browser tests.
class ScopedTestDriverProxy {
 public:
  explicit ScopedTestDriverProxy(content::RenderFrameHost* render_frame_host);
  ~ScopedTestDriverProxy();

  // Returns the source of Javascript that should be included in the page
  // being tested. This overrides the internal definition of testdriver.js
  // actions to forward them to ScopedTestDriverProxy.
  static const char* testdriver_override_script_src();

 private:
  void HandleMessages();

  raw_ptr<content::RenderFrameHost> render_frame_host_;
  content::TestDevToolsProtocolClient devtools_client_;
  content::DOMMessageQueue message_queue_;
};

}  // namespace controlled_frame

#endif  // CHROME_BROWSER_CONTROLLED_FRAME_SCOPED_TEST_DRIVER_PROXY_H_
