// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_LEVEL_LOGS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_LEVEL_LOGS_MANAGER_H_

#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_saver.h"
#include "chrome/browser/profiles/profile.h"

namespace chromeos {

class KioskAppLevelLogsManager {
 public:
  // TODO(b:425622183) implement kiosk app level logs collection logic.
  explicit KioskAppLevelLogsManager(Profile* profile);
  KioskAppLevelLogsManager(const KioskAppLevelLogsManager&) = delete;
  KioskAppLevelLogsManager& operator=(const KioskAppLevelLogsManager&) = delete;
  ~KioskAppLevelLogsManager();

 private:
  void SaveLog(const KioskAppLevelLogsSaver::KioskLogMessage& log);

  KioskAppLevelLogsSaver logs_saver_;
  // TODO(b:425623203) Implement and initialise KioskServiceWorkerLogsCollector.
  // TODO(b:417698708) Implement and initialise KioskAppWindowLogsCollector.
  // TODO(b:425645020) Implement and initialise KioskExtensionsLogsCollector.
  // TODO(b:425645764) Implement and initialise KioskBrowserLogsCollector.
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_LEVEL_LOGS_MANAGER_H_
