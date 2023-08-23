// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SESSIONS_SESSION_SERVICE_FACTORY_H_

#include <utility>

#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/sessions/session_service.h"

// Singleton that owns all SessionServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated SessionService.
class SessionServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the session service for |profile|. This may return NULL. If this
  // profile supports a session service (it isn't incognito), and the session
  // service hasn't yet been created, this forces creation of the session
  // service. This returns NULL if ShutdownForProfile has been called for
  // |profile|.
  //
  // This returns NULL if the profile is incognito. Callers should always check
  // the return value for NULL.
  static SessionService* GetForProfile(Profile* profile);

  // Returns the session service for |profile|, but do not create it if it
  // doesn't exist. This returns NULL if the profile is incognito or if session
  // service has been explicitly shutdown (browser is exiting). Callers should
  // always check the return value for NULL.
  static SessionService* GetForProfileIfExisting(Profile* profile);

  // Returns the session service for |profile|. This is the same as
  // GetForProfile, but will force creation of the session service even if
  // ShutdownForProfile has been called for |profile|.
  static SessionService* GetForProfileForSessionRestore(Profile* profile);

  // If |profile| has a session service, it is shut down. To properly record the
  // current state this forces creation of the session service, then shuts it
  // down.
  static void ShutdownForProfile(Profile* profile);

#if defined(UNIT_TEST)
  // For test use: force setting of the session service for a given profile.
  // This will delete a previous session service for this profile if it exists.
  static void SetForTestProfile(Profile* profile,
                                std::unique_ptr<SessionService> service) {
    GetInstance()->BrowserContextShutdown(profile);
    GetInstance()->BrowserContextDestroyed(profile);
    GetInstance()->Associate(profile, std::move(service));
  }
#endif

  static SessionServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<SessionServiceFactory>;
  FRIEND_TEST_ALL_PREFIXES(SessionCrashedInfoBarDelegateUnitTest,
                           DetachingTabWithCrashedInfoBar);

  SessionServiceFactory();
  ~SessionServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_SERVICE_FACTORY_H_
