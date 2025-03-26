// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURE_TEST_SUPPORT_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURE_TEST_SUPPORT_H_

#include <optional>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/common/web_app_id.h"

class Profile;
namespace content {
class DOMMessageQueue;
class NavigationHandle;
class WebContents;
}  // namespace content

namespace testing {
class AssertionResult;
}

namespace apps::test {

// The valid link capturing configurations that can be enabled. ChromeOS does
// not support default-on.
enum class LinkCapturingFeatureVersion {
  // TODO(https://crbug.com/377522792): Remove v1 values on non-ChromeOS
  kV1DefaultOff,
  kV2DefaultOff,
  kV2DefaultOffCaptureExistingFrames,
#if !BUILDFLAG(IS_CHROMEOS)
  kV2DefaultOn,
#endif
};

// Returns if links that target existing frames (e.g. "_self", "_top",
// "namedFrame" where the frame exists, etc) should capture into an app.
bool ShouldLinksWithExistingFrameTargetsCapture(
    LinkCapturingFeatureVersion version);
bool IsV1(LinkCapturingFeatureVersion version);
bool IsV2(LinkCapturingFeatureVersion version);

std::string ToString(LinkCapturingFeatureVersion version);

std::string LinkCapturingVersionToString(
    const testing::TestParamInfo<LinkCapturingFeatureVersion>& version);

// Used from tests to enable link capturing on all platforms, taking into
// account per platform behavior.
std::vector<base::test::FeatureRefAndParams> GetFeaturesToEnableLinkCapturingUX(
    LinkCapturingFeatureVersion version);

// Enables link capturing as if the user did it from the app settings page.
// Returns the error description if there was an error.
base::expected<void, std::string> EnableLinkCapturingByUser(
    Profile* profile,
    const webapps::AppId& app_id);

// Disables link capturing as if the user did it from the app settings page.
// Returns the error description if there was an error.
base::expected<void, std::string> DisableLinkCapturingByUser(
    Profile* profile,
    const webapps::AppId& app_id);

// Observer which waits for navigation events and blocks until a specific URL
// navigation is complete.
class NavigationCommittedForUrlObserver
    : public ui_test_utils::AllTabsObserver {
 public:
  // `url` is the URL to look for.
  explicit NavigationCommittedForUrlObserver(const GURL& url);
  ~NavigationCommittedForUrlObserver() override;

  // Returns the WebContents which navigated to `url`.
  content::WebContents* web_contents() const { return web_contents_; }

 protected:
  // AllTabsObserver
  std::unique_ptr<base::CheckedObserver> ProcessOneContents(
      content::WebContents* web_contents) override;

  void DidFinishNavigation(content::NavigationHandle* handle);

 private:
  friend class LoadStopObserver;

  GURL url_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
};

// Flush the `WebAppLaunchQueue` instance for every browser tabs.
void FlushLaunchQueuesForAllBrowserTabs();

// Intended to be used with test sites in
// chrome/test/data/banners/link_capturing.
//
// Calls `resolveLaunchParamsFlush()` on any web contents where that function is
// globally defined. This is used in `WaitForNavigationFinishedMessage` below
// (which is recommended for most tests), but defined publicly for Kombucha
// tests which have custom waiting for dom messages.
base::expected<void, std::vector<std::string>>
ResolveWebContentsWaitingForLaunchQueueFlush();

// Intended to be used with test sites in
// chrome/test/data/banners/link_capturing.
//
// Waits for "PleaseFlushLaunchQueue" and "FinishedNavigating" messages, exiting
// after receiving the latter. When a "PleaseFlushLaunchQueue" message is
// received, this will call `FlushLaunchQueuesForAllBrowserTabs()` above and
// then `ResolveWebContentsWaitingForLaunchQueueFlush()` to allow the
// participating tab to then proceed to emit the "FinishedNavigating" message,
// allowing this function to exit.
testing::AssertionResult WaitForNavigationFinishedMessage(
    content::DOMMessageQueue& message_queue);

// Looks for the existence of `params_variable_name` in `contents` assuming that
// urls are stored in it, and returns them.
// One of the use-cases this is used more consistently is by returning the urls
// enqueued in the launch queue of the site currently loaded in `contents`
// inside `params_variable_name`. This will CHECK-fail if `params_variable_name`
// exists but it doesn't store a list or the list doesn't contain any strings.
std::vector<GURL> GetLaunchParamUrlsInContents(
    content::WebContents* contents,
    const std::string& params_variable_name);

}  // namespace apps::test

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURE_TEST_SUPPORT_H_
