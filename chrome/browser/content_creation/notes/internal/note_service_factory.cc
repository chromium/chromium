// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_creation/notes/internal/note_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "components/content_creation/notes/core/note_service.h"
#include "components/content_creation/notes/core/templates/template_store.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace content_creation {

namespace {
std::string GetCountryCode() {
  std::string country_code;
  auto* variations_service = g_browser_process->variations_service();
  if (!variations_service)
    return country_code;
  country_code = variations_service->GetStoredPermanentCountry();
  return country_code.empty() ? variations_service->GetLatestCountry()
                              : country_code;
}
}  // namespace

// static
NoteServiceFactory* NoteServiceFactory::GetInstance() {
  static base::NoDestructor<NoteServiceFactory> instance;
  return instance.get();
}

// static
NoteService* NoteServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<NoteService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

NoteServiceFactory::NoteServiceFactory()
    : ProfileKeyedServiceFactory(
          "NoteService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

NoteServiceFactory::~NoteServiceFactory() = default;

std::unique_ptr<KeyedService>
NoteServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  return std::make_unique<NoteService>(
      std::make_unique<TemplateStore>(profile->GetPrefs(), GetCountryCode()));
}

}  // namespace content_creation
