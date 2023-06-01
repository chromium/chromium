// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_USER_EVENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_USER_EVENT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace syncer {
class UserEventService;
}  // namespace syncer

namespace browser_sync {

class UserEventServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static syncer::UserEventService* GetForProfile(Profile* profile);
  static UserEventServiceFactory* GetInstance();

  UserEventServiceFactory(const UserEventServiceFactory&) = delete;
  UserEventServiceFactory& operator=(const UserEventServiceFactory&) = delete;

 private:
  friend base::NoDestructor<UserEventServiceFactory>;

  UserEventServiceFactory();
  ~UserEventServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_USER_EVENT_SERVICE_FACTORY_H_
