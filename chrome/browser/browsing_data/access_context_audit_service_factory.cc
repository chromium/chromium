// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/access_context_audit_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/browsing_data/access_context_audit_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

AccessContextAuditServiceFactory::AccessContextAuditServiceFactory()
    : ProfileKeyedServiceFactory(
          "AccessContextAuditService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

AccessContextAuditServiceFactory*
AccessContextAuditServiceFactory::GetInstance() {
  static base::NoDestructor<AccessContextAuditServiceFactory> instance;
  return instance.get();
}

AccessContextAuditService* AccessContextAuditServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AccessContextAuditService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

KeyedService* AccessContextAuditServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto context_audit_service = std::make_unique<AccessContextAuditService>();
  context_audit_service->Init(context->GetPath());
  return context_audit_service.release();
}

bool AccessContextAuditServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
