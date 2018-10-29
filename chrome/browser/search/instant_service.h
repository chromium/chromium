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
#include "build/build_config.h"
#include "chrome/common/search.mojom.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/search/url_validity_checker.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#error "Instant is only used on desktop";
#endif

class InstantIOContext;
class InstantServiceObserver;
class NtpBackgroundService;
class Profile;
struct InstantMostVisitedItem;
struct ThemeBackgroundInfo;

namespace content {
class RenderProcessHost;
}  // namespace content

// Tracks render process host IDs that are associated with Instant, i.e.
// processes that are used to render an NTP. Also responsible for keeping
// necessary information (most visited tiles and theme info) updated in those
// renderer processes.
class InstantService : public KeyedService,
                       public content::NotificationObserver,
                       public ntp_tiles::MostVisitedSites::Observer {
 public:
  explicit InstantService(Profile* profile);
  ~InstantService() override;

  // Add, remove, and query RenderProcessHost IDs that are associated with
  // Instant processes.
  void AddInstantProcess(int process_id);
  bool IsInstantProcess(int process_id) const;

  // Adds/Removes InstantService observers.
  void AddObserver(InstantServiceObserver* observer);
  void RemoveObserver(InstantServiceObserver* observer);

  // Register prefs associated with the NTP.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

#if defined(UNIT_TEST)
  int GetInstantProcessCount() const {
    return process_ids_.size();
  }
#endif

  // Invoked whenever an NTP is opened. Causes an async refresh of Most Visited
  // items.
  void OnNewTabPageOpened();

  // Most visited item APIs.
  //
  // Invoked when the Instant page wants to delete a Most Visited item.
  void DeleteMostVisitedItem(const GURL& url);
  // Invoked when the Instant page wants to undo the deletion.
  void UndoMostVisitedDeletion(const GURL& url);
  // Invoked when the Instant page wants to undo all Most Visited deletions.
  void UndoAllMostVisitedDeletions();
  // Invoked when the Instant page wants to add a custom link.
  bool AddCustomLink(const GURL& url, const std::string& title);
  // Invoked when the Instant page wants to update a custom link.
  bool UpdateCustomLink(const GURL& url,
                        const GURL& new_url,
                        const std::string& new_title);
  // Invoked when the Instant page wants to delete a custom link.
  bool DeleteCustomLink(const GURL& url);
  // Invoked when the Instant page wants to undo the previous custom link
  // action. Returns false and does nothing if the profile is using a non-Google
  // search provider.
  bool UndoCustomLinkAction();
  // Invoked when the Instant page wants to delete all custom links and use Most
  // Visited sites instead. Returns false and does nothing if the profile is
  // using a non-Google search provider.
  bool ResetCustomLinks();

  // Invoked during the add/update a custom link flow. Creates a request to
  // check if |url| resolves to an existing page and notifies the frontend of
  // the result. This will be used to determine if we need to use "http" instead
  // of the default "https" scheme for the link's URL. Custom links must be
  // enabled.
  void DoesUrlResolve(
      const GURL& url,
      chrome::mojom::EmbeddedSearch::DoesUrlResolveCallback callback);

  // Invoked by the InstantController to update theme information for NTP.
  //
  // TODO(kmadhusu): Invoking this from InstantController shouldn't be
  // necessary. Investigate more and remove this from here.
  void UpdateThemeInfo();

  // Invoked by the InstantController to update most visited items details for
  // NTP.
  void UpdateMostVisitedItemsInfo();

  // Sends the current NTP URL to a renderer process.
  void SendNewTabPageURLToRenderer(content::RenderProcessHost* rph);

  // Invoked when a custom background is selected on the NTP.
  void SetCustomBackgroundURL(const GURL& url);

  // Invoked when a custom background with attributions is selected on the NTP.
  void SetCustomBackgroundURLWithAttributions(
      const GURL& background_url,
      const std::string& attribution_line_1,
      const std::string& attribution_line_2,
      const GURL& action_url);

  // Invoked when a user selected the "Upload an image" option on the NTP.
  void SelectLocalBackgroundImage(const base::FilePath& path);

  // Used for testing.
  ThemeBackgroundInfo* GetThemeInfoForTesting() { return theme_info_.get(); }

  void AddValidBackdropUrlForTesting(const GURL& url) const;

  // Used for testing.
  void SetUrlValidityCheckerForTesting(UrlValidityChecker* url_checker) {
    url_checker_for_testing_ = url_checker;
  }

 private:
  class SearchProviderObserver;

  friend class InstantExtendedTest;
  friend class InstantUnitTestBase;

  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, ProcessIsolation);
  FRIEND_TEST_ALL_PREFIXES(InstantServiceTest, GetNTPTileSuggestion);

  // KeyedService:
  void Shutdown() override;

  // Called when the request from |DoesUrlResolve| finishes. Invokes the
  // associated callback with the request status.
  //
  // If the request exceeded the UI dialog timeout and the URL did not resolve,
  // calls |UpdateCustomLink| to internally update the link's default "https"
  // scheme to "http".
  void OnDoesUrlResolveComplete(
      const GURL& url,
      chrome::mojom::EmbeddedSearch::DoesUrlResolveCallback callback,
      bool resolves,
      base::TimeDelta duration);

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Called when a renderer process is terminated.
  void OnRendererProcessTerminated(int process_id);

  // Called when the search provider changes. Disables custom links if the
  // search provider is not Google.
  void OnSearchProviderChanged(bool is_google);

  // ntp_tiles::MostVisitedSites::Observer implementation.
  void OnURLsAvailable(
      const std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector>&
          sections) override;
  void OnIconMadeAvailable(const GURL& site_url) override;

  void NotifyAboutMostVisitedItems();
  void NotifyAboutThemeInfo();

  void BuildThemeInfo();

  void ApplyOrResetCustomBackgroundThemeInfo();

  void ApplyCustomBackgroundThemeInfo();
  void ApplyCustomBackgroundThemeInfoFromLocalFile(bool file_exists);

  void ResetCustomBackgroundThemeInfo();

  void FallbackToDefaultThemeInfo();

  void RemoveLocalBackgroundImageCopy();

  // Update the background pref to point to
  // chrome-search://local-ntp/background.jpg
  void SetBackgroundToLocalResource();

  // Returns the owned instance of UrlValidityChecker or
  // |url_checker_for_testing_| if not null. Should only be called from the UI
  // thread.
  UrlValidityChecker* GetUrlValidityChecker();

  Profile* const profile_;

  // The process ids associated with Instant processes.
  std::set<int> process_ids_;

  // InstantMostVisitedItems for NTP tiles, received from |most_visited_sites_|.
  std::vector<InstantMostVisitedItem> most_visited_items_;

  // Theme-related data for NTP overlay to adopt themes.
  std::unique_ptr<ThemeBackgroundInfo> theme_info_;

  base::ObserverList<InstantServiceObserver>::Unchecked observers_;

  content::NotificationRegistrar registrar_;

  scoped_refptr<InstantIOContext> instant_io_context_;

  // Data source for NTP tiles (aka Most Visited tiles). May be null.
  std::unique_ptr<ntp_tiles::MostVisitedSites> most_visited_sites_;

  // Keeps track of any changes in search engine provider. May be null.
  std::unique_ptr<SearchProviderObserver> search_provider_observer_;

  // Test UrlValidityChecker used for testing.
  UrlValidityChecker* url_checker_for_testing_ = nullptr;

  PrefChangeRegistrar pref_change_registrar_;

  PrefService* pref_service_;

  NtpBackgroundService* background_service_;

  base::WeakPtrFactory<InstantService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(InstantService);
};

#endif  // CHROME_BROWSER_SEARCH_INSTANT_SERVICE_H_
