// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"

#include <string>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/ui/managed_ui.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/policy/policy_constants.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace {

std::unique_ptr<KeyedService> BuildManagedBookmarkService(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<bookmarks::ManagedBookmarkService>(
      profile->GetPrefs(),
      base::BindRepeating(
          &ManagedBookmarkServiceFactory::GetManagedBookmarksManager,
          base::Unretained(profile)));
}

}  // namespace

// static
bookmarks::ManagedBookmarkService* ManagedBookmarkServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<bookmarks::ManagedBookmarkService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ManagedBookmarkServiceFactory* ManagedBookmarkServiceFactory::GetInstance() {
  static base::NoDestructor<ManagedBookmarkServiceFactory> instance;
  return instance.get();
}

// static
BrowserContextKeyedServiceFactory::TestingFactory
ManagedBookmarkServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildManagedBookmarkService);
}

// static
std::string ManagedBookmarkServiceFactory::GetManagedBookmarksManager(
    Profile* profile) {
  policy::ProfilePolicyConnector* connector =
      profile->GetProfilePolicyConnector();
  if (connector->IsManaged() &&
      connector->IsProfilePolicy(policy::key::kManagedBookmarks)) {
    std::optional<std::string> account_manager =
        chrome::GetAccountManagerIdentity(profile);
    if (account_manager)
      return *account_manager;
  }
  return std::string();
}

ManagedBookmarkServiceFactory::ManagedBookmarkServiceFactory()
    : ProfileKeyedServiceFactory(
          "ManagedBookmarkService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // Use OTR profile for Guest session.
              // (Bookmarks can be enabled in Guest sessions under some
              // enterprise policies.)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // No service for system profile.
              .WithSystem(ProfileSelection::kNone)
              // ChromeOS creates various profiles (login, lock screen...) that
              // do not have/need access to bookmarks.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

ManagedBookmarkServiceFactory::~ManagedBookmarkServiceFactory() = default;

std::unique_ptr<KeyedService>
ManagedBookmarkServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildManagedBookmarkService(context);
}

bool ManagedBookmarkServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
