// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/chrome_browser_main_extra_parts_lacros.h"

#include <memory>

#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/unguessable_token.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_manager_lacros.h"
#include "chrome/browser/chromeos/smart_reader/smart_reader_client_impl.h"
#include "chrome/browser/chromeos/tablet_mode/tablet_mode_page_behavior.h"
#include "chrome/browser/chromeos/video_conference/video_conference_manager_client.h"
#include "chrome/browser/lacros/app_mode/chrome_kiosk_launch_controller_lacros.h"
#include "chrome/browser/lacros/app_mode/device_local_account_extension_installer_lacros.h"
#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"
#include "chrome/browser/lacros/app_mode/web_kiosk_installer_lacros.h"
#include "chrome/browser/lacros/arc/arc_icon_cache.h"
#include "chrome/browser/lacros/automation_manager_lacros.h"
#include "chrome/browser/lacros/browser_service_lacros.h"
#include "chrome/browser/lacros/clipboard_history_lacros.h"
#include "chrome/browser/lacros/desk_template_client_lacros.h"
#include "chrome/browser/lacros/download_controller_client_lacros.h"
#include "chrome/browser/lacros/drivefs_cache.h"
#include "chrome/browser/lacros/drivefs_native_message_host_bridge_lacros.h"
#include "chrome/browser/lacros/embedded_a11y_manager_lacros.h"
#include "chrome/browser/lacros/field_trial_observer.h"
#include "chrome/browser/lacros/force_installed_tracker_lacros.h"
#include "chrome/browser/lacros/fullscreen_controller_client_lacros.h"
#include "chrome/browser/lacros/geolocation/system_geolocation_source_lacros.h"
#include "chrome/browser/lacros/lacros_apps_publisher.h"
#include "chrome/browser/lacros/lacros_extension_apps_controller.h"
#include "chrome/browser/lacros/lacros_extension_apps_publisher.h"
#include "chrome/browser/lacros/lacros_file_system_provider.h"
#include "chrome/browser/lacros/lacros_memory_pressure_evaluator.h"
#include "chrome/browser/lacros/launcher_search/search_controller_lacros.h"
#include "chrome/browser/lacros/multitask_menu_nudge_delegate_lacros.h"
#include "chrome/browser/lacros/net/network_change_manager_bridge.h"
#include "chrome/browser/lacros/screen_orientation_delegate_lacros.h"
#include "chrome/browser/lacros/standalone_browser_test_controller.h"
#include "chrome/browser/lacros/sync/sync_crosapi_manager_lacros.h"
#include "chrome/browser/lacros/task_manager_lacros.h"
#include "chrome/browser/lacros/ui_metric_recorder_lacros.h"
#include "chrome/browser/lacros/views_text_services_context_menu_lacros.h"
#include "chrome/browser/lacros/vpn_extension_tracker_lacros.h"
#include "chrome/browser/lacros/web_app_provider_bridge_lacros.h"
#include "chrome/browser/lacros/web_page_info_lacros.h"
#include "chrome/browser/lacros/webauthn_request_registrar_lacros.h"
#include "chrome/browser/memory/oom_kills_monitor.h"
#include "chrome/browser/metrics/structured/chrome_structured_metrics_recorder.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/quick_answers/read_write_cards_manager_impl.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "chromeos/ui/clipboard_history/clipboard_history_util.h"
#include "components/arc/common/intent_helper/arc_icon_cache_delegate.h"
#include "components/nacl/common/buildflags.h"
#include "extensions/common/features/feature_session_type.h"
#include "services/device/public/cpp/geolocation/geolocation_manager.h"
#include "ui/views/controls/views_text_services_context_menu_chromeos.h"

#if BUILDFLAG(ENABLE_NACL)
#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/lacros/lacros_paths.h"
#endif  // BUILDFLAG(ENABLE_NACL)

namespace {

// Creates a `crosapi::ClipboardHistoryLacros` instance. This function should
// only be called if the clipboard history refresh is enabled.
std::unique_ptr<crosapi::ClipboardHistoryLacros>
CreateClipboardHistoryLacros() {
  CHECK(chromeos::features::IsClipboardHistoryRefreshEnabled());

  if (chromeos::LacrosService* const service = chromeos::LacrosService::Get();
      service->IsAvailable<crosapi::mojom::ClipboardHistory>() &&
      service->GetInterfaceVersion<crosapi::mojom::ClipboardHistory>() >=
          int{crosapi::mojom::ClipboardHistory::MethodMinVersions::
                  kRegisterClientMinVersion}) {
    return std::make_unique<crosapi::ClipboardHistoryLacros>(
        service->GetRemote<crosapi::mojom::ClipboardHistory>().get());
  }

  return nullptr;
}

extensions::mojom::FeatureSessionType GetExtSessionType() {
  using extensions::mojom::FeatureSessionType;

  if (chromeos::IsKioskSession()) {
    return FeatureSessionType::kKiosk;
  }

  if (profiles::SessionHasGaiaAccount()) {
    return FeatureSessionType::kRegular;
  }

  // TODO: how to implement IsKioskAutolaunchedSession in Lacros
  // http://b/227564794
  return FeatureSessionType::kUnknown;
}

bool IsSessionTypeDeviceLocalAccountSession(
    crosapi::mojom::SessionType session_type) {
  using crosapi::mojom::SessionType;

  return session_type == SessionType::kAppKioskSession ||
         session_type == crosapi::mojom::SessionType::kWebKioskSession ||
         session_type == crosapi::mojom::SessionType::kPublicSession;
}

}  // namespace

ChromeBrowserMainExtraPartsLacros::ChromeBrowserMainExtraPartsLacros() =
    default;
ChromeBrowserMainExtraPartsLacros::~ChromeBrowserMainExtraPartsLacros() =
    default;

void ChromeBrowserMainExtraPartsLacros::PreProfileInit() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::SetCurrentFeatureSessionType(GetExtSessionType());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  tablet_mode_page_behavior_ = std::make_unique<TabletModePageBehavior>();

  if (IsSessionTypeDeviceLocalAccountSession(
          chromeos::BrowserParamsProxy::Get()->SessionType())) {
    device_local_account_extension_installer_ =
        std::make_unique<DeviceLocalAccountExtensionInstallerLacros>();
  }

  DCHECK(!device::GeolocationManager::GetInstance());
  device::GeolocationManager::SetInstance(
      SystemGeolocationSourceLacros::CreateGeolocationManagerOnLacros());

#if BUILDFLAG(ENABLE_NACL)
  // Ash ships PNaCl as part of rootfs, but Lacros doesn't ship it at all.
  // Since the required binaries are guaranteed to be the same, even on
  // different Chrome versions (since 2016), just use the PNaCl binaries shipped
  // with Ash.
  base::FilePath ash_resources_dir;
  if (!base::PathService::Get(chromeos::lacros_paths::ASH_RESOURCES_DIR,
                              &ash_resources_dir)) {
    LOG(WARNING) << "Could not find Ash PNaCl - PNaCl may be unavailable";
    base::debug::DumpWithoutCrashing();
  } else {
    base::FilePath ash_pnacl =
        ash_resources_dir.Append(FILE_PATH_LITERAL("pnacl"));
    base::PathService::Override(chrome::DIR_PNACL_COMPONENT, ash_pnacl);
  }
#endif  // BUILDFLAG(ENABLE_NACL)
}

void ChromeBrowserMainExtraPartsLacros::PostBrowserStart() {
  automation_manager_ = std::make_unique<AutomationManagerLacros>();
  browser_service_ = std::make_unique<BrowserServiceLacros>();
  desk_template_client_ = std::make_unique<DeskTemplateClientLacros>();
  drivefs_native_message_host_bridge_ =
      std::make_unique<drive::DriveFsNativeMessageHostBridge>();
  download_controller_client_ =
      std::make_unique<DownloadControllerClientLacros>();
  file_system_provider_ = std::make_unique<LacrosFileSystemProvider>();
  fullscreen_controller_client_ =
      std::make_unique<FullscreenControllerClientLacros>();
  kiosk_session_service_ = std::make_unique<KioskSessionServiceLacros>();
  network_change_manager_bridge_ =
      std::make_unique<NetworkChangeManagerBridge>();
  screen_orientation_delegate_ =
      std::make_unique<ScreenOrientationDelegateLacros>();
  search_controller_ = std::make_unique<crosapi::SearchControllerLacros>();
  task_manager_provider_ = std::make_unique<crosapi::TaskManagerLacros>();
  web_page_info_provider_ =
      std::make_unique<crosapi::WebPageInfoProviderLacros>();
  if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
    clipboard_history_lacros_ = CreateClipboardHistoryLacros();
  }

  memory_pressure::MultiSourceMemoryPressureMonitor* monitor =
      static_cast<memory_pressure::MultiSourceMemoryPressureMonitor*>(
          base::MemoryPressureMonitor::Get());
  if (monitor) {
    monitor->SetSystemEvaluator(std::make_unique<LacrosMemoryPressureEvaluator>(
        monitor->CreateVoter()));
  }

  lacros_apps_publisher_ = std::make_unique<LacrosAppsPublisher>();
  lacros_apps_publisher_->Initialize();

  if (chromeos::BrowserParamsProxy::Get()->PublishChromeApps()) {
    chrome_apps_publisher_ = LacrosExtensionAppsPublisher::MakeForChromeApps();
    chrome_apps_publisher_->Initialize();
    chrome_apps_controller_ =
        LacrosExtensionAppsController::MakeForChromeApps();
    chrome_apps_controller_->Initialize(chrome_apps_publisher_->publisher());
    chrome_apps_controller_->SetPublisher(chrome_apps_publisher_.get());

    extensions_publisher_ = LacrosExtensionAppsPublisher::MakeForExtensions();
    extensions_publisher_->Initialize();
    extensions_controller_ = LacrosExtensionAppsController::MakeForExtensions();
    extensions_controller_->Initialize(extensions_publisher_->publisher());
  }

  if (chromeos::BrowserParamsProxy::Get()->WebAppsEnabled()) {
    web_app_provider_bridge_ =
        std::make_unique<crosapi::WebAppProviderBridgeLacros>();
  }

  EmbeddedA11yManagerLacros::GetInstance()->Init();

#if !BUILDFLAG(IS_CHROMEOS_DEVICE)
  // The test controller is only created in test builds AND when Ash's test
  // controller service is available.
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::TestController>()) {
    int remote_version =
        lacros_service->GetInterfaceVersion<crosapi::mojom::TestController>();
    if (static_cast<uint32_t>(remote_version) >=
        crosapi::mojom::TestController::
            kRegisterStandaloneBrowserTestControllerMinVersion) {
      auto& ash_test_controller =
          lacros_service->GetRemote<crosapi::mojom::TestController>();
      standalone_browser_test_controller_ =
          std::make_unique<StandaloneBrowserTestController>(
              ash_test_controller);
    }
  }
#endif

  // Construct ArcIconCache and set it to provider.
  arc_icon_cache_ = std::make_unique<ArcIconCache>();
  arc_icon_cache_->Start();
  arc_icon_cache_delegate_provider_ =
      std::make_unique<arc::ArcIconCacheDelegateProvider>(
          arc_icon_cache_.get());

  // Start Lacros' drive mount point path caching, since it is available in Ash.
  drivefs_cache_ = std::make_unique<DriveFsCache>();
  // After construction finishes, start caching.
  drivefs_cache_->Start();

  field_trial_observer_ = std::make_unique<FieldTrialObserver>();
  field_trial_observer_->Start();

  force_installed_tracker_ = std::make_unique<ForceInstalledTrackerLacros>();
  force_installed_tracker_->Start();

  vpn_extension_tracker_ = std::make_unique<VpnExtensionTrackerLacros>();
  vpn_extension_tracker_->Start();

  webauthn_request_registrar_lacros_ =
      std::make_unique<WebAuthnRequestRegistrarLacros>();

  metrics::structured::ChromeStructuredMetricsRecorder::Get()->Initialize();

  if (g_browser_process != nullptr &&
      g_browser_process->local_state() != nullptr) {
    ::memory::OOMKillsMonitor::GetInstance().Initialize(
        g_browser_process->local_state());
  }

  ui_metric_recorder_ = std::make_unique<UiMetricRecorderLacros>();

  if (chromeos::BrowserParamsProxy::Get()->VcControlsUiEnabled() &&
      chromeos::LacrosService::Get()
          ->IsAvailable<crosapi::mojom::VideoConferenceManager>()) {
    video_conference_manager_client_ =
        std::make_unique<video_conference::VideoConferenceManagerClientImpl>();
  }

  smart_reader_client_ =
      std::make_unique<smart_reader::SmartReaderClientImpl>();

  if (chromeos::BrowserParamsProxy::Get()->IsWindowLayoutMenuEnabled()) {
    multitask_menu_nudge_delegate_ =
        std::make_unique<MultitaskMenuNudgeDelegateLacros>();
  }
}

void ChromeBrowserMainExtraPartsLacros::PostProfileInit(
    Profile* profile,
    bool is_initial_profile) {
  sync_crosapi_manager_.PostProfileInit(profile);

  // The setup below is intended to run for only the initial profile.
  if (!is_initial_profile) {
    return;
  }

  read_write_cards_manager_ =
      std::make_unique<chromeos::ReadWriteCardsManagerImpl>();

  // Initialize the metric reporting manager so we can start recording relevant
  // telemetry metrics and events on managed devices. The reporting manager
  // checks for profile affiliation, so we do not need any additional checks
  // here.
  ::reporting::metrics::MetricReportingManagerLacros::GetForProfile(profile);

  if (chromeos::BrowserParamsProxy::Get()->SessionType() ==
      crosapi::mojom::SessionType::kAppKioskSession) {
    chrome_kiosk_launch_controller_ =
        std::make_unique<ChromeKioskLaunchControllerLacros>(*profile);
  }
  if (chromeos::BrowserParamsProxy::Get()->SessionType() ==
      crosapi::mojom::SessionType::kWebKioskSession) {
    web_kiosk_installer_ = std::make_unique<WebKioskInstallerLacros>(*profile);
  }

  views::ViewsTextServicesContextMenuChromeos::SetImplFactory(
      base::BindRepeating(
          [](ui::SimpleMenuModel* menu_model, views::Textfield* textfield)
              -> std::unique_ptr<views::ViewsTextServicesContextMenu> {
            return std::make_unique<
                crosapi::ViewsTextServicesContextMenuLacros>(menu_model,
                                                             textfield);
          }));

  // Sets the implementation of clipboard history utility functions.
  if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
    chromeos::clipboard_history::SetQueryItemDescriptorsImpl(
        base::BindRepeating([]() {
          if (const crosapi::ClipboardHistoryLacros* const
                  clipboard_history_lacros =
                      crosapi::ClipboardHistoryLacros::Get()) {
            return clipboard_history_lacros->cached_descriptors();
          }

          return chromeos::clipboard_history::QueryItemDescriptorsImpl::
              ResultType();
        }));

    chromeos::clipboard_history::SetPasteClipboardItemByIdImpl(
        base::BindRepeating(
            [](const base::UnguessableToken& id, int event_flags,
               crosapi::mojom::ClipboardHistoryControllerShowSource source) {
              if (auto* lacros_service = chromeos::LacrosService::Get();
                  lacros_service &&
                  lacros_service
                      ->IsAvailable<crosapi::mojom::ClipboardHistory>() &&
                  lacros_service->GetInterfaceVersion<
                      crosapi::mojom::ClipboardHistory>() >=
                      int{crosapi::mojom::ClipboardHistory::MethodMinVersions::
                              kPasteClipboardItemByIdMinVersion}) {
                lacros_service->GetRemote<crosapi::mojom::ClipboardHistory>()
                    ->PasteClipboardItemById(id, event_flags, source);
              }
            }));
  }
}

void ChromeBrowserMainExtraPartsLacros::PostMainMessageLoopRun() {
  // Must be destroyed before `chrome_kiosk_launch_controller_->profile_` is
  // destroyed.
  chrome_kiosk_launch_controller_.reset();
  web_kiosk_installer_.reset();
  // Must be destroyed before
  // `kiosk_session_service_->kiosk_browser_session_->profile_` is destroyed.
  kiosk_session_service_.reset();
  // Must be destroyed before the extension system gets destroyed.
  force_installed_tracker_.reset();

  // Initialized in PreProfileInit.
  device::GeolocationManager::SetInstance(nullptr);
}
