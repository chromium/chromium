// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_ANDROID_CUSTOM_BACKGROUND_SERVICE_H_
#define CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_ANDROID_CUSTOM_BACKGROUND_SERVICE_H_

#include "components/themes/ntp_custom_background_service_base.h"

class PrefRegistrySimple;
class Profile;
class NtpThemeCollectionBridge;
class NtpSyncedThemeBridge;

namespace base {
class FilePath;
}  // namespace base

// Android-specific service for managing custom backgrounds on the NTP.
class NtpAndroidCustomBackgroundService
    : public NtpCustomBackgroundServiceBase {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit NtpAndroidCustomBackgroundService(Profile* profile);
  ~NtpAndroidCustomBackgroundService() override;

  void SetThemeCollectionBridge(NtpThemeCollectionBridge* bridge);
  void SetSyncedThemeBridge(NtpSyncedThemeBridge* bridge);

  // NtpCustomBackgroundServiceBase:
  void SetCustomBackgroundInfo(const GURL& background_url,
                               const GURL& thumbnail_url,
                               const std::string& attribution_line_1,
                               const std::string& attribution_line_2,
                               const GURL& action_url,
                               const std::string& collection_id) override;
  void SelectLocalBackgroundImage(const base::FilePath& path) override;
  void OnNextCollectionImageAvailable() override;
  std::optional<int> GetNextRefreshTimestamp() const override;

 protected:
  void NotifyAboutBackgrounds() override;

 private:
  // Determines if the updated background information is for the next daily
  // refresh image.
  //
  // This is true if the new image belongs to the same collection as the
  // current one and daily refresh is enabled for both.
  //
  // `new_info`: The incoming CustomBackground info.
  bool IsNextThemeCollectionImage(const CustomBackground& new_info);

  raw_ptr<NtpThemeCollectionBridge> theme_collection_bridge_ = nullptr;
  raw_ptr<NtpSyncedThemeBridge> synced_theme_bridge_ = nullptr;

  // Cache of the last user-selected or active custom background state, used
  // to differentiate between daily refresh setup (first select) and
  // subsequent cycle refreshes, and to ignore stale async updates.
  std::optional<CustomBackground> active_custom_background_;
};

#endif  // CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_ANDROID_CUSTOM_BACKGROUND_SERVICE_H_
