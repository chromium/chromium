// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLOBAL_FEATURES_H_
#define CHROME_BROWSER_GLOBAL_FEATURES_H_

#include <memory.h>

#include "base/functional/callback.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"

namespace system_permission_settings {
class PlatformHandle;
}  // namespace system_permission_settings
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
namespace whats_new {
class WhatsNewRegistry;
}  // namespace whats_new
#endif
#if BUILDFLAG(ENABLE_GLIC)
namespace glic {
class GlicBackgroundModeManager;
class GlicProfileManager;
class GlicSyntheticTrialManager;
}  // namespace glic
#endif

class ApplicationLocaleStorage;

namespace installer_downloader {
class InstallerDownloaderController;
}

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

  // Called exactly once when the browser starts to shutdown.
  void Shutdown();

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

#if BUILDFLAG(ENABLE_GLIC)
  glic::GlicProfileManager* glic_profile_manager() {
    return glic_profile_manager_.get();
  }

  glic::GlicBackgroundModeManager* glic_background_mode_manager() {
    return glic_background_mode_manager_.get();
  }

  glic::GlicSyntheticTrialManager* glic_synthetic_trial_manager() {
    return synthetic_trial_manager_.get();
  }
#endif

  ApplicationLocaleStorage* application_locale_storage() {
    return application_locale_storage_.get();
  }

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  installer_downloader::InstallerDownloaderController*
  installer_downloader_controller() {
    return installer_downloader_controller_.get();
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

#if BUILDFLAG(ENABLE_GLIC)
  std::unique_ptr<glic::GlicProfileManager> glic_profile_manager_;
  std::unique_ptr<glic::GlicBackgroundModeManager>
      glic_background_mode_manager_;
  std::unique_ptr<glic::GlicSyntheticTrialManager> synthetic_trial_manager_;
#endif

  std::unique_ptr<ApplicationLocaleStorage> application_locale_storage_;

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::unique_ptr<installer_downloader::InstallerDownloaderController>
      installer_downloader_controller_;
#endif
};

#endif  // CHROME_BROWSER_GLOBAL_FEATURES_H_
