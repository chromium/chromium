// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SPARKY_SPARKY_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_SPARKY_SPARKY_DELEGATE_IMPL_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chromeos/ash/components/sparky/snapshot_util.h"
#include "components/manta/sparky/sparky_delegate.h"

class Profile;

namespace ash {

class SparkyDelegateImpl : public manta::SparkyDelegate {
 public:
  explicit SparkyDelegateImpl(Profile* profile);
  ~SparkyDelegateImpl() override;

  SparkyDelegateImpl(const SparkyDelegateImpl&) = delete;
  SparkyDelegateImpl& operator=(const SparkyDelegateImpl&) = delete;

  // manta::SparkyDelegate
  bool SetSettings(std::unique_ptr<manta::SettingsData> settings_data) override;
  SettingsDataList* GetSettingsList() override;
  std::optional<base::Value> GetSettingValue(
      const std::string& setting_id) override;
  void GetScreenshot(manta::ScreenshotDataCallback callback) override;
  std::vector<manta::AppsData> GetAppsList() override;
  void LaunchApp(const std::string& app_id) override;

 private:
  friend class SparkyDelegateImplTest;

  void AddPrefToMap(
      const std::string& pref_name,
      extensions::api::settings_private::PrefType settings_pref_type,
      std::optional<base::Value> value);

  const raw_ptr<Profile> profile_;
  std::unique_ptr<extensions::PrefsUtil> prefs_util_;
  SettingsDataList current_prefs_;
  std::unique_ptr<sparky::ScreenshotHandler> screenshot_handler_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SPARKY_SPARKY_DELEGATE_IMPL_H_
