// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_ANDROID_CUSTOM_BACKGROUND_SERVICE_H_
#define CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_ANDROID_CUSTOM_BACKGROUND_SERVICE_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/model/data_type_store.h"
#include "components/themes/ntp_custom_background_service_base.h"

class PrefRegistrySimple;
class Profile;
class NtpThemeCollectionBridge;
class NtpSyncedThemeBridge;

namespace base {
class FilePath;
}  // namespace base

namespace sync_pb {
class ThemeAndroidSpecifics;
}  // namespace sync_pb

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace ntp_customization {
class NtpAndroidThemeSyncBridge;
}  // namespace ntp_customization

// Android-specific service for managing custom backgrounds on the NTP.
class NtpAndroidCustomBackgroundService
    : public NtpCustomBackgroundServiceBase {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Constructs a custom background service for the given profile and
  // initializes the theme sync bridge if the feature flag is enabled.
  NtpAndroidCustomBackgroundService(
      Profile* profile,
      syncer::OnceDataTypeStoreFactory store_factory);
  ~NtpAndroidCustomBackgroundService() override;

  void SetThemeCollectionBridge(NtpThemeCollectionBridge* bridge);
  void SetSyncedThemeBridge(NtpSyncedThemeBridge* bridge);

  // Returns the DataTypeControllerDelegate for the THEMES_ANDROID sync data
  // type, or nullptr if sync is disabled.
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetSyncControllerDelegate();

  // Exposes whether the underlying service is actively processing a sync
  // update. This is used by observers to route UI behavior differently when
  // syncing versus local manual updates.
  bool IsProcessingSyncUpdate() const { return processing_sync_update_; }

  // NtpCustomBackgroundServiceBase:
  void SetCustomBackgroundInfo(const GURL& background_url,
                               const GURL& thumbnail_url,
                               const std::string& attribution_line_1,
                               const std::string& attribution_line_2,
                               const GURL& action_url,
                               const std::string& collection_id) override;
  void SelectLocalBackgroundImage(const base::FilePath& path) override;
  void ResetCustomBackgroundInfo() override;
  void OnNextCollectionImageAvailable() override;
  std::optional<int> GetNextRefreshTimestamp() const override;
  bool UpdateCustomBackgroundPrefsWithColor(const GURL& image_url,
                                            SkColor color) override;

  // Callback invoked when incoming theme changes are received from Chrome Sync.
  void OnThemeChangedFromSync(const sync_pb::ThemeAndroidSpecifics& specifics);

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

  // Pushes the current local custom background state out to Chrome Sync.
  void NotifySyncBridge();

  raw_ptr<NtpThemeCollectionBridge> theme_collection_bridge_ = nullptr;
  raw_ptr<NtpSyncedThemeBridge> synced_theme_bridge_ = nullptr;

  // Cache of the last user-selected or active custom background state, used
  // to differentiate between daily refresh setup (first select) and
  // subsequent cycle refreshes, and to ignore stale async updates.
  std::optional<CustomBackground> active_custom_background_;

  // The bridge connecting this service to the Chrome Sync engine for Android
  // themes.
  std::unique_ptr<ntp_customization::NtpAndroidThemeSyncBridge>
      theme_sync_bridge_;

  // True while processing an incoming sync update. Used to route UI updates
  // and prevent bouncing events back to sync.
  bool processing_sync_update_ = false;

  // True while updating the primary color in the background preference
  // dictionary. Used to suppress re-entrant NotifyAboutBackgrounds() calls
  // from PrefChangeRegistrar.
  bool updating_color_pref_ = false;

  base::WeakPtrFactory<NtpAndroidCustomBackgroundService> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_ANDROID_CUSTOM_BACKGROUND_SERVICE_H_
