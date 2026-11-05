// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_CHROMEOS_LINK_CAPTURING_USER_CHOICE_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_CHROMEOS_LINK_CAPTURING_USER_CHOICE_H_

#include <string>

class Profile;

namespace apps {

// Applies an explicit user choice for "Opening supported links" of `app_id`
// on ChromeOS. App Service keeps the effective state. This function also
// records the choice in `WebApp::user_link_capturing_preference` (see
// b/578641259):
// - If `open_in_app` is true, sets `app_id` as the preferred app in App
//   Service. Records "Open in app" only if App Service applied it. If
//   navigation capturing is on by default, an overlapping preferred ARC app
//   or System Web App can block it.
// - Records "Open in Chrome browser" on each web app that App Service
//   removed as a preferred app because it overlaps with `app_id`.
// - If `open_in_app` is false, removes `app_id` as a preferred app and
//   records "Open in Chrome browser".
// Records nothing if `app_id` has no supported links. If the preferred apps
// list is not initialized, App Service queues the change, and this function
// records no "Open in app" choice and no displaced apps. The records are
// written asynchronously by web app commands. Will CHECK-fail if `app_id` is
// empty.
void SetSupportedLinksPreferenceByUser(Profile* profile,
                                       const std::string& app_id,
                                       bool open_in_app);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_CHROMEOS_LINK_CAPTURING_USER_CHOICE_H_
