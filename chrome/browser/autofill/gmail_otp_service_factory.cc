// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/gmail_otp_service_factory.h"

#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/autofill/gmail_otp_backend_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/one_time_tokens/core/browser/gmail_otp_service_impl.h"

namespace autofill {

// static
one_time_tokens::GmailOtpService* GmailOtpServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<one_time_tokens::GmailOtpService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GmailOtpServiceFactory* GmailOtpServiceFactory::GetInstance() {
  static base::NoDestructor<GmailOtpServiceFactory> instance;
  return instance.get();
}

GmailOtpServiceFactory::GmailOtpServiceFactory()
    : ProfileKeyedServiceFactory(
          "GmailOtpService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(AffiliationServiceFactory::GetInstance());
  DependsOn(GmailOtpBackendFactory::GetInstance());
}

GmailOtpServiceFactory::~GmailOtpServiceFactory() = default;

std::unique_ptr<KeyedService>
GmailOtpServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  one_time_tokens::GmailOtpBackend* backend =
      GmailOtpBackendFactory::GetForProfile(profile);
  affiliations::AffiliationService* affiliation_service =
      AffiliationServiceFactory::GetForProfile(profile);

  return backend && affiliation_service
             ? std::make_unique<one_time_tokens::GmailOtpServiceImpl>(
                   backend, affiliation_service, std::nullopt)
             : nullptr;
}

}  // namespace autofill
