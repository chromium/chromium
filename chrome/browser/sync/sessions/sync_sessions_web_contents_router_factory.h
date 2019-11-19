// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SESSIONS_SYNC_SESSIONS_WEB_CONTENTS_ROUTER_FACTORY_H_
#define CHROME_BROWSER_SYNC_SESSIONS_SYNC_SESSIONS_WEB_CONTENTS_ROUTER_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace sync_sessions {

class SyncSessionsWebContentsRouter;

class SyncSessionsWebContentsRouterFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Get the SyncSessionsWebContentsRouter service for |profile|, creating one
  // if needed.
  static SyncSessionsWebContentsRouter* GetForProfile(Profile* profile);

  // Get the singleton instance of the factory.
  static SyncSessionsWebContentsRouterFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      SyncSessionsWebContentsRouterFactory>;

  SyncSessionsWebContentsRouterFactory();
  ~SyncSessionsWebContentsRouterFactory() override;

  // Overridden from BrowserContextKeyedServiceFactory.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(SyncSessionsWebContentsRouterFactory);
};

}  // namespace sync_sessions

#endif  // CHROME_BROWSER_SYNC_SESSIONS_SYNC_SESSIONS_WEB_CONTENTS_ROUTER_FACTORY_H_
