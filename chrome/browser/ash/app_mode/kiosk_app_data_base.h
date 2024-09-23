// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_DATA_BASE_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_DATA_BASE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/ash/app_mode/kiosk_app_icon_loader.h"
#include "components/account_id/account_id.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class Value;
}

namespace ash {

class KioskAppDataBase {
 public:
  KioskAppDataBase(const std::string& dictionary_name,
                   const std::string& app_id,
                   const AccountId& account_id);
  KioskAppDataBase(const KioskAppDataBase&) = delete;
  KioskAppDataBase& operator=(const KioskAppDataBase&) = delete;
  virtual ~KioskAppDataBase();

  // Dictionary key for apps.
  static const char kKeyApps[];

  const std::string& dictionary_name() const { return dictionary_name_; }
  const std::string& app_id() const { return app_id_; }
  const AccountId& account_id() const { return account_id_; }
  const std::string& name() const { return name_; }
  const gfx::ImageSkia& icon() const { return icon_; }

  // Clears locally cached data.
  void ClearCache();

 protected:
  // Helper to save name and icon to provided dictionary.
  void SaveToDictionary(ScopedDictPrefUpdate& dict_update);

  // Helper to save icon to provided dictionary.
  void SaveIconToDictionary(ScopedDictPrefUpdate& dict_update);

  // Helper to load name and icon_path from provided dictionary.
  // This method does not load the icon from disk.
  bool LoadFromDictionary(const base::Value::Dict& dict);

  // Starts loading the icon from `icon_path_`;
  void DecodeIcon(KioskAppIconLoader::ResultCallback callback);

  // Helper to cache `icon` to `cache_dir`.
  void SaveIcon(const SkBitmap& icon, const base::FilePath& cache_dir);

  // In protected section to allow derived classes to modify.
  std::string name_;
  gfx::ImageSkia icon_;

  // Should be released when callbacks are called.
  std::unique_ptr<KioskAppIconLoader> kiosk_app_icon_loader_;

 private:
  // Name of a dictionary that holds kiosk app info in Local State.
  const std::string dictionary_name_;

  const std::string app_id_;
  const AccountId account_id_;

  base::FilePath icon_path_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_DATA_BASE_H_
