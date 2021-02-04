// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_proxy.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {

// static
CupsPrintersManagerFactory* CupsPrintersManagerFactory::GetInstance() {
  return base::Singleton<CupsPrintersManagerFactory>::get();
}

// static
CupsPrintersManager* CupsPrintersManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<CupsPrintersManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

CupsPrintersManagerFactory::CupsPrintersManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "CupsPrintersManagerFactory",
          BrowserContextDependencyManager::GetInstance()),
      proxy_(CupsPrintersManagerProxy::Create()) {
  DependsOn(chromeos::SyncedPrintersManagerFactory::GetInstance());
}

CupsPrintersManagerFactory::~CupsPrintersManagerFactory() = default;

CupsPrintersManagerProxy* CupsPrintersManagerFactory::GetProxy() {
  return proxy_.get();
}

KeyedService* CupsPrintersManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // We do not need an instance of CupsPrintersManager on the lockscreen.
  auto* profile = Profile::FromBrowserContext(context);
  if (!ProfileHelper::IsRegularProfile(profile)) {
    return nullptr;
  }

  // In Guest Mode, only use the OffTheRecord profile.
  if (profile->IsGuestSession() && !profile->IsOffTheRecord()) {
    return nullptr;
  }

  auto manager = CupsPrintersManager::Create(profile);
  if (ProfileHelper::IsPrimaryProfile(profile)) {
    proxy_->SetManager(manager.get());
  }
  return manager.release();
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

content::BrowserContext* CupsPrintersManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

bool CupsPrintersManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool CupsPrintersManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace chromeos
