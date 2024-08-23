// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/signin/signin_metrics_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/signin_metrics_service.h"

SigninMetricsServiceFactory::SigninMetricsServiceFactory()
    : ProfileKeyedServiceFactory("SigninMetricsHelper") {
  DependsOn(IdentityManagerFactory::GetInstance());
}

SigninMetricsServiceFactory::~SigninMetricsServiceFactory() = default;

// static
SigninMetricsService* SigninMetricsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SigninMetricsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SigninMetricsServiceFactory* SigninMetricsServiceFactory::GetInstance() {
  static base::NoDestructor<SigninMetricsServiceFactory> instance;
  return instance.get();
}

KeyedService* SigninMetricsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new SigninMetricsService(
      *IdentityManagerFactory::GetForProfile(profile), *profile->GetPrefs(),
      g_browser_process->active_primary_accounts_metrics_recorder());
}

bool SigninMetricsServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

void SigninMetricsServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SigninMetricsService::RegisterProfilePrefs(registry);
}
