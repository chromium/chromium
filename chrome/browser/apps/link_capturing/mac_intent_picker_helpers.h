// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_MAC_INTENT_PICKER_HELPERS_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_MAC_INTENT_PICKER_HELPERS_H_

#include <optional>
#include <string>

#include "base/containers/span.h"
#include "chrome/browser/apps/link_capturing/apps_intent_picker_delegate.h"
#include "ui/gfx/image/image_family.h"
#include "url/gurl.h"

namespace apps {

struct MacAppInfo : IntentPickerAppInfo {
  gfx::ImageFamily icon;
};

// Returns a native Mac app, if any, registered to own the given `url`. Does
// not populate `icon_model` in the return value, but does populate `icon` with
// an ImageFamily containing all requested sizes.
std::optional<MacAppInfo> FindMacAppForUrl(const GURL& url,
                                           base::span<int> icon_sizes);

// Launches a native Mac app, specified by the `launch_name` (the path) returned
// by `FindMacAppForUrl` above, for the given `url`.
void LaunchMacApp(const GURL& url, const std::string& launch_name);

// Force `FindMacAppForUrl` to return fixed values for testing.
// - If `fake` is `true` and `app_path` is set to a path, then
//   `FindMacAppForUrl` will return an `MacAppInfo` for the app at that
//   path.
// - If `fake` is `true` and `app_path` is empty, then `FindMacAppForUrl` will
//   return a `nullopt`.
// - If `fake` is `false`, then `FindMacAppForUrl` will behave normally.
void OverrideMacAppForUrlForTesting(bool fake, const std::string& app_path);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_MAC_INTENT_PICKER_HELPERS_H_
