// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SESSION_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_SESSION_SYNC_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class GURL;
class Profile;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace sync_sessions {
class SessionSyncService;
}  // namespace sync_sessions

class SessionSyncServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static sync_sessions::SessionSyncService* GetForProfile(Profile* profile);
  static SessionSyncServiceFactory* GetInstance();

  SessionSyncServiceFactory(const SessionSyncServiceFactory&) = delete;
  SessionSyncServiceFactory& operator=(const SessionSyncServiceFactory&) =
      delete;

  static bool ShouldSyncURLForTestingAndMetrics(const GURL& url);

 private:
  friend base::NoDestructor<SessionSyncServiceFactory>;

  SessionSyncServiceFactory();
  ~SessionSyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SYNC_SESSION_SYNC_SERVICE_FACTORY_H_
