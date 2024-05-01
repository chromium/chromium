// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLOBAL_FEATURES_H_
#define CHROME_BROWSER_GLOBAL_FEATURES_H_

#include "base/functional/callback.h"

// This class owns the core controllers for features that are globally
// scoped on desktop. It can be subclassed by tests to perform
// dependency injection.
class GlobalFeatures {
 public:
  static std::unique_ptr<GlobalFeatures> CreateGlobalFeatures();
  virtual ~GlobalFeatures();

  GlobalFeatures(const GlobalFeatures&) = delete;
  GlobalFeatures& operator=(const GlobalFeatures&) = delete;

  // Call this method to stub out GlobalFeatures for tests.
  using GlobalFeaturesFactory =
      base::RepeatingCallback<std::unique_ptr<GlobalFeatures>()>;
  static void ReplaceGlobalFeaturesForTesting(GlobalFeaturesFactory factory);

  // Called exactly once to initialize features.
  void Init();

  // Public accessors for features, e.g.
  // FooFeature* foo_feature() { return foo_feature_.get(); }

 protected:
  GlobalFeatures();

  // Override these methods to stub out individual feature controllers for
  // testing. e.g.
  // virtual std::unique_ptr<FooFeature> CreateFooFeature();

 private:
  // Features will each have a controller. e.g.
  // std::unique_ptr<FooFeature> foo_feature_;
};

#endif  // CHROME_BROWSER_GLOBAL_FEATURES_H_
