// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_EXTENSION_INSTALL_EVENT_LOG_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_EXTENSION_INSTALL_EVENT_LOG_COLLECTOR_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/logging.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class Profile;

namespace enterprise_management {
class ExtensionInstallReportLogEvent;
}
namespace policy {

// TODO(crbug/1073436): Refactor out common methods of this class and
// AppInstallEventLogCollector to new class.
// Listens for and logs events related to extension installation process.
class ExtensionInstallEventLogCollector
    : public chromeos::PowerManagerClient::Observer,
      public network::NetworkConnectionTracker::NetworkConnectionObserver,
      public extensions::ExtensionRegistryObserver,
      public extensions::InstallStageTracker::Observer {
 public:
  // The delegate that events are forwarded to for inclusion in the log.
  class Delegate {
   public:
    // Adds an identical log entry for every extension whose install is pending.
    // The |event|'s timestamp is set to the current time if not set yet.
    virtual void AddForAllExtensions(
        std::unique_ptr<enterprise_management::ExtensionInstallReportLogEvent>
            event) = 0;

    // Adds a log entry for extension. The |event|'s timestamp is set to the
    // current time if not set yet. If |gather_disk_space_info| is |true|,
    // information about total and free disk space is gathered in the background
    // and added to |event| before adding it to the log. Does not change the
    // list of pending extensions.
    virtual void Add(
        const extensions::ExtensionId& extension_id,
        bool gather_disk_space_info,
        std::unique_ptr<enterprise_management::ExtensionInstallReportLogEvent>
            event) = 0;

    // Updates the list of pending extensions in case of installation success
    // and failure.
    virtual void OnExtensionInstallationFinished(
        const extensions::ExtensionId& extension_id) = 0;

    // Checks whether the current extension is in pending list or not.
    virtual bool IsExtensionPending(
        const extensions::ExtensionId& extension_id) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Delegate must outlive |this|.
  ExtensionInstallEventLogCollector(extensions::ExtensionRegistry* registry,
                                    Delegate* delegate,
                                    Profile* profile);
  ~ExtensionInstallEventLogCollector() override;

  // Called in case of login when there are pending extensions.
  void AddLoginEvent();

  // Called in case of logout when there are pending extensions.
  void AddLogoutEvent();

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // ExtensionRegistryObserver overrides
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;

  // InstallStageTracker::Observer overrides
  void OnExtensionInstallationFailed(
      const extensions::ExtensionId& extension_id,
      extensions::InstallStageTracker::FailureReason reason) override;
  void OnExtensionInstallationStageChanged(
      const extensions::ExtensionId& id,
      extensions::InstallStageTracker::Stage stage) override;
  void OnExtensionDownloadingStageChanged(
      const extensions::ExtensionId& id,
      extensions::ExtensionDownloaderDelegate::Stage stage) override;
  void OnExtensionInstallCreationStageChanged(
      const extensions::ExtensionId& id,
      extensions::InstallStageTracker::InstallCreationStage stage) override;

  // Reports success events for the extensions which are requested from policy
  // and are already loaded.
  void OnExtensionsRequested(const extensions::ExtensionIdSet& extension_ids);

  // Adds success events and notifies delegate that extension is loaded
  // successfully.
  void AddSuccessEvent(const extensions::Extension* extension);

 private:
  extensions::ExtensionRegistry* registry_;
  Delegate* const delegate_;
  Profile* const profile_;

  // Whether the device is currently online.
  bool online_ = false;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      registry_observer_{this};
  ScopedObserver<extensions::InstallStageTracker,
                 extensions::InstallStageTracker::Observer>
      stage_tracker_observer_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_EXTENSION_INSTALL_EVENT_LOG_COLLECTOR_H_
