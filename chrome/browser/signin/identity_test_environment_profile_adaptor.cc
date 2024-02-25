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
    const TestingProfile::TestingFactories& input_factories) {
  TestingProfile::Builder builder;
  builder.AddTestingFactories(input_factories);
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
  for (const auto& f : GetIdentityTestEnvironmentFactories()) {
    CHECK_EQ(f.service_factory_and_testing_factory.index(), 0u);
    const auto& [service_factory, testing_factory] =
        absl::get<0>(f.service_factory_and_testing_factory);
    service_factory->SetTestingFactory(context, testing_factory);
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
IdentityTestEnvironmentProfileAdaptor::GetIdentityTestEnvironmentFactories() {
  return {{IdentityManagerFactory::GetInstance(),
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
