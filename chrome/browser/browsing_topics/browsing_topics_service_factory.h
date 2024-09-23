// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace browsing_topics {

class BrowsingTopicsService;

// Singleton that owns all BrowsingTopicsService and associates them with
// Profiles.
class BrowsingTopicsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the topics service for the given profile, or nullptr if the Topics
  // API or its dependencies are disabled.
  static BrowsingTopicsService* GetForProfile(Profile* profile);

  static BrowsingTopicsServiceFactory* GetInstance();

  BrowsingTopicsServiceFactory(const BrowsingTopicsServiceFactory&) = delete;
  BrowsingTopicsServiceFactory& operator=(const BrowsingTopicsServiceFactory&) =
      delete;
  BrowsingTopicsServiceFactory(BrowsingTopicsServiceFactory&&) = delete;
  BrowsingTopicsServiceFactory& operator=(BrowsingTopicsServiceFactory&&) =
      delete;

 private:
  friend class base::NoDestructor<BrowsingTopicsServiceFactory>;

  BrowsingTopicsServiceFactory();
  ~BrowsingTopicsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace browsing_topics

#endif  // CHROME_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_SERVICE_FACTORY_H_
