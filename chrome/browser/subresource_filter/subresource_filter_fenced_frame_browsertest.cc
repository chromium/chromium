// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"

#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Mock;

namespace subresource_filter {

// Tests that AddMessageToConsole() is not called from NavigationConsoleLogger
// with a fenced frame to ensure that it works only with the outermost main
// frame.
IN_PROC_BROWSER_TEST_F(SubresourceFilterFencedFrameBrowserTest,
                       NavigatesToURLWithWarning_NoMessageLogged) {
  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("*show ads*");

  GURL fenced_frame_url(GetTestUrl("/fenced_frames/title1.html"));
  ConfigureURLWithWarning(fenced_frame_url,
                          {safe_browsing::SubresourceFilterType::BETTER_ADS});
  Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                       subresource_filter::ActivationScope::ACTIVATION_LIST,
                       subresource_filter::ActivationList::BETTER_ADS);
  ResetConfiguration(std::move(config));

  GURL url(GetTestUrl("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_EQ(0u, console_observer.messages().size());

  // Load a fenced frame.
  ConfigureURLWithWarning(fenced_frame_url,
                          {safe_browsing::SubresourceFilterType::BETTER_ADS});
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetMainFrame(), fenced_frame_url);
  ASSERT_EQ(0u, console_observer.messages().size());

  // Navigate the fenced frame again.
  fenced_frame_test_helper().NavigateFrameInFencedFrameTree(fenced_frame_host,
                                                            fenced_frame_url);
  ASSERT_EQ(0u, console_observer.messages().size());
}

}  // namespace subresource_filter
