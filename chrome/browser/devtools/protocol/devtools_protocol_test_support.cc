// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"

#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/test/base/chrome_test_utils.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#endif  // !BUILDFLAG(IS_ANDROID)

DevToolsProtocolTestBase::DevToolsProtocolTestBase() {
  webui_omnibox_feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/
      // TODO(crbug.com/452061489): Fix tests that fail when the WebUI Omnibox
      // is enabled and then remove these two Features.
      {omnibox::internal::kWebUIOmniboxPopup,
       omnibox::internal::kWebUIOmniboxAimPopup});
}

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
