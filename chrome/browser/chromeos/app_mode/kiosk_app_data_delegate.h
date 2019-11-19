// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_DATA_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_DATA_DELEGATE_H_

#include <string>

namespace base {
class FilePath;
}

namespace chromeos {

class KioskAppDataDelegate {
 public:
  // Invoked to get the root directory for storing cached icon files.
  virtual void GetKioskAppIconCacheDir(base::FilePath* cache_dir) const = 0;

  // Invoked when kiosk app data or status has changed.
  virtual void OnKioskAppDataChanged(const std::string& app_id) const = 0;

  // Invoked when failed to load web store data of an app.
  virtual void OnKioskAppDataLoadFailure(const std::string& app_id) const = 0;

 protected:
  virtual ~KioskAppDataDelegate() {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_DATA_DELEGATE_H_
