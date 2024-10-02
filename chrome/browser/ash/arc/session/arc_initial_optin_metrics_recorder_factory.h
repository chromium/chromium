// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_ARC_INITIAL_OPTIN_METRICS_RECORDER_FACTORY_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_ARC_INITIAL_OPTIN_METRICS_RECORDER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace arc {

class ArcInitialOptInMetricsRecorder;

class ArcInitialOptInMetricsRecorderFactory
    : public ProfileKeyedServiceFactory {
 public:
  static ArcInitialOptInMetricsRecorder* GetForBrowserContext(
      content::BrowserContext* context);

  static ArcInitialOptInMetricsRecorderFactory* GetInstance();

  ArcInitialOptInMetricsRecorderFactory(
      const ArcInitialOptInMetricsRecorderFactory&) = delete;
  ArcInitialOptInMetricsRecorderFactory& operator=(
      const ArcInitialOptInMetricsRecorderFactory&) = delete;

 private:
  friend base::DefaultSingletonTraits<ArcInitialOptInMetricsRecorderFactory>;

  ArcInitialOptInMetricsRecorderFactory();

  ~ArcInitialOptInMetricsRecorderFactory() override;

  // ProfileKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_ARC_INITIAL_OPTIN_METRICS_RECORDER_FACTORY_H_
