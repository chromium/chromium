// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_statistics_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_statistics.h"
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_MAC)
#include "device/fido/mac/credential_store.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "device/fido/cros/credential_store.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#endif

// static
ProfileStatistics* ProfileStatisticsFactory::GetForProfile(Profile* profile) {
  return static_cast<ProfileStatistics*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ProfileStatisticsFactory* ProfileStatisticsFactory::GetInstance() {
  static base::NoDestructor<ProfileStatisticsFactory> instance;
  return instance.get();
}

ProfileStatisticsFactory::ProfileStatisticsFactory()
    : ProfileKeyedServiceFactory(
          "ProfileStatistics",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(WebDataServiceFactory::GetInstance());
  DependsOn(autofill::PersonalDataManagerFactory::GetInstance());
  DependsOn(BookmarkModelFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(ProfilePasswordStoreFactory::GetInstance());
}

KeyedService* ProfileStatisticsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<::device::fido::PlatformCredentialStore> credential_store =
#if BUILDFLAG(IS_MAC)
      std::make_unique<::device::fido::mac::TouchIdCredentialStore>(
          ChromeWebAuthenticationDelegate::TouchIdAuthenticatorConfigForProfile(
              profile));
#elif BUILDFLAG(IS_CHROMEOS)
      std::make_unique<
          ::device::fido::cros::PlatformAuthenticatorCredentialStore>();
#else
      nullptr;
#endif

  return new ProfileStatistics(
      WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      autofill::PersonalDataManagerFactory::GetForBrowserContext(profile),
      BookmarkModelFactory::GetForBrowserContext(profile),
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      profile->GetPrefs(),
#if !BUILDFLAG(IS_ANDROID)
      UserAnnotationsServiceFactory::GetForProfile(profile),
#else
      /*user_annotations_service=*/nullptr,
#endif
      std::move(credential_store));
}
