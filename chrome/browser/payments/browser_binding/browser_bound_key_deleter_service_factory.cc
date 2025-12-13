// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/browser_binding/browser_bound_key_deleter_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/payments/browser_binding/browser_bound_key_deleter_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/payments/content/browser_binding/browser_bound_key_store.h"
#include "components/payments/content/browser_binding/passkey_browser_binder.h"
#include "components/payments/content/web_payments_web_data_service.h"
#include "components/webdata_services/web_data_service_wrapper_factory.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/payments/browser_binding/browser_bound_key_deleter_service_android.h"
#include "components/webauthn/core/browser/internal_authenticator.h"  // nogncheck
#else  // !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/payments/browser_binding/browser_bound_key_deleter_service_desktop.h"
#include "chrome/browser/webauthn/local_credential_management.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
#include "chrome/common/chrome_version.h"  // nogncheck
#endif                                     // BUILDFLAG(IS_MAC)

namespace {
#if BUILDFLAG(IS_MAC)
constexpr char kSecurePaymentConfirmationKeychainAccessGroup[] =
    MAC_TEAM_IDENTIFIER_STRING "." MAC_BUNDLE_IDENTIFIER_STRING
                               ".secure-payment-confirmation";
#endif  // BUILDFLAG(IS_MAC)
}  // namespace

namespace payments {

// static
BrowserBoundKeyDeleterService*
BrowserBoundKeyDeleterServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<BrowserBoundKeyDeleterService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

// static
BrowserBoundKeyDeleterServiceFactory*
BrowserBoundKeyDeleterServiceFactory::GetInstance() {
  static base::NoDestructor<BrowserBoundKeyDeleterServiceFactory> instance;
  return instance.get();
}

void BrowserBoundKeyDeleterServiceFactory::SetServiceForTesting(
    std::unique_ptr<BrowserBoundKeyDeleterService> service) {
  service_for_testing_ = std::move(service);
}

BrowserBoundKeyDeleterServiceFactory::BrowserBoundKeyDeleterServiceFactory()
    : ProfileKeyedServiceFactory(
          "BrowserBoundKeyDeleterService",
          // Browser bound key should not be deleted in off the record profiles
          // as they are not created in them either.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(webdata_services::WebDataServiceWrapperFactory::GetInstance());
}

BrowserBoundKeyDeleterServiceFactory::~BrowserBoundKeyDeleterServiceFactory() =
    default;

std::unique_ptr<KeyedService>
BrowserBoundKeyDeleterServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  std::unique_ptr<BrowserBoundKeyDeleterService> service;
  if (service_for_testing_) {
    service = std::move(service_for_testing_);
  } else {
#if BUILDFLAG(IS_ANDROID)
    service = std::make_unique<BrowserBoundKeyDeleterServiceAndroid>(
        webdata_services::WebDataServiceWrapperFactory::
            GetWebPaymentsWebDataServiceForBrowserContext(
                context, ServiceAccessType::EXPLICIT_ACCESS),
        GetBrowserBoundKeyStoreInstance());
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    service = std::make_unique<BrowserBoundKeyDeleterServiceDesktop>(
        webdata_services::WebDataServiceWrapperFactory::
            GetWebPaymentsWebDataServiceForBrowserContext(
                context, ServiceAccessType::EXPLICIT_ACCESS),
        GetBrowserBoundKeyStoreInstance(BrowserBoundKeyStore::Config{
#if BUILDFLAG(IS_MAC)
            .keychain_access_group =
                kSecurePaymentConfirmationKeychainAccessGroup
#endif  // BUILDFLAG(IS_MAC)
        }),
        LocalCredentialManagement::Create(
            Profile::FromBrowserContext(context)));
#else
    // BrowserBoundKeyDeleterService is only implemented on Android, Mac, and
    // Windows platforms.
    return nullptr;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)
  }

  // This triggers a cleanup of browser bound keys at startup (and the service
  // may be used later for explicit cleanup from delete browsing data).
  service->RemoveInvalidBBKs();

  return service;
}

bool BrowserBoundKeyDeleterServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace payments
