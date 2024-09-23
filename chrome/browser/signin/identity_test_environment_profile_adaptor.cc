// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"

#include "base/functional/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

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
    TestingProfile::TestingFactories input_factories) {
  TestingProfile::Builder builder;
  builder.AddTestingFactories(std::move(input_factories));
  return CreateProfileForIdentityTestEnvironment(builder);
}

// static
std::unique_ptr<TestingProfile>
IdentityTestEnvironmentProfileAdaptor::CreateProfileForIdentityTestEnvironment(
    TestingProfile::Builder& builder) {
  builder.AddTestingFactories(GetIdentityTestEnvironmentFactories());
  return builder.Build();
}

// static
void IdentityTestEnvironmentProfileAdaptor::
    SetIdentityTestEnvironmentFactoriesOnBrowserContext(
        content::BrowserContext* context) {
  for (auto& f : GetIdentityTestEnvironmentFactories()) {
    absl::visit(
        [context](auto& p) {
          p.first->SetTestingFactory(context, std::move(p.second));
        },
        f.service_factory_and_testing_factory);
  }
}

// static
TestingProfile::TestingFactories IdentityTestEnvironmentProfileAdaptor::
    GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
        TestingProfile::TestingFactories testing_factories) {
  for (auto& factory : GetIdentityTestEnvironmentFactories()) {
    testing_factories.push_back(std::move(factory));
  }
  return testing_factories;
}

// static
TestingProfile::TestingFactories
IdentityTestEnvironmentProfileAdaptor::GetIdentityTestEnvironmentFactories() {
  return {TestingProfile::TestingFactory{
      IdentityManagerFactory::GetInstance(),
      base::BindRepeating(&BuildIdentityManagerForTests)}};
}

// static
std::unique_ptr<KeyedService>
IdentityTestEnvironmentProfileAdaptor::BuildIdentityManagerForTests(
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
      profile->GetPath());
#endif
}

IdentityTestEnvironmentProfileAdaptor::IdentityTestEnvironmentProfileAdaptor(
    Profile* profile)
    : identity_test_env_(IdentityManagerFactory::GetForProfile(profile),
                         ChromeSigninClientFactory::GetForProfile(profile)) {}
