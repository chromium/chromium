// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is misnamed. Conceptually, this class owns features which are
// scoped to the entire process. The features must span multiple profiles. If a
// feature is scoped to a single profile it should instead be added as a
// BrowserContextKeyedServiceFactory.
//
// Historically, members of this class were lazily instantiated. Furthermore,
// some members would not be created in tests, resulting in production code
// adding nullptr checks to make tests pass. This is an anti-pattern and should
// be avoided. This is not making a statement about lazy initialization (e.g.
// performing non-trivial setup). This is about having precise lifetime
// semantics.
//
// New members should be added to GlobalFeatures, and be unconditionally
// instantiated.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_H_
#define CHROME_BROWSER_BROWSER_PROCESS_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "media/media_buildflags.h"

class BackgroundModeManager;
class BrowserProcessPlatformPart;
class BuildState;
class DownloadRequestLimiter;
class DownloadStatusUpdater;
class GlobalFeatures;
class GpuModeManager;
class IconManager;
class MediaFileSystemRegistry;
class NotificationPlatformBridge;
class NotificationUIManager;
class PrefService;
class ProfileManager;
class SerialPolicyAllowedPorts;
class StartupData;
class StatusTray;
class SystemNetworkContextManager;
class WebRtcLogUploader;

#if !BUILDFLAG(IS_ANDROID)
class HidSystemTrayIcon;
class UsbSystemTrayIcon;
class IntranetRedirectDetector;
#endif

namespace embedder_support {
class OriginTrialsSettingsStorage;
}

namespace network {
class NetworkQualityTracker;
class SharedURLLoaderFactory;
}

namespace safe_browsing {
class SafeBrowsingService;
}

namespace signin {
class ActivePrimaryAccountsMetricsRecorder;
}

namespace subresource_filter {
class RulesetService;
}

namespace variations {
class VariationsService;
}

namespace component_updater {
class ComponentUpdateService;
}

namespace gcm {
class GCMDriver;
}

namespace metrics {
class MetricsService;
}

namespace metrics_services_manager {
class MetricsServicesManager;
}

namespace network_time {
class NetworkTimeTracker;
}

namespace os_crypt_async {
class KeyProvider;
class OSCryptAsync;
}

namespace policy {
class ChromeBrowserPolicyConnector;
class PolicyService;
}

namespace printing {
class BackgroundPrintingManager;
class PrintJobManager;
class PrintPreviewDialogController;
}

namespace resource_coordinator {
class ResourceCoordinatorParts;
class TabManager;
}

// NOT THREAD SAFE, call only from the main thread.
// These functions shouldn't return NULL unless otherwise noted.
class BrowserProcess {
 public:
  BrowserProcess();

  BrowserProcess(const BrowserProcess&) = delete;
  BrowserProcess& operator=(const BrowserProcess&) = delete;

  virtual ~BrowserProcess();

  // Invoked when the user is logging out/shutting down. When logging off we may
  // not have enough time to do a normal shutdown. This method is invoked prior
  // to normal shutdown and saves any state that must be saved before system
  // shutdown.
  virtual void EndSession() = 0;

  // Ensures |local_state()| was flushed to disk and then posts |reply| back on
  // the current sequence.
  virtual void FlushLocalStateAndReply(base::OnceClosure reply) = 0;

  // Gets the manager for the various metrics-related services, constructing it
  // if necessary.
  virtual metrics_services_manager::MetricsServicesManager*
  GetMetricsServicesManager() = 0;

  // Gets the OriginTrialsSettingsStorage, constructing it if necessary.
  virtual embedder_support::OriginTrialsSettingsStorage*
  GetOriginTrialsSettingsStorage() = 0;

  // Services: any of these getters may return null.
  virtual metrics::MetricsService* metrics_service() = 0;
  virtual ProfileManager* profile_manager() = 0;
  virtual PrefService* local_state() = 0;
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  shared_url_loader_factory() = 0;
  virtual signin::ActivePrimaryAccountsMetricsRecorder*
  active_primary_accounts_metrics_recorder() = 0;
  virtual variations::VariationsService* variations_service() = 0;

  virtual BrowserProcessPlatformPart* platform_part() = 0;

  // Returns the manager for desktop notifications.
  // TODO(miguelg) This is in the process of being deprecated in favour of
  // NotificationPlatformBridge + NotificationDisplayService
  virtual NotificationUIManager* notification_ui_manager() = 0;
  virtual NotificationPlatformBridge* notification_platform_bridge() = 0;

  // Replacement for IOThread. It owns and manages the
  // NetworkContext which will use the network service when the network service
  // is enabled. When the network service is not enabled, its NetworkContext is
  // backed by the IOThread's URLRequestContext.
  virtual SystemNetworkContextManager* system_network_context_manager() = 0;

  // Returns a NetworkQualityTracker that can be used to subscribe for
  // network quality change events.
  virtual network::NetworkQualityTracker* network_quality_tracker() = 0;

  // Starts and manages the policy system.
  virtual policy::ChromeBrowserPolicyConnector* browser_policy_connector() = 0;

  // This is the main interface for chromium components to retrieve policy
  // information from the policy system.
  virtual policy::PolicyService* policy_service() = 0;

  virtual IconManager* icon_manager() = 0;

  virtual GpuModeManager* gpu_mode_manager() = 0;

  virtual void CreateDevToolsProtocolHandler() = 0;

  virtual void CreateDevToolsAutoOpener() = 0;

  virtual bool IsShuttingDown() = 0;

  virtual printing::PrintJobManager* print_job_manager() = 0;
  virtual printing::PrintPreviewDialogController*
      print_preview_dialog_controller() = 0;
  virtual printing::BackgroundPrintingManager*
      background_printing_manager() = 0;

#if !BUILDFLAG(IS_ANDROID)
  virtual IntranetRedirectDetector* intranet_redirect_detector() = 0;
#endif

  // Sets or gets the locale used by the application. It is the IETF language
  // tag, defined in BCP 47. The region subtag is not included when it adds no
  // distinguishing information to the language tag (e.g. both "en-US" and "fr"
  // are correct here).
  //
  // Setting the locale updates a few core places where this information is
  // stored, but does not reload any resources or refresh any UI.
  virtual const std::string& GetApplicationLocale() = 0;
  virtual void SetApplicationLocale(const std::string& actual_locale) = 0;

  virtual DownloadStatusUpdater* download_status_updater() = 0;
  virtual DownloadRequestLimiter* download_request_limiter() = 0;

  // Returns the object that manages background applications.
  virtual BackgroundModeManager* background_mode_manager() = 0;
#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  virtual void set_background_mode_manager_for_test(
      std::unique_ptr<BackgroundModeManager> manager) = 0;
#endif

  // Returns the StatusTray, which provides an API for displaying status icons
  // in the system status tray. Returns NULL if status icons are not supported
  // on this platform (or this is a unit test).
  virtual StatusTray* status_tray() = 0;

  // Returns the SafeBrowsing service.
  virtual safe_browsing::SafeBrowsingService* safe_browsing_service() = 0;

  // Returns the service providing versioned storage for rules used by the Safe
  // Browsing subresource filter.
  virtual subresource_filter::RulesetService*
  subresource_filter_ruleset_service() = 0;

  // Returns the service providing versioned storage for rules used by the
  // Fingerprinting Protection subresource filter.
  virtual subresource_filter::RulesetService*
  fingerprinting_protection_ruleset_service() = 0;

  // Returns the StartupData which owns any pre-created objects in //chrome
  // before the full browser starts.
  virtual StartupData* startup_data() = 0;

// TODO(crbug.com/40118868): Revisit once build flag switch of lacros-chrome is
// complete.
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  // This will start a timer that, if Chrome is in persistent mode, will check
  // whether an update is available, and if that's the case, restart the
  // browser. Note that restart code will strip some of the command line keys
  // and all loose values from the cl this instance of Chrome was launched with,
  // and add the command line key that will force Chrome to start in the
  // background mode. For the full list of "blacklisted" keys, refer to
  // |kSwitchesToRemoveOnAutorestart| array in browser_process_impl.cc.
  virtual void StartAutoupdateTimer() = 0;
#endif

  virtual component_updater::ComponentUpdateService* component_updater() = 0;

  virtual MediaFileSystemRegistry* media_file_system_registry() = 0;

  virtual WebRtcLogUploader* webrtc_log_uploader() = 0;

  virtual network_time::NetworkTimeTracker* network_time_tracker() = 0;

#if !BUILDFLAG(IS_ANDROID)
  // Avoid using this. Prefer using GCMProfileServiceFactory.
  virtual gcm::GCMDriver* gcm_driver() = 0;
#endif

  // Returns the tab manager. On non-supported platforms, this returns null.
  // TODO(sebmarchand): Update callers to
  // resource_coordinator_parts()->tab_manager() and remove this.
  virtual resource_coordinator::TabManager* GetTabManager() = 0;

  virtual resource_coordinator::ResourceCoordinatorParts*
  resource_coordinator_parts() = 0;

#if !BUILDFLAG(IS_ANDROID)
  // Returns the object which keeps track of serial port permissions configured
  // through the policy engine.
  virtual SerialPolicyAllowedPorts* serial_policy_allowed_ports() = 0;

  // Returns the object which maintains Human Interface Device (HID) system tray
  // icon.
  virtual HidSystemTrayIcon* hid_system_tray_icon() = 0;

  // Returns the object which maintains Universal Serial Bus (USB) system tray
  // icon.
  virtual UsbSystemTrayIcon* usb_system_tray_icon() = 0;
#endif

  // Obtain the browser instance of OSCryptAsync, which should be used for data
  // encryption.
  virtual os_crypt_async::OSCryptAsync* os_crypt_async() = 0;

  // Add an additional OSCryptAsync provider for use in tests. Should only be
  // called once, during startup.
  virtual void set_additional_os_crypt_async_provider_for_test(
      size_t precedence,
      std::unique_ptr<os_crypt_async::KeyProvider> provider) = 0;

  virtual BuildState* GetBuildState() = 0;
  // Returns the feature controllers scoped to this browser process.
  virtual GlobalFeatures* GetFeatures() = 0;

  // Do not add new members to this class. Instead use GlobalFeatures. See file
  // level comment for details.
};

extern BrowserProcess* g_browser_process;

#endif  // CHROME_BROWSER_BROWSER_PROCESS_H_
