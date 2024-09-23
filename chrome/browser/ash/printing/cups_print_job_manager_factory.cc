// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_print_job_manager_factory.h"

#include "chrome/browser/ash/printing/cups_print_job_manager.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {
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

CupsPrintJobManagerFactory::CupsPrintJobManagerFactory()
    : ProfileKeyedServiceFactory(
          "CupsPrintJobManagerFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(SyncedPrintersManagerFactory::GetInstance());
  DependsOn(CupsPrintersManagerFactory::GetInstance());
}

CupsPrintJobManagerFactory::~CupsPrintJobManagerFactory() = default;

KeyedService* CupsPrintJobManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return CupsPrintJobManager::CreateInstance(
      Profile::FromBrowserContext(context));
}

}  // namespace ash
