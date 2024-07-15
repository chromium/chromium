// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_data_service.h"
#include "chrome/browser/sessions/session_data_service_factory.h"
#include "chrome/browser/sessions/session_service.h"

// static
SessionService* SessionServiceFactory::GetForProfile(Profile* profile) {
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
  // We're about to exit, force creation of the session service if it hasn't
  // been created yet. We do this to ensure session state matches the point in
  // time the user exited.
  SessionServiceFactory* factory = GetInstance();
  SessionService* service = factory->GetForProfile(profile);
  if (!service)
    return;

  if (SessionDataServiceFactory::GetForProfile(profile))
    SessionDataServiceFactory::GetForProfile(profile)->StartCleanup();

  // Shut down and remove the reference to the session service, and replace it
  // with an explicit nullptr to prevent it being recreated on the next access.
  factory->BrowserContextShutdown(profile);
  factory->BrowserContextDestroyed(profile);
  factory->Associate(profile, nullptr);
}

SessionServiceFactory* SessionServiceFactory::GetInstance() {
  static base::NoDestructor<SessionServiceFactory> instance;
  return instance.get();
}

SessionServiceFactory::SessionServiceFactory()
    : ProfileKeyedServiceFactory(
          "SessionService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  // Ensure that session data is cleared before session restore can happen.
  DependsOn(SessionDataServiceFactory::GetInstance());
}

SessionServiceFactory::~SessionServiceFactory() = default;

std::unique_ptr<KeyedService>
SessionServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  std::unique_ptr<SessionService> service =
      std::make_unique<SessionService>(static_cast<Profile*>(profile));
  service->ResetFromCurrentBrowsers();
  return service;
}

bool SessionServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool SessionServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
