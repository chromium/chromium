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

class KioskAppDataBase : public KioskAppIconLoader::Delegate {
 public:
  // Dictionary key for apps.
  static const char kKeyApps[];

  const std::string& dictionary_name() const { return dictionary_name_; }
  const std::string& app_id() const { return app_id_; }
  const AccountId& account_id() const { return account_id_; }
  const std::string& name() const { return name_; }
  const gfx::ImageSkia& icon() const { return icon_; }

  // Callbacks for KioskAppIconLoader.
  void OnIconLoadSuccess(const gfx::ImageSkia& icon) override = 0;
  void OnIconLoadFailure() override = 0;

  // Clears locally cached data.
  void ClearCache();

 protected:
  KioskAppDataBase(const std::string& dictionary_name,
                   const std::string& app_id,
                   const AccountId& account_id);
  KioskAppDataBase(const KioskAppDataBase&) = delete;
  KioskAppDataBase& operator=(const KioskAppDataBase&) = delete;
  ~KioskAppDataBase() override;

  // Helper to save name and icon to provided dictionary.
  void SaveToDictionary(ScopedDictPrefUpdate& dict_update);

  // Helper to save icon to provided dictionary.
  void SaveIconToDictionary(ScopedDictPrefUpdate& dict_update);

  // Helper to load name and icon from provided dictionary.
  // if |lazy_icon_load| is set to true, the icon will not be updated, only
  // icon_path_.
  bool LoadFromDictionary(const base::Value::Dict& dict,
                          bool lazy_icon_load = false);

  // Starts loading the icon from |icon_path_|;
  void DecodeIcon();

  // Helper to cache |icon| to |cache_dir|.
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
