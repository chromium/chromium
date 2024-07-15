// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLOBAL_DESKTOP_FEATURES_H_
#define CHROME_BROWSER_GLOBAL_DESKTOP_FEATURES_H_

#include "base/functional/callback.h"
#include "build/build_config.h"

namespace whats_new {
class WhatsNewRegistry;
}  // namespace whats_new

// This class owns the core controllers for features that are globally
// scoped on desktop. It can be subclassed by tests to perform
// dependency injection.
class GlobalDesktopFeatures {
 public:
  static std::unique_ptr<GlobalDesktopFeatures> CreateGlobalDesktopFeatures();
  virtual ~GlobalDesktopFeatures();

  GlobalDesktopFeatures(const GlobalDesktopFeatures&) = delete;
  GlobalDesktopFeatures& operator=(const GlobalDesktopFeatures&) = delete;

  // Call this method to stub out GlobalDesktopFeatures for tests.
  using GlobalDesktopFeaturesFactory =
      base::RepeatingCallback<std::unique_ptr<GlobalDesktopFeatures>()>;
  static void ReplaceGlobalDesktopFeaturesForTesting(
      GlobalDesktopFeaturesFactory factory);

  // Called exactly once to initialize features.
  void Init();

  // Public accessors for features, e.g.
  // FooFeature* foo_feature() { return foo_feature_.get(); }

  whats_new::WhatsNewRegistry* whats_new_registry() {
    return whats_new_registry_.get();
  }

 protected:
  GlobalDesktopFeatures();

  // Override these methods to stub out individual feature controllers for
  // testing. e.g.
  // virtual std::unique_ptr<FooFeature> CreateFooFeature();

  virtual std::unique_ptr<whats_new::WhatsNewRegistry> CreateWhatsNewRegistry();

 private:
  // Features will each have a controller. e.g.
  // std::unique_ptr<FooFeature> foo_feature_;

  std::unique_ptr<whats_new::WhatsNewRegistry> whats_new_registry_;
};

#endif  // CHROME_BROWSER_GLOBAL_DESKTOP_FEATURES_H_
