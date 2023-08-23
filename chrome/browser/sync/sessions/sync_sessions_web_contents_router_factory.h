// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SESSIONS_SYNC_SESSIONS_WEB_CONTENTS_ROUTER_FACTORY_H_
#define CHROME_BROWSER_SYNC_SESSIONS_SYNC_SESSIONS_WEB_CONTENTS_ROUTER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace sync_sessions {

class SyncSessionsWebContentsRouter;

class SyncSessionsWebContentsRouterFactory : public ProfileKeyedServiceFactory {
 public:
  // Get the SyncSessionsWebContentsRouter service for |profile|, creating one
  // if needed.
  static SyncSessionsWebContentsRouter* GetForProfile(Profile* profile);

  // Get the singleton instance of the factory.
  static SyncSessionsWebContentsRouterFactory* GetInstance();

  SyncSessionsWebContentsRouterFactory(
      const SyncSessionsWebContentsRouterFactory&) = delete;
  SyncSessionsWebContentsRouterFactory& operator=(
      const SyncSessionsWebContentsRouterFactory&) = delete;

 private:
  friend base::NoDestructor<SyncSessionsWebContentsRouterFactory>;

  SyncSessionsWebContentsRouterFactory();
  ~SyncSessionsWebContentsRouterFactory() override;

  // Overridden from BrowserContextKeyedServiceFactory.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace sync_sessions

#endif  // CHROME_BROWSER_SYNC_SESSIONS_SYNC_SESSIONS_WEB_CONTENTS_ROUTER_FACTORY_H_
