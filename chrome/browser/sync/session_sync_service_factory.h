// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SESSION_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_SESSION_SYNC_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class GURL;
class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace sync_sessions {
class SessionSyncService;
}  // namespace sync_sessions

class SessionSyncServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static sync_sessions::SessionSyncService* GetForProfile(Profile* profile);
  static SessionSyncServiceFactory* GetInstance();

  static bool ShouldSyncURLForTesting(const GURL& url);

 private:
  friend struct base::DefaultSingletonTraits<SessionSyncServiceFactory>;

  SessionSyncServiceFactory();
  ~SessionSyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(SessionSyncServiceFactory);
};

#endif  // CHROME_BROWSER_SYNC_SESSION_SYNC_SERVICE_FACTORY_H_
