// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_policy_service_factory.h"

#include "base/check_is_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_policy_service.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_extension_system.h"
#include "chrome/browser/extensions/chrome_extension_system_factory.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registrar_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// static
SigninPolicyService* SigninPolicyServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SigninPolicyService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SigninPolicyServiceFactory* SigninPolicyServiceFactory::GetInstance() {
  static base::NoDestructor<SigninPolicyServiceFactory> instance;
  return instance.get();
}

SigninPolicyServiceFactory::SigninPolicyServiceFactory()
    : ProfileKeyedServiceFactory("SigninPolicyService") {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(extensions::ExtensionRegistrarFactory::GetInstance());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

SigninPolicyServiceFactory::~SigninPolicyServiceFactory() = default;

std::unique_ptr<KeyedService>
SigninPolicyServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!g_browser_process || !g_browser_process->profile_manager()) {
    CHECK_IS_TEST();
    return nullptr;
  }

  return std::make_unique<SigninPolicyService>(
      context->GetPath(),
      &g_browser_process->profile_manager()->GetProfileAttributesStorage()
#if BUILDFLAG(ENABLE_EXTENSIONS)
          ,
      extensions::ChromeExtensionSystemFactory::GetInstance()
          ->GetForBrowserContext(context),
      extensions::ExtensionRegistrarFactory::GetForBrowserContext(context)
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  );
}

bool SigninPolicyServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
