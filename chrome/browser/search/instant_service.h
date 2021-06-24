// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_INSTANT_SERVICE_H_
#define CHROME_BROWSER_SEARCH_INSTANT_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/search/background/ntp_background_service.h"
#include "chrome/browser/search/background/ntp_background_service_observer.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#error "Instant is only used on desktop";
#endif

class InstantServiceObserver;
class Profile;
struct CollectionImage;
struct InstantMostVisitedInfo;
struct NtpTheme;

namespace base {
class Clock;
}  // namespace base

namespace content {
class BrowserContext;
class RenderProcessHost;
}  // namespace content

extern const char kNtpCustomBackgroundMainColor[];

// Tracks render process host IDs that are associated with Instant, i.e.
// processes that are used to render an NTP. Also responsible for keeping
// necessary information (most visited tiles and theme info) updated in those
// renderer processes.
class InstantService : public KeyedService,
                       public NtpBackgroundServiceObserver,
                       public content::NotificationObserver,
                       public ntp_tiles::MostVisitedSites::Observer,
                       public ui::NativeThemeObserver,
                       public ThemeServiceObserver {
 public:
  explicit InstantService(Profile* profile);
  ~InstantService() override;

  // Add, remove, and query RenderProcessHost IDs that are associated with
  // Instant processes.
  void AddInstantProcess(int process_id);
  bool IsInstantProcess(int process_id) const;

  // Adds/Removes InstantService observers.
  virtual void AddObserver(InstantServiceObserver* observer);
  void RemoveObserver(InstantServiceObserver* observer);

  // Register prefs associated with the NTP.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Determine if this chrome-search: request is coming from an Instant render
  // process.
  static bool ShouldServiceRequest(const GURL& url,
                                   content::BrowserContext* browser_context,
                                   int render_process_id);

#if defined(UNIT_TEST)
  int GetInstantProcessCount() const {
    return process_ids_.size();
  }
#endif

  // Invoked whenever an NTP is opened. Causes an async refresh of Most Visited
  // items.
  void OnNewTabPageOpened();

  // ThemeServiceObserver implementation.
  void OnThemeChanged() override;

  // Most visited item APIs.
  //
  // Invoked when the Instant page wants to delete a Most Visited item.
  void DeleteMostVisitedItem(const GURL& url);
  // Invoked when the Instant page wants to undo the deletion.
  void UndoMostVisitedDeletion(const GURL& url);
  // Invoked when the Instant page wants to undo all Most Visited deletions.
  void UndoAllMostVisitedDeletions();

  // Invoked to update theme information for the NTP.
  virtual void UpdateNtpTheme();

  // Invoked when a background pref update is received via sync, triggering
  // an update of theme info.
  void UpdateBackgroundFromSync();

  // Invoked by the InstantController to update most visited items details for
  // NTP.
  void UpdateMostVisitedInfo();

  // Invoked when the background is reset on the NTP.
  void ResetCustomBackgroundInfo();

  // Invoked when a custom background is configured on the NTP.
  void SetCustomBackgroundInfo(const GURL& background_url,
                               const std::string& attribution_line_1,
                               const std::string& attribution_line_2,
                               const GURL& action_url,
                               const std::string& collection_id);

  // Invoked when a user selected the "Upload an image" option on the NTP.
  void SelectLocalBackgroundImage(const base::FilePath& path);

  // Getter for |theme_| that will also initialize it if necessary.
  NtpTheme* GetInitializedNtpTheme();

  // Used for testing.
  void SetNativeThemeForTesting(ui::NativeTheme* theme);

  // Used for testing.
  void AddValidBackdropUrlForTesting(const GURL& url) const;

  // Used for testing.
  void AddValidBackdropCollectionForTesting(
      const std::string& collection_id) const;

  // Used for testing.
  void SetNextCollectionImageForTesting(const CollectionImage& image) const;

  // Returns whether having a custom background is disabled by policy.
  bool IsCustomBackgroundDisabledByPolicy();

  // Returns whether a custom background has been set by the user.
  bool IsCustomBackgroundSet();

  // Reset all NTP customizations to default. Marked virtual for mocking in
  // tests.
  virtual void ResetToDefault();

  // Calculates the most frequent color of the image and stores it in prefs.
  void UpdateCustomBackgroundColorAsync(
      base::TimeTicks timestamp,
      const gfx::Image& fetched_image,
      const image_fetcher::RequestMetadata& metadata);

  // Fetches the image for the given |fetch_url|.
  void FetchCustomBackground(base::TimeTicks timestamp, const GURL& fetch_url);

 private:
  friend class InstantExtendedTest;
  friend class InstantUnitTestBase;
  friend class LocalNTPBackgroundsAndDarkModeTest;
  friend class TestInstantService;

  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, ProcessIsolation);
  FRIEND_TEST_ALL_PREFIXES(InstantServiceTest, GetNTPTileSuggestion);
  FRIEND_TEST_ALL_PREFIXES(InstantServiceTest, DoesToggleShortcutsVisibility);
  FRIEND_TEST_ALL_PREFIXES(InstantServiceTest, TestNoNtpTheme);
  FRIEND_TEST_ALL_PREFIXES(InstantServiceTest, TestUpdateCustomBackgroundColor);
  FRIEND_TEST_ALL_PREFIXES(InstantServiceTest,
                           LocalImageDoesNotUpdateCustomBackgroundColor);
  FRIEND_TEST_ALL_PREFIXES(InstantServiceTest, RefreshesBackgroundAfter24Hours);

  // KeyedService:
  void Shutdown() override;

  // NtpBackgroundServiceObserver:
  void OnCollectionInfoAvailable() override {}
  void OnCollectionImagesAvailable() override {}
  void OnNextCollectionImageAvailable() override;
  void OnNtpBackgroundServiceShuttingDown() override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Called when a renderer process is terminated.
  void OnRendererProcessTerminated(int process_id);

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  // ntp_tiles::MostVisitedSites::Observer implementation.
  void OnURLsAvailable(
      const std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector>&
          sections) override;
  void OnIconMadeAvailable(const GURL& site_url) override;

  void NotifyAboutMostVisitedInfo();
  void NotifyAboutNtpTheme();

  void BuildNtpTheme();

  void ApplyOrResetCustomBackgroundNtpTheme();

  void ApplyCustomBackgroundNtpTheme();

  // Marked virtual for mocking in tests.
  virtual void ResetCustomBackgroundNtpTheme();

  void FallbackToDefaultNtpTheme();

  void RemoveLocalBackgroundImageCopy();

  // Returns false if the custom background pref cannot be parsed, otherwise
  // returns true and sets custom_background_url to the value in the pref.
  bool IsCustomBackgroundPrefValid(GURL& custom_background_url);

  // Update the background pref to point to
  // chrome://new-tab-page/background.jpg.
  void SetBackgroundToLocalResource();

  // Updates custom background prefs with color if the background hasn't changed
  // since the calculation started.
  void UpdateCustomBackgroundPrefsWithColor(base::TimeTicks timestamp,
                                            SkColor color);

  void SetImageFetcherForTesting(image_fetcher::ImageFetcher* image_fetcher);

  void SetClockForTesting(base::Clock* clock);

  base::TimeTicks GetBackgroundUpdatedTimestampForTesting() {
    return background_updated_timestamp_;
  }

  // Requests a new background image if it hasn't been updated in >24 hours.
  void RefreshBackgroundIfNeeded();

  // Sets NTP elements theme info that are overridden when custom
  // background is used.
  void SetNtpElementsNtpTheme();

  Profile* const profile_;

  // The process ids associated with Instant processes.
  std::set<int> process_ids_;

  // Contains InstantMostVisitedItems received from |most_visited_sites_| and
  // information required to display NTP tiles.
  std::unique_ptr<InstantMostVisitedInfo> most_visited_info_;

  // Theme-related data for NTP overlay to adopt themes.
  std::unique_ptr<NtpTheme> theme_;

  base::ObserverList<InstantServiceObserver>::Unchecked observers_;

  content::NotificationRegistrar registrar_;

  // Data source for NTP tiles (aka Most Visited tiles). May be null.
  std::unique_ptr<ntp_tiles::MostVisitedSites> most_visited_sites_;

  PrefChangeRegistrar pref_change_registrar_;

  PrefService* pref_service_;

  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      theme_observation_{this};

  base::ScopedObservation<NtpBackgroundService, NtpBackgroundServiceObserver>
      background_service_observation_{this};

  ui::NativeTheme* native_theme_;

  NtpBackgroundService* background_service_;

  std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher_;

  base::TimeTicks background_updated_timestamp_;

  base::Clock* clock_;

  base::WeakPtrFactory<InstantService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InstantService);
};

#endif  // CHROME_BROWSER_SEARCH_INSTANT_SERVICE_H_
