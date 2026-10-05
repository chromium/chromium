// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_COPY_SEARCH_JOURNEY_TRACKER_FACTORY_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_COPY_SEARCH_JOURNEY_TRACKER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace contextual_tasks {

class CopySearchJourneyTracker;

class CopySearchJourneyTrackerFactory : public ProfileKeyedServiceFactory {
 public:
  static CopySearchJourneyTracker* GetForProfile(Profile* profile);
  static CopySearchJourneyTrackerFactory* GetInstance();

  CopySearchJourneyTrackerFactory(const CopySearchJourneyTrackerFactory&) =
      delete;
  CopySearchJourneyTrackerFactory& operator=(
      const CopySearchJourneyTrackerFactory&) = delete;

 private:
  friend base::NoDestructor<CopySearchJourneyTrackerFactory>;

  CopySearchJourneyTrackerFactory();
  ~CopySearchJourneyTrackerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_COPY_SEARCH_JOURNEY_TRACKER_FACTORY_H_
