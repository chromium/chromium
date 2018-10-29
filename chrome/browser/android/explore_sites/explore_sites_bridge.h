// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_BRIDGE_H_

namespace explore_sites {

// Methods for interacting with the Java side via JNI.
class ExploreSitesBridge {
 public:
  // Causes the Android JobScheduler to execute the catalog update daily.
  // The catalog update task checks that the feature is enabled and if not,
  // unschedules itself.
  static void ScheduleDailyTask();

  // Gets the device screen scale factor from Android.
  static float GetScaleFactorFromDevice();
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_BRIDGE_H_
