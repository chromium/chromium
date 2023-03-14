// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_INSTANT_SERVICE_H_
#define CHROME_BROWSER_SEARCH_INSTANT_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "content/public/browser/render_process_host_observer.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
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

// Tracks render process host IDs that are associated with Instant, i.e.
// processes that are used to render an NTP. Also responsible for keeping
// necessary information (most visited tiles and theme info) updated in those
// renderer processes.
class InstantService : public KeyedService,
                       public content::RenderProcessHostObserver,
                       public ntp_tiles::MostVisitedSites::Observer,
                       public ui::NativeThemeObserver,
                       public ThemeServiceObserver {
 public:
  explicit InstantService(Profile* profile);

  InstantService(const InstantService&) = delete;
  InstantService& operator=(const InstantService&) = delete;

  ~InstantService() override;

  // Add RenderProcessHosts that are associated with Instant processes and query
  // based on PID.
  void AddInstantProcess(content::RenderProcessHost* host);
  bool IsInstantProcess(int process_id) const;

  // Adds/Removes InstantService observers.
  virtual void AddObserver(InstantServiceObserver* observer);
  void RemoveObserver(InstantServiceObserver* observer);

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

  // Invoked by the InstantController to update most visited items details for
  // NTP.
  void UpdateMostVisitedInfo();

  // Getter for |theme_| that will also initialize it if necessary.
  NtpTheme* GetInitializedNtpTheme();

  // Used for testing.
  void SetNativeThemeForTesting(ui::NativeTheme* theme);

 private:
  friend class InstantExtendedTest;
  friend class InstantUnitTestBase;
  friend class TestInstantService;

  FRIEND_TEST_ALL_PREFIXES(InstantServiceTest, GetNTPTileSuggestion);
  FRIEND_TEST_ALL_PREFIXES(InstantServiceTest, TestNoNtpTheme);

  // KeyedService:
  void Shutdown() override;

  // content::RenderProcessHostObserver:
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

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

  base::TimeTicks GetBackgroundUpdatedTimestampForTesting() {
    return background_updated_timestamp_;
  }

  // Sets NTP elements theme info that are overridden when custom
  // background is used.
  void SetNtpElementsNtpTheme();

  const raw_ptr<Profile> profile_;

  // The process ids associated with Instant processes.
  std::set<int> process_ids_;

  // Contains InstantMostVisitedItems received from |most_visited_sites_| and
  // information required to display NTP tiles.
  std::unique_ptr<InstantMostVisitedInfo> most_visited_info_;

  // Theme-related data for NTP overlay to adopt themes.
  std::unique_ptr<NtpTheme> theme_;

  base::ObserverList<InstantServiceObserver>::Unchecked observers_;

  // Data source for NTP tiles (aka Most Visited tiles). May be null.
  std::unique_ptr<ntp_tiles::MostVisitedSites> most_visited_sites_;

  PrefChangeRegistrar pref_change_registrar_;

  raw_ptr<PrefService> pref_service_;

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      host_observation_{this};

  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      theme_observation_{this};

  raw_ptr<ui::NativeTheme> native_theme_;

  base::TimeTicks background_updated_timestamp_;

  base::WeakPtrFactory<InstantService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SEARCH_INSTANT_SERVICE_H_
