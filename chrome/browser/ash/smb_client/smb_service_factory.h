// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "content/public/browser/browser_context.h"

namespace ash::smb_client {

class SmbService;

class SmbServiceFactory : public ProfileKeyedServiceFactory,
                          public session_manager::SessionManagerObserver {
 public:
  // Returns a service instance singleton, after creating it (if necessary).
  static SmbService* Get(content::BrowserContext* context);

  // Returns a service instance for the context if exists. Otherwise, returns
  // NULL.
  static SmbService* FindExisting(content::BrowserContext* context);

  // Gets a singleton instance of the factory.
  static SmbServiceFactory* GetInstance();

  // Returns whether Smb service is created for the given context.
  bool IsSmbServiceCreated(void* context);

  // Disallow copy and assignment.
  SmbServiceFactory(const SmbServiceFactory&) = delete;
  SmbServiceFactory& operator=(const SmbServiceFactory&) = delete;

  // session_manager::SessionManagerObserver:
  void OnUserSessionStartUpTaskCompleted() override;

  // Registers to SessionManagerObserver.
  void StartObservingSessionManager();

 private:
  friend base::NoDestructor<SmbServiceFactory>;

  SmbServiceFactory();
  ~SmbServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};
};

}  // namespace ash::smb_client

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SERVICE_FACTORY_H_
