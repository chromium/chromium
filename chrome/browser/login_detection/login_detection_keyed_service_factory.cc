// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/login_detection/login_detection_keyed_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/login_detection/login_detection_keyed_service.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace login_detection {

// static
LoginDetectionKeyedService* LoginDetectionKeyedServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<LoginDetectionKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
LoginDetectionKeyedServiceFactory*
LoginDetectionKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<LoginDetectionKeyedServiceFactory> factory;
  return factory.get();
}

LoginDetectionKeyedServiceFactory::LoginDetectionKeyedServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "LoginDetectionKeyedService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(AccountPasswordStoreFactory::GetInstance());
  DependsOn(PasswordStoreFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

LoginDetectionKeyedServiceFactory::~LoginDetectionKeyedServiceFactory() =
    default;

content::BrowserContext*
LoginDetectionKeyedServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  if (context->IsOffTheRecord())
    return nullptr;

  if (!IsLoginDetectionFeatureEnabled())
    return nullptr;

  return context;
}

KeyedService* LoginDetectionKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new LoginDetectionKeyedService(Profile::FromBrowserContext(context));
}

bool LoginDetectionKeyedServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  // Required, since the service's constructor applies site isolation for saved
  // login sites, which needs to happen at profile initialization time, before
  // any navigations happen in it.
  return true;
}

}  // namespace login_detection
