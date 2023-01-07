// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"

#include "base/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#endif

// static
std::unique_ptr<TestingProfile> IdentityTestEnvironmentProfileAdaptor::
    CreateProfileForIdentityTestEnvironment() {
  return CreateProfileForIdentityTestEnvironment(
      TestingProfile::TestingFactories());
}

// static
std::unique_ptr<TestingProfile>
IdentityTestEnvironmentProfileAdaptor::CreateProfileForIdentityTestEnvironment(
    const TestingProfile::TestingFactories& input_factories) {
  TestingProfile::Builder builder;

  for (auto& input_factory : input_factories) {
    builder.AddTestingFactory(input_factory.first, input_factory.second);
  }

  return CreateProfileForIdentityTestEnvironment(builder);
}

// static
std::unique_ptr<TestingProfile>
IdentityTestEnvironmentProfileAdaptor::CreateProfileForIdentityTestEnvironment(
    TestingProfile::Builder& builder,
    signin::AccountConsistencyMethod account_consistency) {
  for (auto& identity_factory :
       GetIdentityTestEnvironmentFactories(account_consistency)) {
    builder.AddTestingFactory(identity_factory.first, identity_factory.second);
  }

  return builder.Build();
}

// static
void IdentityTestEnvironmentProfileAdaptor::
    SetIdentityTestEnvironmentFactoriesOnBrowserContext(
        content::BrowserContext* context,
        signin::AccountConsistencyMethod account_consistency) {
  for (const auto& factory_pair :
       GetIdentityTestEnvironmentFactories(account_consistency)) {
    factory_pair.first->SetTestingFactory(context, factory_pair.second);
  }
}

// static
void IdentityTestEnvironmentProfileAdaptor::
    AppendIdentityTestEnvironmentFactories(
        TestingProfile::TestingFactories* factories_to_append_to) {
  TestingProfile::TestingFactories identity_factories =
      GetIdentityTestEnvironmentFactories();
  factories_to_append_to->insert(factories_to_append_to->end(),
                                 identity_factories.begin(),
                                 identity_factories.end());
}

// static
TestingProfile::TestingFactories
IdentityTestEnvironmentProfileAdaptor::GetIdentityTestEnvironmentFactories(
    signin::AccountConsistencyMethod account_consistency) {
  return {{IdentityManagerFactory::GetInstance(),
           base::BindRepeating(&BuildIdentityManagerForTests,
                               account_consistency)}};
}

// static
std::unique_ptr<KeyedService>
IdentityTestEnvironmentProfileAdaptor::BuildIdentityManagerForTests(
    signin::AccountConsistencyMethod account_consistency,
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return signin::IdentityTestEnvironment::BuildIdentityManagerForTests(
      ChromeSigninClientFactory::GetForProfile(profile), profile->GetPrefs(),
      profile->GetPath(),
      g_browser_process->platform_part()->GetAccountManagerFactory(),
      GetAccountManagerFacade(profile->GetPath().value()));
#else
  return signin::IdentityTestEnvironment::BuildIdentityManagerForTests(
      ChromeSigninClientFactory::GetForProfile(profile), profile->GetPrefs(),
      profile->GetPath(), account_consistency);
#endif
}

IdentityTestEnvironmentProfileAdaptor::IdentityTestEnvironmentProfileAdaptor(
    Profile* profile)
    : identity_test_env_(IdentityManagerFactory::GetForProfile(profile),
                         ChromeSigninClientFactory::GetForProfile(profile)) {}
