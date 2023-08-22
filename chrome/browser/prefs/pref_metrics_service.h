// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_METRICS_SERVICE_H_
#define CHROME_BROWSER_PREFS_PREF_METRICS_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class PrefService;
class Profile;

// PrefMetricsService is responsible for recording prefs-related UMA stats.
class PrefMetricsService : public KeyedService {
 public:
  explicit PrefMetricsService(Profile* profile);

  PrefMetricsService(const PrefMetricsService&) = delete;
  PrefMetricsService& operator=(const PrefMetricsService&) = delete;

  ~PrefMetricsService() override;

  // Records metrics about various per-profile configurations on profile open.
  static void RecordHomePageLaunchMetrics(bool show_home_button,
                                          bool homepage_is_ntp,
                                          const GURL& homepage_url);

  class Factory : public ProfileKeyedServiceFactory {
   public:
    static Factory* GetInstance();
    static PrefMetricsService* GetForProfile(Profile* profile);
   private:
    friend struct base::DefaultSingletonTraits<Factory>;

    Factory();
    ~Factory() override;

    // BrowserContextKeyedServiceFactory implementation
    std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const override;
    bool ServiceIsCreatedWithBrowserContext() const override;
  };

 private:
  // Record prefs state on browser context creation.
  void RecordLaunchPrefs();

  raw_ptr<Profile> profile_;
  raw_ptr<PrefService> prefs_;

  base::WeakPtrFactory<PrefMetricsService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PREFS_PREF_METRICS_SERVICE_H_
