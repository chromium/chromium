// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLOBAL_FEATURES_H_
#define CHROME_BROWSER_GLOBAL_FEATURES_H_

#include <memory.h>

#include "base/functional/callback.h"
#include "build/build_config.h"

namespace system_permission_settings {
class PlatformHandle;
}  // namespace system_permission_settings
namespace whats_new {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
class WhatsNewRegistry;
#endif
}  // namespace whats_new

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

  system_permission_settings::PlatformHandle*
  system_permissions_platform_handle() {
    return system_permissions_platform_handle_.get();
  }
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  whats_new::WhatsNewRegistry* whats_new_registry() {
    return whats_new_registry_.get();
  }
#endif

 protected:
  GlobalFeatures();

  // Override these methods to stub out individual feature controllers for
  // testing. e.g.
  // virtual std::unique_ptr<FooFeature> CreateFooFeature();

  virtual std::unique_ptr<system_permission_settings::PlatformHandle>
  CreateSystemPermissionsPlatformHandle();
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  virtual std::unique_ptr<whats_new::WhatsNewRegistry> CreateWhatsNewRegistry();
#endif

 private:
  // Features will each have a controller. e.g.
  // std::unique_ptr<FooFeature> foo_feature_;

  std::unique_ptr<system_permission_settings::PlatformHandle>
      system_permissions_platform_handle_;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  std::unique_ptr<whats_new::WhatsNewRegistry> whats_new_registry_;
#endif
};

#endif  // CHROME_BROWSER_GLOBAL_FEATURES_H_
