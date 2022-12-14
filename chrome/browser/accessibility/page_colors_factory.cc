// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/page_colors_factory.h"

#include "chrome/browser/accessibility/page_colors.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
PageColors* PageColorsFactory::GetForProfile(Profile* profile) {
  return static_cast<PageColors*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

void PageColorsFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  PageColors::RegisterProfilePrefs(registry);
}

// static
PageColorsFactory* PageColorsFactory::GetInstance() {
  static base::NoDestructor<PageColorsFactory> instance;
  return instance.get();
}

PageColorsFactory::PageColorsFactory()
    : BrowserContextKeyedServiceFactory(
          "PageColors",
          BrowserContextDependencyManager::GetInstance()) {}

PageColorsFactory::~PageColorsFactory() = default;

content::BrowserContext* PageColorsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (profile->IsSystemProfile() || profile->IsGuestSession())
    return nullptr;

  // The incognito profile shares the PageColors with it's original profile.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool PageColorsFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

std::unique_ptr<KeyedService>
PageColorsFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto page_colors = std::make_unique<PageColors>(
      Profile::FromBrowserContext(context)->GetPrefs());
  page_colors->Init();
  return page_colors;
}
