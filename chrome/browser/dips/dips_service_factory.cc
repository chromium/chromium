// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/dips/chrome_dips_delegate.h"
#include "chrome/browser/dips/dips_service_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

using PassKey = base::PassKey<DIPSServiceFactory>;

/* static */
DIPSServiceImpl* DIPSServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  auto* dips_service = static_cast<DIPSServiceImpl*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
  if (dips_service) {
    dips_service->MaybeNotifyCreated(PassKey());
  }
  return dips_service;
}

DIPSServiceFactory* DIPSServiceFactory::GetInstance() {
  static base::NoDestructor<DIPSServiceFactory> instance;
  return instance.get();
}

DIPSServiceFactory::DIPSServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "DIPSServiceImpl",
          BrowserContextDependencyManager::GetInstance()) {}

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
  return new DIPSServiceImpl(PassKey(), context);
}
