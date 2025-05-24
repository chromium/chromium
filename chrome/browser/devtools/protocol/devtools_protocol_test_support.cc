// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"

#include "chrome/test/base/chrome_test_utils.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#endif  // !BUILDFLAG(IS_ANDROID)

DevToolsProtocolTestBase::DevToolsProtocolTestBase() = default;
DevToolsProtocolTestBase::~DevToolsProtocolTestBase() = default;

void DevToolsProtocolTestBase::Attach() {
  AttachToWebContents(web_contents());
}

void DevToolsProtocolTestBase::TearDownOnMainThread() {
  DetachProtocolClient();
}

content::WebContents* DevToolsProtocolTestBase::web_contents() {
#if BUILDFLAG(IS_ANDROID)
  return chrome_test_utils::GetActiveWebContents(this);
#else
  return browser()->tab_strip_model()->GetWebContentsAt(0);
#endif  // BUILDFLAG(IS_ANDROID)
}
