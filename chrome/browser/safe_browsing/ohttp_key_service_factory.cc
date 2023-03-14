// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/ohttp_key_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/ohttp_key_service.h"
#include "content/public/browser/browser_context.h"

namespace safe_browsing {

// static
OhttpKeyService* OhttpKeyServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<OhttpKeyService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
OhttpKeyServiceFactory* OhttpKeyServiceFactory::GetInstance() {
  return base::Singleton<OhttpKeyServiceFactory>::get();
}

OhttpKeyServiceFactory::OhttpKeyServiceFactory()
    : ProfileKeyedServiceFactory(
          "SafeBrowsingOhttpKeyService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

KeyedService* OhttpKeyServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new OhttpKeyService();
}

}  // namespace safe_browsing
