// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_GROUP_FEATURES_H_
#define CHROME_BROWSER_ANDROID_TAB_GROUP_FEATURES_H_

#include <memory>

#include "base/functional/callback.h"

class Profile;
class TabGroupAndroid;

// This class owns the core controllers for features that are scoped to a tab
// group. This is the Android implementation of the forward declared type in
// components/tabs/. Desktop has a similar implementation in
// chrome/browser/ui/tabs/tab_group_features.h. This class can be subclassed by
// tests to perform dependency injection.
class TabGroupFeatures {
 public:
  static std::unique_ptr<TabGroupFeatures> CreateTabGroupFeatures();
  virtual ~TabGroupFeatures();

  TabGroupFeatures(const TabGroupFeatures&) = delete;
  TabGroupFeatures& operator=(const TabGroupFeatures&) = delete;

  // Call this method to stub out TabGroupFeatures for tests.
  using TabGroupFeaturesFactory =
      base::RepeatingCallback<std::unique_ptr<TabGroupFeatures>()>;
  static void ReplaceTabGroupFeaturesForTesting(
      TabGroupFeaturesFactory factory);

  // Called exactly once to initialize features.
  virtual void Init(TabGroupAndroid& group, Profile* profile);

  // Public accessors for features, e.g.
  // FooFeature* foo_feature() { return foo_feature_.get(); }

 protected:
  TabGroupFeatures();

 private:
  // Features will each have a controller. e.g.
  // std::unique_ptr<FooFeature> foo_feature_;

  bool initialized_ = false;
};

#endif  // CHROME_BROWSER_ANDROID_TAB_GROUP_FEATURES_H_
