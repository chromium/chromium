// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tips/tips_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tips/core/tips_service.h"

namespace tips {

// static
TipsServiceFactory* TipsServiceFactory::GetInstance() {
  static base::NoDestructor<TipsServiceFactory> instance;
  return instance.get();
}

// static
TipsService* TipsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<TipsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

TipsServiceFactory::TipsServiceFactory()
    : ProfileKeyedServiceFactory(
          "TipsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

TipsServiceFactory::~TipsServiceFactory() = default;

std::unique_ptr<KeyedService>
TipsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<TipsService>();
}

}  // namespace tips
