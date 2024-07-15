// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_client_factory.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry_factory.h"
#endif

ChromeSigninClientFactory::ChromeSigninClientFactory()
    : ProfileKeyedServiceFactory(
          "ChromeSigninClient",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(ProfileNetworkContextServiceFactory::GetInstance());
  // Used to keep track of bookmark metrics on Signin/Sync.
  DependsOn(BookmarkModelFactory::GetInstance());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Used to keep track of extensions metrics on Signin/Sync.
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
#endif
}

ChromeSigninClientFactory::~ChromeSigninClientFactory() = default;

// static
SigninClient* ChromeSigninClientFactory::GetForProfile(Profile* profile) {
  return static_cast<SigninClient*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ChromeSigninClientFactory* ChromeSigninClientFactory::GetInstance() {
  static base::NoDestructor<ChromeSigninClientFactory> instance;
  return instance.get();
}

KeyedService* ChromeSigninClientFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ChromeSigninClient(Profile::FromBrowserContext(context));
}
