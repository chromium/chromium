// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_MAC_INTENT_PICKER_HELPERS_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_MAC_INTENT_PICKER_HELPERS_H_

#include <optional>
#include <string>

#include "chrome/browser/apps/link_capturing/apps_intent_picker_delegate.h"
#include "url/gurl.h"

namespace apps {

// Returns a native Mac app, if any, registered to own the given `url`.
std::optional<IntentPickerAppInfo> FindMacAppForUrl(const GURL& url);

// Launches a native Mac app, specified by the `launch_name` (the path) returned
// by `FindMacAppForUrl` above, for the given `url`.
void LaunchMacApp(const GURL& url, const std::string& launch_name);

// Force `FindMacAppForUrl` to return fixed values for testing.
// - If `fake` is `true` and `app_path` is set to a path, then
//   `FindMacAppForUrl` will return an `IntentPickerAppInfo` for the app at that
//   path.
// - If `fake` is `true` and `app_path` is empty, then `FindMacAppForUrl` will
//   return a `nullopt`.
// - If `fake` is `false`, then `FindMacAppForUrl` will behave normally.
void OverrideMacAppForUrlForTesting(bool fake, const std::string& app_path);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_MAC_INTENT_PICKER_HELPERS_H_
