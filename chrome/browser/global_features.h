// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLOBAL_FEATURES_H_
#define CHROME_BROWSER_GLOBAL_FEATURES_H_

#include <memory>

#include "base/functional/callback.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "net/net_buildflags.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

class GlobalBrowserCollection;

namespace system_permission_settings {
class PlatformHandle;
}  // namespace system_permission_settings
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
namespace whats_new {
class WhatsNewRegistry;
}  // namespace whats_new

namespace default_browser {
class DefaultBrowserManager;
}  // namespace default_browser
#endif

#if BUILDFLAG(ENABLE_GLIC)
namespace glic {
class GlicBackgroundModeManager;
class GlicGlobalEnabling;
class GlicProfileManager;
class GlicSyntheticTrialManager;
}  // namespace glic
#endif

class ApplicationLocaleStorage;
class AudioProcessMlModelForwarder;
class BrowserProcess;

namespace installer_downloader {
class InstallerDownloaderController;
}

namespace optimization_guide {
class OptimizationGuideGlobalFeature;
}  // namespace optimization_guide

namespace safe_browsing {
class ApplicationAdvancedProtectionStatusDetector;
}  // namespace safe_browsing

#if !BUILDFLAG(IS_ANDROID)
class ProfileLaunchObserver;
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
class StartupLaunchManager;
#endif

#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)
namespace unexportable_keys {
class UnexportableKeyObsoleteProfileGarbageCollector;
}  // namespace unexportable_keys
#endif  // BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)

namespace local_network_access {
class IPAddressSpaceOverridesPrefsObserver;
}  // namespace local_network_access

namespace on_device_translation {
class OnDeviceTranslationInstaller;
}

// This class owns the core controllers for features that are globally
// scoped on desktop and Android. It can be subclassed by tests to perform
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

  // Each of these is called exactly once to initialize features.
  // `PreBrowserProcessInit()` happens very early in
  // `BrowserProcessImpl::Init()` - in particular, it must happen before a
  // `ProfileManager` is created. `PostBrowserProcessInit()` happens further
  // down near the very end of `BrowserProcessImpl::Init()` after a
  // `ProfileManager` is allowed to exist. As with anything in
  // `BrowserProcessImpl::Init()`, both of these functions are called before
  // threads are created.
  void PreBrowserProcessInit();
  void PostBrowserProcessInit();

  // Only initializes core features. Used in unittests to create partial
  // features for TestingBrowserProcess.
  //
  // TODO(crbug.com/463444220) Merge implementation back into
  // PreBrowserProcessInit() and PostBrowserProcessInit() once unit tests stop
  // creating TestingBrowserProcess.
  void PreBrowserProcessInitCore();
  void PostBrowserProcessInitCore();

  // Each of these is called exactly once when the browser starts to shutdown,
  // in the named browser shutdown lifecycle phases. Importantly,
  // `PostMainMessageLoopRun()` must be called before the `ProfileManager` is
  // destroyed, and `PostDestroyThreads()` must be called after the
  // `ProfileManager` and `ResourceCoordinatorParts` are destroyed.
  //
  // In unit tests, it is recommended that you call
  // TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting() for
  // convenience instead of calling these methods directly.
  void PostMainMessageLoopRun();
  void PostDestroyThreads();

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
#endif
#if BUILDFLAG(ENABLE_GLIC) && !BUILDFLAG(IS_ANDROID)
  glic::GlicBackgroundModeManager* glic_background_mode_manager() {
    return glic_background_mode_manager_.get();
  }
#endif

#if BUILDFLAG(ENABLE_GLIC)
  glic::GlicSyntheticTrialManager* glic_synthetic_trial_manager() {
    return synthetic_trial_manager_.get();
  }

  glic::GlicGlobalEnabling& glic_global_enabling() {
    return *glic_global_enabling_.get();
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

  optimization_guide::OptimizationGuideGlobalFeature*
  optimization_guide_global_feature() {
    return optimization_guide_global_feature_.get();
  }

  AudioProcessMlModelForwarder* audio_process_ml_model_forwarder() {
    return audio_process_ml_model_forwarder_.get();
  }

  safe_browsing::ApplicationAdvancedProtectionStatusDetector*
  application_advanced_protection_status_detector() {
    return application_advanced_protection_status_detector_.get();
  }

  GlobalBrowserCollection* global_browser_collection() {
    return global_browser_collection_.get();
  }

  static ui::UserDataFactoryWithOwner<BrowserProcess>&
  GetUserDataFactoryForTesting();

#if !BUILDFLAG(IS_ANDROID)
  // Prefer using ProfileLaunchObserver::GetInstance() over calling this method
  // directly.
  ProfileLaunchObserver* profile_launch_observer() {
    return profile_launch_observer_.get();
  }
#endif  // !BUILDFLAG(IS_ANDROID)

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
  static ui::UserDataFactoryWithOwner<BrowserProcess>& GetUserDataFactory();

  // Features will each have a controller. e.g.
  // std::unique_ptr<FooFeature> foo_feature_;

  std::unique_ptr<system_permission_settings::PlatformHandle>
      system_permissions_platform_handle_;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  std::unique_ptr<whats_new::WhatsNewRegistry> whats_new_registry_;

  std::unique_ptr<default_browser::DefaultBrowserManager>
      default_browser_manager_;
#endif

#if BUILDFLAG(ENABLE_GLIC)
  std::unique_ptr<glic::GlicGlobalEnabling> glic_global_enabling_;
#endif
#if BUILDFLAG(ENABLE_GLIC)
  std::unique_ptr<glic::GlicProfileManager> glic_profile_manager_;
#endif
#if BUILDFLAG(ENABLE_GLIC) && !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<glic::GlicBackgroundModeManager>
      glic_background_mode_manager_;
#endif
#if BUILDFLAG(ENABLE_GLIC)
  std::unique_ptr<glic::GlicSyntheticTrialManager> synthetic_trial_manager_;
#endif

  std::unique_ptr<ApplicationLocaleStorage> application_locale_storage_;

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::unique_ptr<installer_downloader::InstallerDownloaderController>
      installer_downloader_controller_;
#endif

  std::unique_ptr<optimization_guide::OptimizationGuideGlobalFeature>
      optimization_guide_global_feature_;

  // Must be outlived by `optimization_guide_global_feature_`.
  std::unique_ptr<AudioProcessMlModelForwarder>
      audio_process_ml_model_forwarder_;

  std::unique_ptr<safe_browsing::ApplicationAdvancedProtectionStatusDetector>
      application_advanced_protection_status_detector_;

  std::unique_ptr<local_network_access::IPAddressSpaceOverridesPrefsObserver>
      ip_address_space_overrides_prefs_observer_;

  std::unique_ptr<GlobalBrowserCollection> global_browser_collection_;

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<StartupLaunchManager> startup_launch_manager_;
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)
  std::unique_ptr<
      unexportable_keys::UnexportableKeyObsoleteProfileGarbageCollector>
      unexportable_key_obsolete_profile_garbage_collector_;
#endif  // BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<on_device_translation::OnDeviceTranslationInstaller>
      on_device_translation_installer_;

  std::unique_ptr<ProfileLaunchObserver> profile_launch_observer_;
#endif  // !BUILDFLAG(IS_ANDROID)
};

#endif  // CHROME_BROWSER_GLOBAL_FEATURES_H_
