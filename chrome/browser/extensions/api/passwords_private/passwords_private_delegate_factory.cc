// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"

#include "ash/constants/web_app_id_constants.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_impl.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/extensions/profile_util.h"
#include "chrome/browser/password_manager/factories/account_password_store_factory.h"
#include "chrome/browser/password_manager/factories/bulk_leak_check_service_factory.h"
#include "chrome/browser/password_manager/factories/password_sender_service_factory.h"
#include "chrome/browser/password_manager/factories/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
// TODO(crbug.com/528209923): //chrome/browser/ui cannot be added to deps
// because it would cause a circular dependency. Remove this nogncheck once
// MaybeShowProfileSwitchIPH is moved to //chrome/browser/ui/...
#include "chrome/browser/ui/browser_window.h"  // nogncheck
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_system_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#endif

namespace extensions {

using content::BrowserContext;

namespace {

void MaybeShowProfileSwitchIPH(Profile* profile) {
#if !BUILDFLAG(IS_CHROMEOS)
  if (extensions::profile_util::GetNumberOfProfiles() < 2) {
    return;
  }

  BrowserWindowInterface* launched_app =
      web_app::AppBrowserController::FindForWebApp(*profile,
                                                   ash::kPasswordManagerAppId);
  if (launched_app && web_app::AppBrowserController::From(launched_app)
                          ->HasProfileMenuButton()) {
    BrowserWindow::FromBrowser(launched_app)->MaybeShowProfileSwitchIPH();
  }
#endif
}

// ChromeDeviceAuthenticatorFactory::GetForProfile() is overloaded, so binding
// it directly makes the compiler confused.
std::unique_ptr<device_reauth::DeviceAuthenticator> GetDeviceAuthenticator(
    Profile* profile,
    const device_reauth::DeviceAuthParams& params) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  return ChromeDeviceAuthenticatorFactory::GetForProfile(profile, params);
#else
  // For other platforms, ChromeDeviceAuthenticatorFactory::GetForProfile() is
  // not implemented.
  return nullptr;
#endif
}

}  // namespace

PasswordsPrivateDelegateProxy::PasswordsPrivateDelegateProxy(
    BrowserContext* browser_context)
    : browser_context_(browser_context) {}

PasswordsPrivateDelegateProxy::PasswordsPrivateDelegateProxy(
    BrowserContext* browser_context,
    scoped_refptr<PasswordsPrivateDelegate> delegate)
    : browser_context_(browser_context) {
  weak_instance_ = delegate->AsWeakPtr();
}
PasswordsPrivateDelegateProxy::~PasswordsPrivateDelegateProxy() = default;

void PasswordsPrivateDelegateProxy::Shutdown() {
  browser_context_ = nullptr;
  weak_instance_ = nullptr;
}

scoped_refptr<PasswordsPrivateDelegate>
PasswordsPrivateDelegateProxy::GetOrCreateDelegate() {
  if (weak_instance_) {
    return scoped_refptr<PasswordsPrivateDelegate>(weak_instance_.get());
  }

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  scoped_refptr<PasswordsPrivateDelegate> delegate =
      base::MakeRefCounted<PasswordsPrivateDelegateImpl>(
          profile->GetPrefs(), IdentityManagerFactory::GetForProfile(profile),
          profile->GetDefaultStoragePartition()
              ->GetURLLoaderFactoryForBrowserProcess(),
          PasswordSenderServiceFactory::GetForProfile(profile),
          SyncServiceFactory::GetForProfile(profile),
          TrustSafetySentimentServiceFactory::GetForProfile(profile),
          AffiliationServiceFactory::GetForProfile(profile),
          ProfilePasswordStoreFactory::GetForProfile(
              profile, ServiceAccessType::EXPLICIT_ACCESS),
          AccountPasswordStoreFactory::GetForProfile(
              profile, ServiceAccessType::EXPLICIT_ACCESS),
          PasskeyModelFactory::GetInstance()->GetForProfile(profile),
          BulkLeakCheckServiceFactory::GetForProfile(profile),
          PasswordsPrivateEventRouterFactory::GetForProfile(profile),
          &web_app::WebAppProvider::GetForWebApps(profile)->install_manager(),
          EnclaveManagerFactory::GetForProfile(profile),
          base::BindRepeating(&GetDeviceAuthenticator, profile),
          base::BindRepeating(&MaybeShowProfileSwitchIPH, profile));
  weak_instance_ = delegate->AsWeakPtr();
  return delegate;
}

scoped_refptr<PasswordsPrivateDelegate>
PasswordsPrivateDelegateProxy::GetDelegate() {
  return scoped_refptr<PasswordsPrivateDelegate>(weak_instance_.get());
}

// static
scoped_refptr<PasswordsPrivateDelegate>
PasswordsPrivateDelegateFactory::GetForBrowserContext(
    BrowserContext* browser_context,
    bool create) {
  PasswordsPrivateDelegateProxy* proxy =
      static_cast<PasswordsPrivateDelegateProxy*>(
          GetInstance()->GetServiceForBrowserContext(browser_context, true));
  return create ? proxy->GetOrCreateDelegate() : proxy->GetDelegate();
}

// static
PasswordsPrivateDelegateFactory*
    PasswordsPrivateDelegateFactory::GetInstance() {
  static base::NoDestructor<PasswordsPrivateDelegateFactory> instance;
  return instance.get();
}

PasswordsPrivateDelegateFactory::PasswordsPrivateDelegateFactory()
    : ProfileKeyedServiceFactory(
          "PasswordsPrivateDelegate",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  // LINT.IfChange(Dependencies)
  DependsOn(BulkLeakCheckServiceFactory::GetInstance());
  DependsOn(ProfilePasswordStoreFactory::GetInstance());
  DependsOn(AccountPasswordStoreFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(PasswordsPrivateEventRouterFactory::GetInstance());
  DependsOn(AffiliationServiceFactory::GetInstance());
  DependsOn(PasswordSenderServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(TrustSafetySentimentServiceFactory::GetInstance());
  DependsOn(PasskeyModelFactory::GetInstance());
  DependsOn(EnclaveManagerFactory::GetInstance());
  DependsOn(web_app::WebAppProviderFactory::GetInstance());
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  DependsOn(ChromeDeviceAuthenticatorFactory::GetInstance());
#endif
  // LINT.ThenChange(//chrome/browser/extensions/api/passwords_private/passwords_private_delegate_impl.h:Dependencies)
}

PasswordsPrivateDelegateFactory::~PasswordsPrivateDelegateFactory() = default;

std::unique_ptr<KeyedService>
PasswordsPrivateDelegateFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  return std::make_unique<PasswordsPrivateDelegateProxy>(profile);
}

}  // namespace extensions
