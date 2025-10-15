// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/one_time_token_service_factory.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/one_time_tokens/core/browser/one_time_token_service_impl.h"
#include "components/one_time_tokens/core/browser/sms_otp_backend.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/autofill/android/android_sms_otp_backend_factory.h"
#endif  // BUILDFLAG(IS_ANDROID)

using ::one_time_tokens::OneTimeTokenService;
using ::one_time_tokens::OneTimeTokenServiceImpl;
using ::one_time_tokens::SmsOtpBackend;

namespace autofill {

OneTimeTokenServiceFactory::OneTimeTokenServiceFactory()
    : ProfileKeyedServiceFactory(
          "OneTimeTokenServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
#if BUILDFLAG(IS_ANDROID)
  DependsOn(AndroidSmsOtpBackendFactory::GetInstance());
#endif  // BUILDFLAG(IS_ANDROID)
}
OneTimeTokenServiceFactory::~OneTimeTokenServiceFactory() = default;

OneTimeTokenServiceFactory* OneTimeTokenServiceFactory::GetInstance() {
  static base::NoDestructor<OneTimeTokenServiceFactory> instance;
  return instance.get();
}

OneTimeTokenService* OneTimeTokenServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<OneTimeTokenServiceImpl*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

std::unique_ptr<KeyedService>
OneTimeTokenServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  SmsOtpBackend* backend = nullptr;
#if BUILDFLAG(IS_ANDROID)
  backend = AndroidSmsOtpBackendFactory::GetForBrowserContext(context);
#endif  // BUILDFLAG(IS_ANDROID)
  return std::make_unique<OneTimeTokenServiceImpl>(backend);
}

}  // namespace autofill
