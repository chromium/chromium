// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_METRICS_SERVICE_H_
#define CHROME_BROWSER_PREFS_PREF_METRICS_SERVICE_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class PrefService;
class Profile;

// PrefMetricsService is responsible for recording prefs-related UMA stats.
class PrefMetricsService : public KeyedService {
 public:
  explicit PrefMetricsService(Profile* profile);
  ~PrefMetricsService() override;

  // Records metrics about the state of the homepage on launch.
  static void RecordHomePageLaunchMetrics(bool show_home_button,
                                          bool homepage_is_ntp,
                                          const GURL& homepage_url);

  class Factory : public BrowserContextKeyedServiceFactory {
   public:
    static Factory* GetInstance();
    static PrefMetricsService* GetForProfile(Profile* profile);
   private:
    friend struct base::DefaultSingletonTraits<Factory>;

    Factory();
    ~Factory() override;

    // BrowserContextKeyedServiceFactory implementation
    KeyedService* BuildServiceInstanceFor(
        content::BrowserContext* profile) const override;
    bool ServiceIsCreatedWithBrowserContext() const override;
    content::BrowserContext* GetBrowserContextToUse(
        content::BrowserContext* context) const override;
  };

 private:
  // Record prefs state on browser context creation.
  void RecordLaunchPrefs();

  Profile* profile_;
  PrefService* prefs_;

  base::WeakPtrFactory<PrefMetricsService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrefMetricsService);
};

#endif  // CHROME_BROWSER_PREFS_PREF_METRICS_SERVICE_H_
