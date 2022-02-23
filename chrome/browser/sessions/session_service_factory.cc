// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_data_service.h"
#include "chrome/browser/sessions/session_data_service_factory.h"
#include "chrome/browser/sessions/session_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace {

bool ShouldUseSessionServiceForProfile(const Profile& profile) {
  return profile.IsRegularProfile();
}

}  // namespace

// static
SessionService* SessionServiceFactory::GetForProfile(Profile* profile) {
  if (!ShouldUseSessionServiceForProfile(*profile))
    return nullptr;

  return static_cast<SessionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SessionService* SessionServiceFactory::GetForProfileIfExisting(
    Profile* profile) {
  return static_cast<SessionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
SessionService* SessionServiceFactory::GetForProfileForSessionRestore(
    Profile* profile) {
  SessionService* service = GetForProfile(profile);
  if (!service) {
    // If the service has been shutdown, remove the reference to nullptr for
    // |profile| so GetForProfile will recreate it.
    GetInstance()->Disassociate(profile);
    service = GetForProfile(profile);
  }
  return service;
}

// static
void SessionServiceFactory::ShutdownForProfile(Profile* profile) {
  if (!ShouldUseSessionServiceForProfile(*profile))
    return;

  if (SessionDataServiceFactory::GetForProfile(profile))
    SessionDataServiceFactory::GetForProfile(profile)->StartCleanup();

  // We're about to exit, force creation of the session service if it hasn't
  // been created yet. We do this to ensure session state matches the point in
  // time the user exited.
  SessionServiceFactory* factory = GetInstance();
  factory->GetServiceForBrowserContext(profile, true);

  // Shut down and remove the reference to the session service, and replace it
  // with an explicit nullptr to prevent it being recreated on the next access.
  factory->BrowserContextShutdown(profile);
  factory->BrowserContextDestroyed(profile);
  factory->Associate(profile, nullptr);
}

SessionServiceFactory* SessionServiceFactory::GetInstance() {
  return base::Singleton<SessionServiceFactory>::get();
}

SessionServiceFactory::SessionServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "SessionService",
        BrowserContextDependencyManager::GetInstance()) {
  // Ensure that session data is cleared before session restore can happen.
  DependsOn(SessionDataServiceFactory::GetInstance());
}

SessionServiceFactory::~SessionServiceFactory() = default;

KeyedService* SessionServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  if (!ShouldUseSessionServiceForProfile(*static_cast<Profile*>(profile)))
    return nullptr;

  SessionService* service = new SessionService(static_cast<Profile*>(profile));
  service->ResetFromCurrentBrowsers();
  return service;
}

bool SessionServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool SessionServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
