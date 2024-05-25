// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_NTP_CUSTOM_BACKGROUND_SERVICE_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_NTP_CUSTOM_BACKGROUND_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/search/background/ntp_background_service.h"
#include "chrome/browser/search/background/ntp_background_service_observer.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_utils.h"

class NtpCustomBackgroundServiceObserver;
class PrefRegistrySimple;
class PrefService;
class Profile;

namespace base {
class Clock;
class FilePath;
}  // namespace base

// Manages custom backgrounds on the new tab page.
class NtpCustomBackgroundService : public KeyedService,
                                   public NtpBackgroundServiceObserver,
                                   public ThemeServiceObserver {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static void ResetNtpTheme(Profile* profile);
  static void ResetProfilePrefs(Profile* profile);

  explicit NtpCustomBackgroundService(Profile* profile);
  ~NtpCustomBackgroundService() override;

  // NtpBackgroundServiceObserver:
  void OnCollectionInfoAvailable() override;
  void OnCollectionImagesAvailable() override;
  void OnNextCollectionImageAvailable() override;
  void OnNtpBackgroundServiceShuttingDown() override;

  // ThemeServiceObserver:
  void OnThemeChanged() override;
  void OnCustomNtpBackgroundObsolete() override;

  // Invoked when a background pref update is received via sync, triggering
  // an update of theme info.
  void UpdateBackgroundFromSync();

  // Invoked when the background is reset on the NTP.
  // Virtual for testing.
  virtual void ResetCustomBackgroundInfo();

  // Invoked when a custom background is configured on the NTP.
  // Virtual for testing.
  virtual void SetCustomBackgroundInfo(const GURL& background_url,
                                       const GURL& thumbnail_url,
                                       const std::string& attribution_line_1,
                                       const std::string& attribution_line_2,
                                       const GURL& action_url,
                                       const std::string& collection_id);

  // Invoked when a user selected the "Upload an image" option on the NTP.
  // Virtual for testing.
  virtual void SelectLocalBackgroundImage(const base::FilePath& path);

  // Set bool pref for local background and set id.
  virtual void SetBackgroundToLocalResourceWithId(const base::Token& id,
                                                  bool is_inspiration_image);

  // Virtual for testing.
  virtual void RefreshBackgroundIfNeeded();

  // Reverts any changes to the background when a background preview
  // is cancelled.
  void RevertBackgroundChanges();
  // Confirms that background has been changed.
  void ConfirmBackgroundChanges();

  // Virtual for testing.
  virtual std::optional<CustomBackground> GetCustomBackground();

  // Adds/Removes NtpCustomBackgroundServiceObserver observers.
  virtual void AddObserver(NtpCustomBackgroundServiceObserver* observer);
  void RemoveObserver(NtpCustomBackgroundServiceObserver* observer);

  // Returns whether having a custom background is disabled by policy.
  virtual bool IsCustomBackgroundDisabledByPolicy();

  // Returns whether a custom background has been set by the user.
  bool IsCustomBackgroundSet();

  void AddValidBackdropUrlForTesting(const GURL& url) const;
  void SetClockForTesting(base::Clock* clock);

  // TODO(crbug.com/40877728): Make private when color extraction is refactored
  // outside of this service.
  // Calculates the most frequent color of the image and stores it in prefs.
  void UpdateCustomBackgroundColorAsync(
      const GURL& image_url,
      const gfx::Image& fetched_image,
      const image_fetcher::RequestMetadata& metadata);

  // TODO(crbug.com/40877728): Make private when color extraction is refactored
  // outside of this service.
  // Calculates the most frequent color of the local image and stores it.
  virtual void UpdateCustomLocalBackgroundColorAsync(const gfx::Image& image);

  // Requests an asynchronous fetch of a custom background image's URL headers.
  // Virtual for testing.
  virtual void VerifyCustomBackgroundImageURL();

 protected:
  // TODO(crbug.com/40877728): Make private when color extraction is refactored
  // outside of this service.
  // Fetches the image for the given |fetch_url| and extract its main color.
  // Virtual for testing.
  virtual void FetchCustomBackgroundAndExtractBackgroundColor(
      const GURL& image_url,
      const GURL& fetch_url);

 private:
  // Set bool pref for local background and clear id.
  void SetBackgroundToLocalResource();

  void ForceRefreshBackground();
  // Returns false if the custom background pref cannot be parsed, otherwise
  // returns true.
  bool IsCustomBackgroundPrefValid();
  void NotifyAboutBackgrounds();

  // Updates custom background prefs with color for the given |image_url|.
  void UpdateCustomBackgroundPrefsWithColor(const GURL& image_url,
                                            SkColor color);

  // Updates prefs with custom background color for local background image.
  void UpdateLocalCustomBackgroundPrefsWithColor(SkColor color);

  // Process local background image for color extraction
  void ProcessLocalImageData(std::string image_data);

  // Callback that updates custom background information after the fetch of its
  // URL's headers has been completed.
  void OnCustomBackgroundURLHeadersReceived(
      const GURL& verified_custom_background_url,
      int headers_response_code);

  const raw_ptr<Profile> profile_;
  raw_ptr<PrefService, DanglingUntriaged> pref_service_;
  raw_ptr<ThemeService, DanglingUntriaged> theme_service_;
  std::unique_ptr<network::SimpleURLLoader> custom_background_image_url_loader_;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<NtpBackgroundService, DanglingUntriaged> background_service_;
  base::ScopedObservation<NtpBackgroundService, NtpBackgroundServiceObserver>
      background_service_observation_{this};
  base::ScopedObservation<ThemeService, ThemeServiceObserver>
      theme_service_observation_{this};
  raw_ptr<base::Clock> clock_;
  base::TimeTicks background_updated_timestamp_;
  base::ObserverList<NtpCustomBackgroundServiceObserver> observers_;
  std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher_;

  // Used to track information for previous background when a background is
  // being previewed.
  std::optional<base::Value> previous_background_info_;
  bool previous_local_background_ = false;

  base::WeakPtrFactory<NtpCustomBackgroundService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_NTP_CUSTOM_BACKGROUND_SERVICE_H_
