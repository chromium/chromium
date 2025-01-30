// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_factory.h"

#include <memory>

#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/profiles/profile.h"

// static
MicrosoftAuthService* MicrosoftAuthServiceFactory::GetForProfile(
    Profile* profile) {
  if (!IsMicrosoftModuleEnabledForProfile(profile)) {
    return nullptr;
  }

  return static_cast<MicrosoftAuthService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
MicrosoftAuthServiceFactory* MicrosoftAuthServiceFactory::GetInstance() {
  static base::NoDestructor<MicrosoftAuthServiceFactory> instance;
  return instance.get();
}

MicrosoftAuthServiceFactory::MicrosoftAuthServiceFactory()
    : ProfileKeyedServiceFactory("MicrosoftAuthService",
                                 ProfileSelections::BuildForRegularProfile()) {}

MicrosoftAuthServiceFactory::~MicrosoftAuthServiceFactory() = default;

std::unique_ptr<KeyedService>
MicrosoftAuthServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<MicrosoftAuthService>();
}
