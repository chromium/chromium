// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_FORCE_INSTALLED_METRICS_H_
#define CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_FORCE_INSTALLED_METRICS_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/forced_extensions/force_installed_tracker.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/common/extension.h"

class Profile;

namespace extensions {

// Used to report force-installed extension stats to UMA.
// ExtensionService owns this class and outlives it.
class ForceInstalledMetrics : public ForceInstalledTracker::Observer {
 public:
  ForceInstalledMetrics(ExtensionRegistry* registry,
                        Profile* profile,
                        ForceInstalledTracker* tracker,
                        std::unique_ptr<base::OneShotTimer> timer =
                            std::make_unique<base::OneShotTimer>());

  ForceInstalledMetrics(const ForceInstalledMetrics&) = delete;
  ForceInstalledMetrics& operator=(const ForceInstalledMetrics&) = delete;

  ~ForceInstalledMetrics() override;

  // Note: enum used for UMA. Do NOT reorder or remove entries. Don't forget to
  // update enums.xml (name: SessionType) when adding new
  // entries.
  // Type of session for current user. This enum is required as UserType enum
  // doesn't support new regular users. See user_manager::UserType enum for
  // description of session types other than new and existing regular users.
  enum class UserType {
    // Session with Regular existing user, which has a user name and password.
    USER_TYPE_REGULAR_EXISTING = 0,
    USER_TYPE_GUEST = 1,
    // Session with Regular new user, which has a user name and password.
    USER_TYPE_REGULAR_NEW = 2,
    USER_TYPE_PUBLIC_ACCOUNT = 3,
    // USER_TYPE_SUPERVISED_DEPRECATED = 4,
    USER_TYPE_KIOSK_APP = 5,
    USER_TYPE_CHILD = 6,
    // USER_TYPE_ARC_KIOSK_APP_DEPRECATED = 7,
    USER_TYPE_ACTIVE_DIRECTORY = 8,
    USER_TYPE_WEB_KIOSK_APP = 9,
    // Maximum histogram value.
    kMaxValue = USER_TYPE_WEB_KIOSK_APP
  };

  // ForceInstalledTracker::Observer overrides:
  //
  // Calls ReportMetrics method if there is a non-empty list of
  // force-installed extensions, and is responsible for cleanup of
  // observers.
  void OnForceInstalledExtensionsLoaded() override;

  // Calls ReportMetricsOnExtensionsReady method if there is a non-empty list of
  // force-installed extensions.
  void OnForceInstalledExtensionsReady() override;

  // Reports cache status for the force installed extensions.
  void OnExtensionDownloadCacheStatusRetrieved(
      const ExtensionId& id,
      ExtensionDownloaderDelegate::CacheStatus cache_status) override;

 private:

  // Reports disable reasons for the extensions which are installed but not
  // loaded.
  void ReportDisableReason(const ExtensionId& extension_id);

  // If |kInstallationTimeout| report time elapsed for extensions load,
  // otherwise amount of not yet loaded extensions and reasons
  // why they were not installed.
  void ReportMetrics();

  // Reports metrics for sessions when all force installed extensions are ready
  // for use.
  void ReportMetricsOnExtensionsReady();

  const raw_ptr<ExtensionRegistry> registry_;
  const raw_ptr<Profile> profile_;
  const raw_ptr<ForceInstalledTracker> tracker_;

  // Tracks the moment when the class was initialized and used as timer start
  // for reporting metrics related to extensions not loaded after 5 minutes.
  base::Time start_time_;

  // Tracks whether extensions load stats were already for the session.
  bool load_reported_ = false;

  // Tracks whether extensions ready stats were already reported for the
  // session.
  bool ready_reported_ = false;

  base::ScopedObservation<ForceInstalledTracker,
                          ForceInstalledTracker::Observer>
      tracker_observation_{this};

  // Tracks installation reporting timeout.
  std::unique_ptr<base::OneShotTimer> timer_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_FORCE_INSTALLED_METRICS_H_
