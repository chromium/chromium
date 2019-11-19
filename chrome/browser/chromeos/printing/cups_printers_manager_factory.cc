// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
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
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(chromeos::SyncedPrintersManagerFactory::GetInstance());
}

CupsPrintersManagerFactory::~CupsPrintersManagerFactory() = default;

KeyedService* CupsPrintersManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // We do not need an instance of CupsPrintersManager on the lockscreen.
  if (ProfileHelper::IsLockScreenAppProfile(
          Profile::FromBrowserContext(context)) ||
      ProfileHelper::IsSigninProfile(Profile::FromBrowserContext(context))) {
    return nullptr;
  }
  return CupsPrintersManager::Create(Profile::FromBrowserContext(context))
      .release();
}

content::BrowserContext* CupsPrintersManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool CupsPrintersManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool CupsPrintersManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace chromeos
