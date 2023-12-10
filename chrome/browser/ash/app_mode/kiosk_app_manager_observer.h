// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_MANAGER_OBSERVER_H_

#include <string>

#include "base/observer_list_types.h"

namespace ash {

class KioskAppManagerObserver : public base::CheckedObserver {
 public:
  // Invoked when the app data is changed or loading state is changed.
  virtual void OnKioskAppDataChanged(const std::string& app_id) {}

  // Invoked when failed to load web store data of an app.
  virtual void OnKioskAppDataLoadFailure(const std::string& app_id) {}

  // Invoked when the extension is loaded in cache.
  virtual void OnKioskExtensionLoadedInCache(const std::string& app_id) {}

  // Invoked when the extension download fails.
  virtual void OnKioskExtensionDownloadFailed(const std::string& app_id) {}

  // Invoked when the Kiosk Apps configuration changes.
  virtual void OnKioskAppsSettingsChanged() {}

  // Invoked when kiosk app cache is updated for `app_id`.
  virtual void OnKioskAppCacheUpdated(const std::string& app_id) {}

  // Invoked when kiosk app updating from usb stick has been completed.
  // `success` indicates if all the updates are completed successfully.
  virtual void OnKioskAppExternalUpdateComplete(bool success) {}

  // Called when kiosk app session initialization is complete - i.e. when
  // KioskChromeAppManager::InitSession() is called.
  virtual void OnKioskSessionInitialized() {}

 protected:
  ~KioskAppManagerObserver() override = default;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_MANAGER_OBSERVER_H_
