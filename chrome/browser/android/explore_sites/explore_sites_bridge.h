// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_BRIDGE_H_

namespace explore_sites {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// This enum should be kept in sync with ExploreSitesCatalogUpdateRequestSource
// in enums.xml.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.explore_sites
enum class ExploreSitesCatalogUpdateRequestSource {
  kNewTabPage = 0,
  kExploreSitesPage = 1,
  kBackground = 2,
  // Default to the old enum style because the
  // RecordEnumeratedHistogram method on the java side requires it.
  kNumEntries = 3,
};

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
