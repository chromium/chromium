// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BADGING_BADGE_MANAGER_FACTORY_H_
#define CHROME_BROWSER_BADGING_BADGE_MANAGER_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

class Profile;

namespace badging {

class BadgeManager;

// Singleton that provides access to Profile specific BadgeManagers.
class BadgeManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Gets the BadgeManager for the current profile. |nullptr| for guest and
  // incognito profiles.
  static BadgeManager* GetForProfile(Profile* profile);

  // Returns the BadgeManagerFactory singleton.
  static BadgeManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<BadgeManagerFactory>;

  BadgeManagerFactory();
  ~BadgeManagerFactory() override;

  // BrowserContextKeyedServiceFactory
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(BadgeManagerFactory);
};

}  // namespace badging

#endif  // CHROME_BROWSER_BADGING_BADGE_MANAGER_FACTORY_H_
