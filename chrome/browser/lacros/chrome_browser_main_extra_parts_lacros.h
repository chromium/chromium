// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_LACROS_H_
#define CHROME_BROWSER_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_LACROS_H_

#include "chrome/browser/chrome_browser_main_extra_parts.h"

#include <memory>

#include "chrome/browser/chromeos/smart_reader/smart_reader_client_impl.h"
#include "chrome/browser/lacros/sync/sync_crosapi_manager_lacros.h"

class ArcIconCache;
class AutomationManagerLacros;
class BrowserServiceLacros;
class ChromeKioskLaunchControllerLacros;
class DeskTemplateClientLacros;
class DeviceLocalAccountExtensionInstallerLacros;
class DriveFsCache;
class DownloadControllerClientLacros;
class ForceInstalledTrackerLacros;
class FullscreenControllerClientLacros;
class LacrosExtensionAppsController;
class LacrosExtensionAppsPublisher;
class LacrosFileSystemProvider;
class KioskSessionServiceLacros;
class FieldTrialObserver;
class NetworkChangeManagerBridge;
class QuickAnswersController;
class StandaloneBrowserTestController;
class TabletModePageBehavior;
class UiMetricRecorderLacros;
class VpnExtensionTrackerLacros;
class WebAuthnRequestRegistrarLacros;
class WebKioskInstallerLacros;
class MultitaskMenuNudgeDelegateLacros;

namespace arc {
class ArcIconCacheDelegateProvider;
}  // namespace arc

namespace crosapi {
class ClipboardHistoryLacros;
class SearchControllerLacros;
class TaskManagerLacros;
class WebAppProviderBridgeLacros;
class WebPageInfoProviderLacros;
}  // namespace crosapi

namespace content {
class ScreenOrientationDelegate;
}  // namespace content

namespace drive {
class DriveFsNativeMessageHostBridge;
}  // namespace drive

namespace video_conference {
class VideoConferenceManagerClientImpl;
}  // namespace video_conference

namespace smart_reader {
class SmartReaderClientImpl;
}  // namespace smart_reader

// Browser initialization for Lacros.
class ChromeBrowserMainExtraPartsLacros : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsLacros();
  ChromeBrowserMainExtraPartsLacros(const ChromeBrowserMainExtraPartsLacros&) =
      delete;
  ChromeBrowserMainExtraPartsLacros& operator=(
      const ChromeBrowserMainExtraPartsLacros&) = delete;
  ~ChromeBrowserMainExtraPartsLacros() override;

 private:
  // ChromeBrowserMainExtraParts:
  void PreProfileInit() override;
  void PostBrowserStart() override;
  void PostProfileInit(Profile* profile, bool is_initial_profile) override;
  void PostMainMessageLoopRun() override;

  // Receiver and cache of arc icon info updates.
  std::unique_ptr<ArcIconCache> arc_icon_cache_;

  std::unique_ptr<AutomationManagerLacros> automation_manager_;

  // Handles browser action requests from ash-chrome.
  std::unique_ptr<BrowserServiceLacros> browser_service_;

  // Handles requests for desk template data from ash-chrome.
  std::unique_ptr<DeskTemplateClientLacros> desk_template_client_;

  // Handles queries regarding full screen control from ash-chrome.
  std::unique_ptr<FullscreenControllerClientLacros>
      fullscreen_controller_client_;

  // Handles search queries from ash-chrome.
  std::unique_ptr<crosapi::SearchControllerLacros> search_controller_;

  // Handles task manager crosapi from ash for sending lacros tasks to ash.
  std::unique_ptr<crosapi::TaskManagerLacros> task_manager_provider_;

  // Receiver and cache of drive mount point path updates.
  std::unique_ptr<DriveFsCache> drivefs_cache_;

  // Handles requests from DriveFS to connect to an extension in lacros.
  std::unique_ptr<drive::DriveFsNativeMessageHostBridge>
      drivefs_native_message_host_bridge_;

  // Sends lacros download information to ash.
  std::unique_ptr<DownloadControllerClientLacros> download_controller_client_;

  // Sends lacros installation status of force-installed extensions to ash.
  std::unique_ptr<ForceInstalledTrackerLacros> force_installed_tracker_;

  // Receives and handles network change status.
  std::unique_ptr<NetworkChangeManagerBridge> network_change_manager_bridge_;

  // Sends lacros load/unload events of Vpn extensions to ash.
  std::unique_ptr<VpnExtensionTrackerLacros> vpn_extension_tracker_;

  std::unique_ptr<ChromeKioskLaunchControllerLacros>
      chrome_kiosk_launch_controller_;
  std::unique_ptr<WebKioskInstallerLacros> web_kiosk_installer_;

  // Manages the resources used in the web Kiosk session, and sends window
  // status changes of lacros-chrome to ash when necessary.
  std::unique_ptr<KioskSessionServiceLacros> kiosk_session_service_;

  std::unique_ptr<DeviceLocalAccountExtensionInstallerLacros>
      device_local_account_extension_installer_;

  // Provides ArcIconCache impl.
  std::unique_ptr<arc::ArcIconCacheDelegateProvider>
      arc_icon_cache_delegate_provider_;

  // Handles tab property requests from ash.
  std::unique_ptr<crosapi::WebPageInfoProviderLacros> web_page_info_provider_;

  // Receives web app control commands from ash.
  std::unique_ptr<crosapi::WebAppProviderBridgeLacros> web_app_provider_bridge_;

  // Sends Chrome app (AKA extension app) events to ash.
  std::unique_ptr<LacrosExtensionAppsPublisher> chrome_apps_publisher_;

  // Receives Chrome app (AKA extension app) events from ash.
  std::unique_ptr<LacrosExtensionAppsController> chrome_apps_controller_;

  // Sends extension events to ash.
  std::unique_ptr<LacrosExtensionAppsPublisher> extensions_publisher_;

  // Receives extension events from ash.
  std::unique_ptr<LacrosExtensionAppsController> extensions_controller_;

  // A test controller that is registered with the ash-chrome's test controller
  // service over crosapi to let tests running in ash-chrome control this Lacros
  // instance. It is only instantiated in Linux builds AND only when Ash's test
  // controller is available (practically, just test binaries), so this will
  // remain null in production builds.
  std::unique_ptr<StandaloneBrowserTestController>
      standalone_browser_test_controller_;

  // Receiver of field trial updates.
  std::unique_ptr<FieldTrialObserver> field_trial_observer_;

  // Receives orientation lock data.
  std::unique_ptr<content::ScreenOrientationDelegate>
      screen_orientation_delegate_;

  // Handles WebAuthn request id generation.
  std::unique_ptr<WebAuthnRequestRegistrarLacros>
      webauthn_request_registrar_lacros_;

  // Handles Quick answers requests from the Lacros browser.
  std::unique_ptr<QuickAnswersController> quick_answers_controller_;

  // Updates Blink preferences on tablet mode state change.
  std::unique_ptr<TabletModePageBehavior> tablet_mode_page_behavior_;

  // Forwards file system provider events to extensions.
  std::unique_ptr<LacrosFileSystemProvider> file_system_provider_;

  // Records UI metrics such as dropped frame percentage.
  std::unique_ptr<UiMetricRecorderLacros> ui_metric_recorder_;

  // Tracks videoconference apps and notifies VideoConferenceManagerAsh of
  // changes to the permissions or capturing statuses of these apps.
  std::unique_ptr<video_conference::VideoConferenceManagerClientImpl>
      video_conference_manager_client_;

  // Tracks the content within the current active tab in chrome to provide to
  // the smart reader manager.
  std::unique_ptr<smart_reader::SmartReaderClientImpl> smart_reader_client_;

  // Controls sync-related Crosapi clients.
  SyncCrosapiManagerLacros sync_crosapi_manager_;

  // Handles getting and setting multitask menu nudge related prefs from ash.
  std::unique_ptr<MultitaskMenuNudgeDelegateLacros>
      multitask_menu_nudge_delegate_;

  // Caches the clipboard history item descriptors in Lacros. Used only when
  // the clipboard history refresh feature is enabled.
  std::unique_ptr<crosapi::ClipboardHistoryLacros> clipboard_history_lacros_;
};

#endif  // CHROME_BROWSER_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_LACROS_H_
