// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_LACROS_H_
#define CHROME_BROWSER_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_LACROS_H_

#include <memory>

#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/chromeos/smart_reader/smart_reader_client_impl.h"
#include "chrome/browser/lacros/magic_boost_state_lacros.h"
#include "chrome/browser/lacros/sync/sync_crosapi_manager_lacros.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"

class ArcIconCache;
class AutomationManagerLacros;
class BrowserServiceLacros;
class ChromeKioskLaunchControllerLacros;
class DeskTemplateClientLacros;
class DeviceLocalAccountExtensionInstallerLacros;
class CloudFileSystemPathCache;
class DownloadControllerClientLacros;
class ForceInstalledTrackerLacros;
class FullRestoreClientLacros;
class FullscreenControllerClientLacros;
class SuggestionServiceLacros;
class LacrosAppsPublisher;
class LacrosExtensionAppsController;
class LacrosExtensionAppsPublisher;
class LacrosFileSystemProvider;
class KioskSessionServiceLacros;
class FieldTrialObserver;
class NetworkSettingsObserver;
class TabletModePageBehavior;
class VpnExtensionTrackerLacros;
class WebAuthnRequestRegistrarLacros;
class WebKioskInstallerLacros;
class MultitaskMenuNudgeDelegateLacros;

namespace arc {
class ArcIconCacheDelegateProvider;
}  // namespace arc

namespace chromeos {
class ReadWriteCardsManager;
}  // namespace chromeos

namespace crosapi {
class ClipboardHistoryLacros;
class DebugInterfaceLacros;
class DeskProfilesLacros;
class MediaAppLacros;
class SearchControllerLacros;
class SearchControllerFactoryLacros;
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

namespace guest_os {
class VmSkForwardingService;
}

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

  // Handles request for session restore data used to display a nice UI in
  // ash-chrome.
  std::unique_ptr<FullRestoreClientLacros> full_restore_client_;

  // Handles queries regarding full screen control from ash-chrome.
  std::unique_ptr<FullscreenControllerClientLacros>
      fullscreen_controller_client_;

  // Handles search queries from ash-chrome.
  std::unique_ptr<crosapi::SearchControllerLacros> search_controller_;

  // Handles creating SearchControllers (above) from ash-chrome.
  std::unique_ptr<crosapi::SearchControllerFactoryLacros>
      search_controller_factory_;

  // Handles task manager crosapi from ash for sending lacros tasks to ash.
  std::unique_ptr<crosapi::TaskManagerLacros> task_manager_provider_;

  // Receiver and cache of cloud file systems mount points paths updates.
  std::unique_ptr<CloudFileSystemPathCache> cloud_file_system_cache_;

  // Handles requests from DriveFS to connect to an extension in lacros.
  std::unique_ptr<drive::DriveFsNativeMessageHostBridge>
      drivefs_native_message_host_bridge_;

  // Sends lacros download information to ash.
  std::unique_ptr<DownloadControllerClientLacros> download_controller_client_;

  // Sends lacros installation status of force-installed extensions to ash.
  std::unique_ptr<ForceInstalledTrackerLacros> force_installed_tracker_;

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

  // Sends Lacros events to ash.
  std::unique_ptr<LacrosAppsPublisher> lacros_apps_publisher_;

  // Sends Chrome app (AKA extension app) events to ash.
  std::unique_ptr<LacrosExtensionAppsPublisher> chrome_apps_publisher_;

  // Receives Chrome app (AKA extension app) events from ash.
  std::unique_ptr<LacrosExtensionAppsController> chrome_apps_controller_;

  // Sends extension events to ash.
  std::unique_ptr<LacrosExtensionAppsPublisher> extensions_publisher_;

  // Receives extension events from ash.
  std::unique_ptr<LacrosExtensionAppsController> extensions_controller_;

  // Receiver of field trial updates.
  std::unique_ptr<FieldTrialObserver> field_trial_observer_;

  // Receives orientation lock data.
  std::unique_ptr<content::ScreenOrientationDelegate>
      screen_orientation_delegate_;

  // Handles WebAuthn request id generation.
  std::unique_ptr<WebAuthnRequestRegistrarLacros>
      webauthn_request_registrar_lacros_;

  // Handles prefs related to magic boost.
  std::unique_ptr<chromeos::MagicBoostStateLacros> magic_boost_state_lacros_;

  // Handles read write cards requests from the Lacros browser.
  std::unique_ptr<chromeos::ReadWriteCardsManager> read_write_cards_manager_;

  // Updates Blink preferences on tablet mode state change.
  std::unique_ptr<TabletModePageBehavior> tablet_mode_page_behavior_;

  // Forwards file system provider events to extensions.
  std::unique_ptr<LacrosFileSystemProvider> file_system_provider_;

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

  // Forwards messages between VMs and the gnubbyd extension.
  std::unique_ptr<guest_os::VmSkForwardingService> vm_sk_forwarding_service_;

  // Observes profile information updates and sends summary info to ash. Used
  // only when the desk profiles feature is enabled.
  std::unique_ptr<crosapi::DeskProfilesLacros> desk_profiles_lacros_;

  // Observers network updates from the NetworkSettingsService.
  std::unique_ptr<NetworkSettingsObserver> network_settings_observer_;

  // Handles debug commands sent from ash-chrome.
  std::unique_ptr<crosapi::DebugInterfaceLacros> debug_interface_;

  // Handles sending requested suggestions to ash.
  std::unique_ptr<SuggestionServiceLacros> suggestion_service_;

  // Handles receiving requests from the Media App (which is in ash).
  std::unique_ptr<crosapi::MediaAppLacros> media_app_;
};

#endif  // CHROME_BROWSER_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_LACROS_H_
