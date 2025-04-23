// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOADER_FROM_GWS_NAVIGATION_AND_KEEP_ALIVE_REQUEST_TRACKER_FACTORY_H_
#define CHROME_BROWSER_LOADER_FROM_GWS_NAVIGATION_AND_KEEP_ALIVE_REQUEST_TRACKER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class FromGWSNavigationAndKeepAliveRequestTracker;
class Profile;

// `FromGWSNavigationAndKeepAliveRequestTrackerFactory` creates a unique
// `FromGWSNavigationAndKeepAliveRequestTracker` per profile.
class FromGWSNavigationAndKeepAliveRequestTrackerFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Not copyable.
  FromGWSNavigationAndKeepAliveRequestTrackerFactory(
      const FromGWSNavigationAndKeepAliveRequestTrackerFactory&) = delete;
  FromGWSNavigationAndKeepAliveRequestTrackerFactory& operator=(
      const FromGWSNavigationAndKeepAliveRequestTrackerFactory&) = delete;

  // Returns the no destructor instance of
  // `FromGWSNavigationAndKeepAliveRequestTrackerFactory`, or nullptr if the
  // feature is disabled.
  static FromGWSNavigationAndKeepAliveRequestTrackerFactory* GetInstance();

  // Returns the `FromGWSNavigationAndKeepAliveRequestTracker` for the given
  // `profile`, or nullptr if the feature is disabled.
  static FromGWSNavigationAndKeepAliveRequestTracker* GetForProfile(
      Profile* profile);

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<
      FromGWSNavigationAndKeepAliveRequestTrackerFactory>;

  FromGWSNavigationAndKeepAliveRequestTrackerFactory();
  ~FromGWSNavigationAndKeepAliveRequestTrackerFactory() override;

  // `BrowserContextKeyedServiceFactory` overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_LOADER_FROM_GWS_NAVIGATION_AND_KEEP_ALIVE_REQUEST_TRACKER_FACTORY_H_
