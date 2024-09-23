// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extensions_browser_client.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/login/login_state/login_state.h"
#endif

namespace {

// Only main profile should be allowed to access the API.
bool IsContextForMainProfile(content::BrowserContext* context) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!Profile::FromBrowserContext(context)->IsMainProfile()) {
    return false;
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string user_hash =
      extensions::ExtensionsBrowserClient::Get()->GetUserIdHashFromContext(
          context);
  if (!ash::LoginState::IsInitialized() ||
      user_hash != ash::LoginState::Get()->primary_user_hash()) {
    return false;
  }
#endif

  return true;
}

}  // namespace

namespace chromeos {

// static
VpnServiceInterface* VpnServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<VpnServiceInterface*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
VpnServiceFactory* VpnServiceFactory::GetInstance() {
  static base::NoDestructor<VpnServiceFactory> instance;
  return instance.get();
}

VpnServiceFactory::VpnServiceFactory()
    : ProfileKeyedServiceFactory(
          "VpnService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(extensions::EventRouterFactory::GetInstance());
}

VpnServiceFactory::~VpnServiceFactory() = default;

bool VpnServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool VpnServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

KeyedService* VpnServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!VpnService::GetVpnService() || !IsContextForMainProfile(context)) {
    return nullptr;
  }
  return new VpnService(context);
}

}  // namespace chromeos
