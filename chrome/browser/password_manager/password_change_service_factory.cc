// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_change_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/password_manager/core/browser/password_feature_manager_impl.h"
#include "content/public/browser/browser_context.h"

PasswordChangeServiceFactory::PasswordChangeServiceFactory()
    : ProfileKeyedServiceFactory("PasswordChangeServiceFactory",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(AffiliationServiceFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

PasswordChangeServiceFactory::~PasswordChangeServiceFactory() = default;

PasswordChangeServiceFactory* PasswordChangeServiceFactory::GetInstance() {
  static base::NoDestructor<PasswordChangeServiceFactory> instance;
  return instance.get();
}

ChromePasswordChangeService* PasswordChangeServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ChromePasswordChangeService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

std::unique_ptr<KeyedService>
PasswordChangeServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ChromePasswordChangeService>(
      AffiliationServiceFactory::GetForProfile(profile),
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
      std::make_unique<password_manager::PasswordFeatureManagerImpl>(
          profile->GetPrefs(), g_browser_process->local_state(),
          SyncServiceFactory::GetForProfile(profile)));
}
