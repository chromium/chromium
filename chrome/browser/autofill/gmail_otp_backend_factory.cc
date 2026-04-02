// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/gmail_otp_backend_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
one_time_tokens::GmailOtpBackend* GmailOtpBackendFactory::GetForProfile(
    Profile* profile) {
  return static_cast<one_time_tokens::GmailOtpBackend*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
GmailOtpBackendFactory* GmailOtpBackendFactory::GetInstance() {
  static base::NoDestructor<GmailOtpBackendFactory> instance;
  return instance.get();
}

GmailOtpBackendFactory::GmailOtpBackendFactory()
    : ProfileKeyedServiceFactory("GmailOtpBackend",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

GmailOtpBackendFactory::~GmailOtpBackendFactory() = default;

std::unique_ptr<KeyedService>
GmailOtpBackendFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  // The `GmailOtpBackend` is only build for regular profiles for which the
  // IdentityManager is guaranteed to be non-null.
  CHECK(identity_manager != nullptr);
  return one_time_tokens::GmailOtpBackend::Create(
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      *identity_manager);
}
