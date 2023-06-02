// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_ENGAGEMENT_TRACKER_FACTORY_H_
#define CHROME_BROWSER_FEATURE_ENGAGEMENT_TRACKER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace feature_engagement {
class Tracker;

// TrackerFactory is the main client class for interaction with
// the feature_engagement component.
class TrackerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns singleton instance of TrackerFactory.
  static TrackerFactory* GetInstance();

  // Returns the feature_engagement::Tracker associated with |context|.
  static feature_engagement::Tracker* GetForBrowserContext(
      content::BrowserContext* context);

  TrackerFactory(const TrackerFactory&) = delete;
  TrackerFactory& operator=(const TrackerFactory&) = delete;

 private:
  friend base::NoDestructor<TrackerFactory>;

  TrackerFactory();
  ~TrackerFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace feature_engagement

#endif  // CHROME_BROWSER_FEATURE_ENGAGEMENT_TRACKER_FACTORY_H_
