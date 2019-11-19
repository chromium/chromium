// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"

#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {
namespace {

static base::LazyInstance<CupsPrintJobManagerFactory>::DestructorAtExit
    g_cups_print_job_manager_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
CupsPrintJobManagerFactory* CupsPrintJobManagerFactory::GetInstance() {
  return g_cups_print_job_manager_factory.Pointer();
}

// static
CupsPrintJobManager* CupsPrintJobManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<CupsPrintJobManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

content::BrowserContext* CupsPrintJobManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

CupsPrintJobManagerFactory::CupsPrintJobManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "CupsPrintJobManagerFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(chromeos::SyncedPrintersManagerFactory::GetInstance());
  DependsOn(chromeos::CupsPrintersManagerFactory::GetInstance());
}

CupsPrintJobManagerFactory::~CupsPrintJobManagerFactory() = default;

KeyedService* CupsPrintJobManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return CupsPrintJobManager::CreateInstance(
      Profile::FromBrowserContext(context));
}

}  // namespace chromeos
