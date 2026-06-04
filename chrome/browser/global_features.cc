// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/global_features.h"

#include <memory>
#include <utility>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_profile_manager.h"               // nogncheck
#include "chrome/browser/glic/host/glic_synthetic_trial_manager.h"  // nogncheck
#include "chrome/browser/glic/public/glic_enabling.h"               // nogncheck
#include "chrome/browser/infobars/browser_infobar_manager.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/local_network_access/ip_address_space_overrides_prefs_observer.h"
#include "chrome/browser/media/audio_process_ml_model_forwarder.h"
#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"
#include "chrome/browser/permissions/system/platform_handle.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/application_advanced_protection_status_detector.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_manager.h"
#include "components/on_device_translation/buildflags/buildflags.h"
#include "components/safe_browsing/core/common/features.h"
#include "media/base/media_switches.h"
#include "net/net_buildflags.h"

#if !BUILDFLAG(IS_ANDROID)
// This causes a gn error on Android builds, because gn does not understand
// buildflags, so we include it only on platforms where it is used.
#include "chrome/browser/background/glic/glic_background_mode_manager.h"  // nogncheck
#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_session_desktop_injector.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// This causes a gn error on Android builds, because gn does not understand
// buildflags, so we include it only on platforms where it is used.
#include "chrome/browser/default_browser/default_browser_manager.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_registrar.h"
#include "components/user_education/common/user_education_features.h"  // nogncheck
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_controller.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_feature.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_infobar_delegate.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/lifetime/smart_restart_manager.h"
#include "chrome/browser/lifetime/smart_restart_metrics_observer.h"
#include "chrome/browser/ui/startup/profile_launch_observer.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/chrome_features.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/startup/startup_launch_manager.h"
#endif

#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_obsolete_profile_garbage_collector.h"  // nogncheck
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_provider_config.h"  // nogncheck
#include "components/unexportable_keys/features.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#endif  // BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)

#if BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
#include "chrome/browser/on_device_translation/installer_impl.h"
#endif  // BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)

namespace {

// This is the generic entry point for test code to stub out browser
// functionality. It is called by production code, but only used by tests.
GlobalFeatures::GlobalFeaturesFactory& GetFactory() {
  static base::NoDestructor<GlobalFeatures::GlobalFeaturesFactory> factory;
  return *factory;
}

}  // namespace

// static
std::unique_ptr<GlobalFeatures> GlobalFeatures::CreateGlobalFeatures() {
  if (GetFactory()) {
    CHECK_IS_TEST();
    return GetFactory().Run();
  }
  // Constructor is protected.
  return base::WrapUnique(new GlobalFeatures());
}

GlobalFeatures::~GlobalFeatures() = default;

// static
void GlobalFeatures::ReplaceGlobalFeaturesForTesting(
    GlobalFeaturesFactory factory) {
  GlobalFeatures::GlobalFeaturesFactory& f = GetFactory();
  f = std::move(factory);
}

void GlobalFeatures::PostBrowserProcessInit() {
#if BUILDFLAG(IS_WIN)
  startup_launch_manager_ =
      GetUserDataFactory().CreateInstance<StartupLaunchManager>(
          *g_browser_process, g_browser_process);
#endif  // !BUILDFLAG(IS_ANDROID)

  PostBrowserProcessInitCore();

  // Create Glic global features for all users in case they are only
  // temporarily ineligible (in which case we continue to show the entrypoint
  // but surface IPH dialogs instead of the webclient).
  // Many tests do not initialize profile_manager so we gate on that too.
  if (g_browser_process->profile_manager()) {
    glic_profile_manager_ = std::make_unique<glic::GlicProfileManager>();
#if !BUILDFLAG(IS_ANDROID)
    glic_background_mode_manager_ =
        std::make_unique<glic::GlicBackgroundModeManager>(
            g_browser_process->status_tray());
#endif
    synthetic_trial_manager_ =
        std::make_unique<glic::GlicSyntheticTrialManager>();
  }

#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)
  if (unexportable_keys::UnexportableKeyServiceImpl::
          IsStatefulUnexportableKeyProviderSupported(
              unexportable_keys::GetDefaultConfig()) &&
      base::FeatureList::IsEnabled(
          unexportable_keys::kUnexportableKeyDeletion)) {
    unexportable_key_obsolete_profile_garbage_collector_ = std::make_unique<
        unexportable_keys::UnexportableKeyObsoleteProfileGarbageCollector>(
        g_browser_process->profile_manager());
  }
#endif  // BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)

  ip_address_space_overrides_prefs_observer_ = std::make_unique<
      local_network_access::IPAddressSpaceOverridesPrefsObserver>(
      g_browser_process->local_state());
#if BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
  on_device_translation_installer_ = std::make_unique<
      on_device_translation::OnDeviceTranslationInstallerImpl>();
#endif  // BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)

#if !BUILDFLAG(IS_ANDROID)
  profile_launch_observer_ = std::make_unique<ProfileLaunchObserver>();

  if (base::FeatureList::IsEnabled(features::kSmartRestartMetrics)) {
    smart_restart_metrics_observer_ =
        std::make_unique<smart_restart::SmartRestartMetricsObserver>(
            UpgradeDetector::GetInstance());
  }

  if (base::FeatureList::IsEnabled(features::kSmartRestart)) {
    smart_restart_manager_ =
        std::make_unique<smart_restart::SmartRestartManager>(
            UpgradeDetector::GetInstance());
  }
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
  tab_drag_session_manager_ = std::make_unique<tabs_api::TabDragSessionManager>(
      std::make_unique<tabs_api::TabDragSessionDesktopInjector>());
#endif
}

void GlobalFeatures::PostBrowserProcessInitCore() {
#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(infobars::kCentralizedInfoBarFramework)) {
    browser_infobar_manager_ =
        GetUserDataFactory().CreateInstance<infobars::BrowserInfoBarManager>(
            *g_browser_process, g_browser_process);
  }
#endif
  system_permissions_platform_handle_ = CreateSystemPermissionsPlatformHandle();
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // TODO(crbug.com/463742800): Migrate WhatsNewRegistry (and other non-core
  // features) to Init().
  whats_new_registry_ = CreateWhatsNewRegistry();

  default_browser_manager_ =
      GetUserDataFactory()
          .CreateInstance<default_browser::DefaultBrowserManager>(
              *g_browser_process, g_browser_process,
              default_browser::DefaultBrowserManager::CreateDefaultDelegate(),
              base::BindRepeating(&ProfileManager::GetLastUsedProfile));
#endif

  application_locale_storage_ = std::make_unique<ApplicationLocaleStorage>();

  glic::GlicGlobalEnabling::Delegate glic_enabling_delegate;
  glic_global_enabling_ =
      std::make_unique<glic::GlicGlobalEnabling>(glic_enabling_delegate);

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (base::FeatureList::IsEnabled(
          installer_downloader::kInstallerDownloader)) {
    installer_downloader_controller_ = std::make_unique<
        installer_downloader::InstallerDownloaderController>(
        base::BindRepeating(
            &installer_downloader::InstallerDownloaderInfoBarDelegate::Show),
        base::BindRepeating(static_cast<bool (*)()>(
            &ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled)));
  }
#endif

  optimization_guide_global_feature_ =
      std::make_unique<optimization_guide::OptimizationGuideGlobalFeature>();

  if (media::IsAudioProcessMlModelUsageEnabled()) {
    audio_process_ml_model_forwarder_ =
        AudioProcessMlModelForwarder::Create(g_browser_process->local_state());
  }

  if (base::FeatureList::IsEnabled(
          safe_browsing::kRelaunchNotificationForAdvancedProtection)) {
    application_advanced_protection_status_detector_ = std::make_unique<
        safe_browsing::ApplicationAdvancedProtectionStatusDetector>(
        g_browser_process->profile_manager());
  }
}

void GlobalFeatures::Init() {
  global_browser_collection_ = CreateGlobalBrowserCollection();
}

void GlobalFeatures::PostMainMessageLoopRun() {
#if !BUILDFLAG(IS_ANDROID)
  smart_restart_manager_.reset();
  smart_restart_metrics_observer_.reset();
  profile_launch_observer_.reset();
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
  if (glic_background_mode_manager_) {
    glic_background_mode_manager_->Shutdown();
    glic_background_mode_manager_.reset();
  }
#endif
  if (glic_profile_manager_) {
    glic_profile_manager_->Shutdown();
    glic_profile_manager_.reset();
  }
  synthetic_trial_manager_.reset();
  audio_process_ml_model_forwarder_.reset();
  optimization_guide_global_feature_.reset();

  application_advanced_protection_status_detector_.reset();
  tab_drag_session_manager_.reset();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  DefaultBrowserPromptManager::GetInstance()->CloseAllPrompts(
      DefaultBrowserPromptManager::CloseReason::kDismiss);
#endif
}

void GlobalFeatures::PostDestroyThreads() {
#if BUILDFLAG(IS_WIN)
  // Startup launch manager should be destroyed before GlobalBrowserCollection
  // since its infobar manager observes GlobalBrowserCollection.
  startup_launch_manager_.reset();
#endif  // !BUILDFLAG(IS_ANDROID)
}

std::unique_ptr<system_permission_settings::PlatformHandle>
GlobalFeatures::CreateSystemPermissionsPlatformHandle() {
  return system_permission_settings::PlatformHandle::Create();
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
std::unique_ptr<whats_new::WhatsNewRegistry>
GlobalFeatures::CreateWhatsNewRegistry() {
  return whats_new::CreateWhatsNewRegistry();
}
#endif

std::unique_ptr<GlobalBrowserCollection>
GlobalFeatures::CreateGlobalBrowserCollection() {
  return std::make_unique<GlobalBrowserCollection>();
}

// static
ui::UserDataFactoryWithOwner<BrowserProcess>&
GlobalFeatures::GetUserDataFactoryForTesting() {
  return GetUserDataFactory();
}

// static
ui::UserDataFactoryWithOwner<BrowserProcess>&
GlobalFeatures::GetUserDataFactory() {
  static base::NoDestructor<ui::UserDataFactoryWithOwner<BrowserProcess>>
      factory;
  return *factory;
}

GlobalFeatures::GlobalFeatures() = default;
