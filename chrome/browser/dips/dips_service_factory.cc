// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/dips/chrome_dips_delegate.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

/* static */
DIPSServiceImpl* DIPSServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DIPSServiceImpl*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

DIPSServiceFactory* DIPSServiceFactory::GetInstance() {
  static base::NoDestructor<DIPSServiceFactory> instance;
  return instance.get();
}

DIPSServiceFactory::DIPSServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "DIPSServiceImpl",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(CookieSettingsFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

DIPSServiceFactory::~DIPSServiceFactory() = default;

content::BrowserContext* DIPSServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kDIPS)) {
    return nullptr;
  }

  if (!ChromeDipsDelegate::Create()->ShouldEnableDips(context)) {
    return nullptr;
  }

  return context;
}

KeyedService* DIPSServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new DIPSServiceImpl(context);
}
