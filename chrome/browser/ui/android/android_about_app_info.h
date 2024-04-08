// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_ANDROID_ABOUT_APP_INFO_H_
#define CHROME_BROWSER_UI_ANDROID_ANDROID_ABOUT_APP_INFO_H_

#include <string>

class AndroidAboutAppInfo {
 public:
  // Returns a string containing detailed info about the Google Play services
  // status.
  static std::string GetGmsInfo();

  // Returns a string containing detailed info about the os environment.
  static std::string GetOsInfo();

  // Returns a string containing info about whether the device is at least
  // Android U and whether Chrome targets at least U.
  static std::string GetTargetsUInfo();
};

#endif  // CHROME_BROWSER_UI_ANDROID_ANDROID_ABOUT_APP_INFO_H_
