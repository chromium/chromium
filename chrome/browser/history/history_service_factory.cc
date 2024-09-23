// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_service_factory.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/chrome_history_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/content/browser/content_visit_delegate.h"
#include "components/history/content/browser/history_database_helper.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/common/pref_names.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"

namespace {

std::unique_ptr<KeyedService> BuildHistoryService(
    content::BrowserContext* context) {
  auto history_service = std::make_unique<history::HistoryService>(
      std::make_unique<ChromeHistoryClient>(
          BookmarkModelFactory::GetForBrowserContext(context)),
      std::make_unique<history::ContentVisitDelegate>(context));
  if (!history_service->Init(history::HistoryDatabaseParamsForPath(
          context->GetPath(), chrome::GetChannel()))) {
    return nullptr;
  }
  return history_service;
}

}  // namespace

// static
history::HistoryService* HistoryServiceFactory::GetForProfile(
    Profile* profile,
    ServiceAccessType sat) {
  // If saving history is disabled, only allow explicit access.
  if (sat != ServiceAccessType::EXPLICIT_ACCESS &&
      profile->GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled)) {
    return nullptr;
  }

  return static_cast<history::HistoryService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
history::HistoryService* HistoryServiceFactory::GetForProfileIfExists(
    Profile* profile,
    ServiceAccessType sat) {
  // If saving history is disabled, only allow explicit access.
  if (sat != ServiceAccessType::EXPLICIT_ACCESS &&
      profile->GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled)) {
    return nullptr;
  }

  return static_cast<history::HistoryService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
history::HistoryService* HistoryServiceFactory::GetForProfileWithoutCreating(
    Profile* profile) {
  return static_cast<history::HistoryService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
HistoryServiceFactory* HistoryServiceFactory::GetInstance() {
  static base::NoDestructor<HistoryServiceFactory> instance;
  return instance.get();
}

// static
void HistoryServiceFactory::ShutdownForProfile(Profile* profile) {
  HistoryServiceFactory* factory = GetInstance();
  factory->BrowserContextShutdown(profile);
  factory->BrowserContextDestroyed(profile);
}

// static
BrowserContextKeyedServiceFactory::TestingFactory
HistoryServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildHistoryService);
}

HistoryServiceFactory::HistoryServiceFactory()
    : ProfileKeyedServiceFactory(
          "HistoryService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(BookmarkModelFactory::GetInstance());
}

HistoryServiceFactory::~HistoryServiceFactory() = default;

std::unique_ptr<KeyedService>
HistoryServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildHistoryService(context);
}

bool HistoryServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
