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
#include "chrome/browser/media/audio_process_ml_model_forwarder.h"
#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"
#include "chrome/browser/permissions/system/platform_handle.h"
#include "chrome/browser/safe_browsing/application_advanced_protection_status_detector.h"
#include "chrome/common/chrome_features.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/safe_browsing/core/common/features.h"
#include "media/base/media_switches.h"

#if BUILDFLAG(ENABLE_GLIC)
// This causes a gn error on Android builds, because gn does not understand
// buildflags, so we include it only on platforms where it is used.
#include "chrome/browser/background/glic/glic_background_mode_manager.h"  // nogncheck
#include "chrome/browser/glic/glic_profile_manager.h"               // nogncheck
#include "chrome/browser/glic/host/glic_synthetic_trial_manager.h"  // nogncheck
#include "chrome/browser/glic/public/glic_enabling.h"               // nogncheck
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// This causes a gn error on Android builds, because gn does not understand
// buildflags, so we include it only on platforms where it is used.
#include "chrome/browser/default_browser/default_browser_manager.h"
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
#include "chrome/browser/startup/startup_launch_manager.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#endif  // !BUILDFLAG(IS_ANDROID)

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

void GlobalFeatures::Init() {
#if !BUILDFLAG(IS_ANDROID)
  startup_launch_manager_ =
      GetUserDataFactory().CreateInstance<StartupLaunchManager>(
          *g_browser_process, g_browser_process);
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_GLIC)
  if (glic::GlicEnabling::IsEnabledByFlags()) {
    glic_profile_manager_ = std::make_unique<glic::GlicProfileManager>();
    glic_background_mode_manager_ =
        std::make_unique<glic::GlicBackgroundModeManager>(
            g_browser_process->status_tray());
    synthetic_trial_manager_ =
        std::make_unique<glic::GlicSyntheticTrialManager>();
  }
#endif

  InitCoreFeatures();
}

void GlobalFeatures::InitCoreFeatures() {
  system_permissions_platform_handle_ = CreateSystemPermissionsPlatformHandle();
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // TODO(crbug.com/463742800): Migrate WhatsNewRegistry (and other non-core
  // features) to Init().
  whats_new_registry_ = CreateWhatsNewRegistry();

  default_browser_manager_ =
      std::make_unique<default_browser::DefaultBrowserManager>(
          default_browser::DefaultBrowserManager::CreateDefaultDelegate());
#endif

  application_locale_storage_ = std::make_unique<ApplicationLocaleStorage>();

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
    audio_process_ml_model_forwarder_ = AudioProcessMlModelForwarder::Create();
  }

  if (base::FeatureList::IsEnabled(
          safe_browsing::kRelaunchNotificationForAdvancedProtection)) {
    application_advanced_protection_status_detector_ = std::make_unique<
        safe_browsing::ApplicationAdvancedProtectionStatusDetector>(
        g_browser_process->profile_manager());
  }

#if !BUILDFLAG(IS_ANDROID)
  global_browser_collection_ = std::make_unique<GlobalBrowserCollection>();
#endif  // !BUILDFLAG(IS_ANDROID)
}

void GlobalFeatures::Shutdown() {
#if BUILDFLAG(ENABLE_GLIC)
  if (glic_background_mode_manager_) {
    glic_background_mode_manager_->Shutdown();
    glic_background_mode_manager_.reset();
  }
  if (glic_profile_manager_) {
    glic_profile_manager_->Shutdown();
    glic_profile_manager_.reset();
  }
  synthetic_trial_manager_.reset();
#endif
  audio_process_ml_model_forwarder_.reset();
  optimization_guide_global_feature_.reset();

  application_advanced_protection_status_detector_.reset();
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
