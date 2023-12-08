// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURE_TEST_SUPPORT_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURE_TEST_SUPPORT_H_

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace apps::test {

// The functions should only be called from tests, and is used to enable or
// disable link capturing UXes. Only use these if link capturing needs to be
// enabled on all platforms, i.e. ChromeOS, Windows, Mac and Linux. For platform
// specific implementations, prefer initializing the feature list in the test
// file itself.
// Note: `captures_by_default` being set to true is not supported by ChromeOS.
std::vector<base::test::FeatureRefAndParams> GetFeaturesToEnableLinkCapturingUX(
    std::optional<bool> override_captures_by_default = std::nullopt);

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

}  // namespace apps::test

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURE_TEST_SUPPORT_H_
