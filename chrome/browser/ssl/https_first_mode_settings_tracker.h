// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_HTTPS_FIRST_MODE_SETTINGS_TRACKER_H_
#define CHROME_BROWSER_SSL_HTTPS_FIRST_MODE_SETTINGS_TRACKER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;
namespace base {
class Clock;
}

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

  // Check the Site Engagement scores of the hostname of `url` and enable
  // HFM on the hostname if the HTTPS score is high enough.
  void MaybeEnableHttpsFirstModeForUrl(const GURL& url);

  // Check previous HTTPS-Upgrade fallback events and enable HTTPS-First Mode
  // if the user typically visits secure sites. HTTPS-Upgrade fallback events
  // are stored in a pref. This method extracts the fallback events, deletes old
  // events, adds a new event if `add_fallback_entry` is true and updates the
  // pref.
  bool MaybeEnableHttpsFirstModeForUser(bool add_fallback_entry);

  // Sets the clock for use in tests.
  void SetClockForTesting(base::Clock* clock);

 private:
  void OnHttpsFirstModePrefChanged();

  raw_ptr<Profile> profile_;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<base::Clock> clock_;

  base::ScopedObservation<
      safe_browsing::AdvancedProtectionStatusManager,
      safe_browsing::AdvancedProtectionStatusManager::StatusChangedObserver>
      obs_{this};
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

 private:
  friend struct base::DefaultSingletonTraits<HttpsFirstModeServiceFactory>;

  HttpsFirstModeServiceFactory();
  ~HttpsFirstModeServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SSL_HTTPS_FIRST_MODE_SETTINGS_TRACKER_H_
