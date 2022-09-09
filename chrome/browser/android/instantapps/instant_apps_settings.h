// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_INSTANTAPPS_INSTANT_APPS_SETTINGS_H_
#define CHROME_BROWSER_ANDROID_INSTANTAPPS_INSTANT_APPS_SETTINGS_H_

#include <string>

namespace content {
class WebContents;
}

// Instant Apps banner events are stored with other app banner events, but
// with an instant app specific key. This class should be used to store and
// retrieve information about the Instant App banner events.
class InstantAppsSettings {
 public:
  InstantAppsSettings() = delete;
  InstantAppsSettings(const InstantAppsSettings&) = delete;
  InstantAppsSettings& operator=(const InstantAppsSettings&) = delete;

  static void RecordShowEvent(content::WebContents* web_contents,
                              const std::string& url);
  static void RecordDismissEvent(content::WebContents* web_contents,
                                 const std::string& url);
};

#endif  // CHROME_BROWSER_ANDROID_INSTANTAPPS_INSTANT_APPS_SETTINGS_H_
