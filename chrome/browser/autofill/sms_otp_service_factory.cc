// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/sms_otp_service_factory.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/autofill/content/browser/autofill_log_router_factory.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/one_time_token_log_handler.h"
#include "components/one_time_tokens/core/browser/sms_otp_backend.h"
#include "components/one_time_tokens/core/browser/sms_otp_service_impl.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/autofill/android/android_sms_otp_backend_factory.h"
#endif  // BUILDFLAG(IS_ANDROID)

using ::one_time_tokens::SmsOtpBackend;
using ::one_time_tokens::SmsOtpService;
using ::one_time_tokens::SmsOtpServiceImpl;

namespace autofill {

SmsOtpServiceFactory::SmsOtpServiceFactory()
    : ProfileKeyedServiceFactory("SmsOtpServiceFactory",
                                 ProfileSelections::BuildForRegularProfile()) {
#if BUILDFLAG(IS_ANDROID)
  DependsOn(AndroidSmsOtpBackendFactory::GetInstance());
#endif  // BUILDFLAG(IS_ANDROID)
  DependsOn(AutofillLogRouterFactory::GetInstance());
}
SmsOtpServiceFactory::~SmsOtpServiceFactory() = default;

SmsOtpServiceFactory* SmsOtpServiceFactory::GetInstance() {
  static base::NoDestructor<SmsOtpServiceFactory> instance;
  return instance.get();
}

SmsOtpService* SmsOtpServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SmsOtpService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

std::unique_ptr<KeyedService>
SmsOtpServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  SmsOtpBackend* sms_otp_backend = nullptr;
#if BUILDFLAG(IS_ANDROID)
  sms_otp_backend = AndroidSmsOtpBackendFactory::GetForBrowserContext(context);
#endif  // BUILDFLAG(IS_ANDROID)

  return std::make_unique<SmsOtpServiceImpl>(
      sms_otp_backend,
      CreateOneTimeTokenLogHandler(
          AutofillLogRouterFactory::GetForBrowserContext(context)));
}

}  // namespace autofill
