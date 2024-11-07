// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURE_TEST_SUPPORT_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURE_TEST_SUPPORT_H_

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace content {
class NavigationHandle;
}  // namespace content

namespace apps::test {

// The valid link capturing configurations that can be enabled. ChromeOS does
// not support default-on.
enum class LinkCapturingFeatureVersion {
  // TODO(https://crbug.com/377522792): Remove v1 values on non-ChromeOS
  kV1DefaultOff,
  kV2DefaultOff,
#if !BUILDFLAG(IS_CHROMEOS)
  // TODO(https://crbug.com/377522792): Remove this.
  kV1DefaultOn,
  kV2DefaultOn,
#endif
};

std::string ToString(LinkCapturingFeatureVersion version);

// The functions should only be called from tests, and is used to enable or
// disable link capturing UXes. Only use these if link capturing needs to be
// enabled on all platforms, i.e. ChromeOS, Windows, Mac and Linux. For platform
// specific implementations, prefer initializing the feature list in the test
// file itself.
// Note: `captures_by_default` being set to true is not supported by ChromeOS.
std::vector<base::test::FeatureRefAndParams> GetFeaturesToEnableLinkCapturingUX(
    std::optional<bool> override_captures_by_default = std::nullopt,
    bool use_v2 = false);

// Same as above, but simplifies to using an enum to only accept the valid
// configurations for a given platform.
std::vector<base::test::FeatureRefAndParams> GetFeaturesToEnableLinkCapturingUX(
    LinkCapturingFeatureVersion version);

std::vector<base::test::FeatureRef> GetFeaturesToDisableLinkCapturingUX();

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

}  // namespace apps::test

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURE_TEST_SUPPORT_H_
