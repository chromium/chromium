// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_HTTPS_FIRST_MODE_SETTINGS_TRACKER_H_
#define CHROME_BROWSER_SSL_HTTPS_FIRST_MODE_SETTINGS_TRACKER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_traits.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/ssl/daily_navigation_counter.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "content/public/browser/browser_thread.h"

class Profile;
namespace base {
class Clock;
}

namespace site_engagement {
class SiteEngagementService;
}

class StatefulSSLHostStateDelegate;

// The set of valid states of the user-controllable HTTPS-First Mode setting.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Must be kept in sync with the HttpsFirstModeSetting enums located in
// chrome/browser/resources/settings/privacy_page/security_page.ts and enums.xml
// LINT.IfChange
enum class HttpsFirstModeSetting {
  kDisabled = 0,
  // DEPRECATED: A separate Incognito setting never shipped.
  // kEnabledIncognito = 1,
  kEnabledFull = 2,
  kEnabledBalanced = 3,
  kMaxValue = kEnabledBalanced,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/security/enums.xml)

// A `KeyedService` that tracks changes to the HTTPS-First Mode pref for each
// profile. This is currently used for:
// - Recording pref state in metrics and registering the client for a synthetic
//   field trial based on that state.
// - Changing the pref based on user's Advanced Protection status.
// - Checking the Site Engagement scores of a site and enable/disable HFM based
//   on that.
class HttpsFirstModeService
    : public KeyedService,
      public safe_browsing::AdvancedProtectionStatusManager::
          StatusChangedObserver {
 public:
  explicit HttpsFirstModeService(Profile* profile, base::Clock* clock);
  ~HttpsFirstModeService() override;

  HttpsFirstModeService(const HttpsFirstModeService&) = delete;
  HttpsFirstModeService& operator=(const HttpsFirstModeService&) = delete;

  // safe_browsing::AdvancedProtectionStatusManager::StatusChangedObserver:
  void OnAdvancedProtectionStatusChanged(bool enabled) override;

  // Runs Typically Secure User and Site Engagement heuristics after the service
  // is created.
  void AfterStartup();

  // Returns true if the Typically Secure Heuristic enabled HTTPS-First Mode
  // in this profile. Does not update the recorded fallback events list.
  bool IsInterstitialEnabledByTypicallySecureUserHeuristic() const;
  // Records an HTTPS-Upgrade fallback event if the Typically Secure heuristic
  // isn't yet enabled and evicts old fallback events.
  void RecordHttpsUpgradeFallbackEvent();
  // Updates HTTPS-Upgrade fallback events and enables HTTPS-First Balanced Mode
  // if the user typically visits secure sites.
  // The first invocation of this method will almost always be a no-op in
  // browser tests because the method checks that the clock is sufficiently
  // advanced, and tests can't change the clock before getting here. Therefore,
  // browser tests need to call this method explicitly.
  void CheckUserIsTypicallySecureAndMaybeEnableHttpsFirstBalancedMode();

  // Gets the list of engaged sites from Site Engagement service and determines
  // whether HTTPS-First Mode should be enabled on each site. Calls
  // `done_callback` before returning.
  void MaybeEnableHttpsFirstModeForEngagedSites(
      base::OnceClosure done_callback);

  HttpsFirstModeSetting GetCurrentSetting() const;

  // Increment recent navigation count and maybe save the counts to a pref.
  void IncrementRecentNavigationCount();
  // Returns the number of navigations counted recently in a rolling window.
  size_t GetRecentNavigationCount() const;

  // Sets the clock for use in tests.
  void SetClockForTesting(base::Clock* clock);
  // Returns the current number of fallback entries recorded.
  size_t GetFallbackEntryCountForTesting() const;

 private:
  void OnHttpsFirstModePrefChanged();
  // HTTPS-Upgrade fallback events are stored in a pref. This method extracts
  // the fallback events, deletes old events, adds a new event if
  // `add_new_entry` is true. Returns true if the heuristic indicates that
  // HFM can be auto-enabled, but this function doesn't auto-enable it.
  bool UpdateFallbackEntries(bool add_new_entry);
  // Returns true if the user is considered typically secure. Does not
  // auto-enable HFM pref, but updates the fallback events, evicting old ones.
  bool IsUserTypicallySecure();

  // Check the Site Engagement scores of the hostname of `url` and enable
  // HFM on the hostname if the HTTPS score is high enough. `url` should have a
  // default port.
  void MaybeEnableHttpsFirstModeForUrl(
      const GURL& url,
      site_engagement::SiteEngagementService* engagement_service,
      StatefulSSLHostStateDelegate* state);
  // Called after getting the engaged sites list from Site Engagement service.
  // Calls `done_callback` before returning.
  void ProcessEngagedSitesList(
      base::OnceClosure done_callback,
      const std::vector<site_engagement::mojom::SiteEngagementDetails>&
          details);

  // If true, will not clear the HTTP allowlist when the HFM pref changes next
  // time. Will be set to false again upon pref change.
  // The HFM pref can be changed by the UI setting or the Typically Secure User
  // heuristic. We only need to clear the allowlist if the UI setting is. The
  // pref observer has no way of knowing how the pref was changed, so we use
  // this bool to tell it to clear or keep the allowlist.
  bool keep_http_allowlist_on_next_pref_change_ = false;

  raw_ptr<Profile> profile_;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<base::Clock> clock_;

  base::Value::Dict navigation_counts_dict_;
  std::unique_ptr<DailyNavigationCounter> navigation_counter_;

  base::ScopedObservation<
      safe_browsing::AdvancedProtectionStatusManager,
      safe_browsing::AdvancedProtectionStatusManager::StatusChangedObserver>
      obs_{this};

  base::WeakPtrFactory<HttpsFirstModeService> weak_factory_{this};
};

// Factory boilerplate for creating the `HttpsFirstModeService` for each browser
// context (profile).
class HttpsFirstModeServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static HttpsFirstModeService* GetForProfile(Profile* profile);
  static HttpsFirstModeServiceFactory* GetInstance();

  HttpsFirstModeServiceFactory(const HttpsFirstModeServiceFactory&) = delete;
  HttpsFirstModeServiceFactory& operator=(const HttpsFirstModeServiceFactory&) =
      delete;

  // Returns the default factory, useful in tests where it's null by default.
  static BrowserContextKeyedServiceFactory::TestingFactory
  GetDefaultFactoryForTesting();

  // Sets the clock to use when creating the service.
  static base::Clock* SetClockForTesting(base::Clock* clock);

 private:
  friend struct base::DefaultSingletonTraits<HttpsFirstModeServiceFactory>;

  HttpsFirstModeServiceFactory();
  ~HttpsFirstModeServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SSL_HTTPS_FIRST_MODE_SETTINGS_TRACKER_H_
