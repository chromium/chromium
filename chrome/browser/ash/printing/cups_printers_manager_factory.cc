// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ash/printing/cups_printers_manager_proxy.h"
#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {

// static
CupsPrintersManagerFactory* CupsPrintersManagerFactory::GetInstance() {
  static base::NoDestructor<CupsPrintersManagerFactory> instance;
  return instance.get();
}

// static
CupsPrintersManager* CupsPrintersManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<CupsPrintersManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

CupsPrintersManagerFactory::CupsPrintersManagerFactory()
    : ProfileKeyedServiceFactory(
          "CupsPrintersManagerFactory",
          // In Guest Mode, only use the OffTheRecord profile.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              // We do not need an instance of CupsPrintersManager on the
              // lockscreen.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()),
      proxy_(CupsPrintersManagerProxy::Create()) {
  DependsOn(SyncedPrintersManagerFactory::GetInstance());
}

CupsPrintersManagerFactory::~CupsPrintersManagerFactory() = default;

CupsPrintersManagerProxy* CupsPrintersManagerFactory::GetProxy() {
  return proxy_.get();
}

std::unique_ptr<KeyedService>
CupsPrintersManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  // This condition still needs to be explicitly stated here despite having
  // ProfileKeyedService logic implemented because `IsGuestSession()` and
  // `IsRegularProfile()` are not yet mutually exclusive in ASH and Lacros.
  // TODO(crbug.com/40233408): remove this condition when `IsGuestSession() is
  // fixed.
  //
  // In Guest Mode, only use the OffTheRecord profile.
  if (profile->IsGuestSession() && !profile->IsOffTheRecord()) {
    return nullptr;
  }

  std::unique_ptr<CupsPrintersManager> manager =
      CupsPrintersManager::Create(profile);
  if (ProfileHelper::IsPrimaryProfile(profile)) {
    proxy_->SetManager(manager.get());
  }
  return manager;
}

void CupsPrintersManagerFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  CupsPrintersManager* manager = static_cast<CupsPrintersManager*>(
      GetServiceForBrowserContext(context, false));
  if (manager) {
    // Remove the manager from the proxy before the manager is deleted.
    proxy_->RemoveManager(manager);
  }
  BrowserContextKeyedServiceFactory::BrowserContextShutdown(context);
}

bool CupsPrintersManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool CupsPrintersManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ash
