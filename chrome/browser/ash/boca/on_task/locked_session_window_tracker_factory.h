// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_ON_TASK_LOCKED_SESSION_WINDOW_TRACKER_FACTORY_H_
#define CHROME_BROWSER_ASH_BOCA_ON_TASK_LOCKED_SESSION_WINDOW_TRACKER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class LockedSessionWindowTracker;

// Singleton factory that builds and owns LockedSessionWindowTracker.
class LockedSessionWindowTrackerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  LockedSessionWindowTrackerFactory(const LockedSessionWindowTrackerFactory&) =
      delete;
  LockedSessionWindowTrackerFactory& operator=(
      const LockedSessionWindowTrackerFactory&) = delete;

  static LockedSessionWindowTrackerFactory* GetInstance();
  static LockedSessionWindowTracker* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<LockedSessionWindowTrackerFactory>;

  LockedSessionWindowTrackerFactory();
  ~LockedSessionWindowTrackerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  // Finds which browser context (if any) to use.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_ASH_BOCA_ON_TASK_LOCKED_SESSION_WINDOW_TRACKER_FACTORY_H_
