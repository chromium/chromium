// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"

#include "chrome/browser/ui/browser.h"

DevToolsProtocolTestBase::DevToolsProtocolTestBase() = default;
DevToolsProtocolTestBase::~DevToolsProtocolTestBase() = default;

void DevToolsProtocolTestBase::Attach() {
  AttachToWebContents(web_contents());
}

void DevToolsProtocolTestBase::TearDownOnMainThread() {
  DetachProtocolClient();
}

content::WebContents* DevToolsProtocolTestBase::web_contents() {
  return browser()->tab_strip_model()->GetWebContentsAt(0);
}
