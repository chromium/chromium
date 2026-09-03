// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_verification_tokens/private_verification_tokens_service_factory.h"

#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/private_verification_tokens/private_verification_tokens_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "net/base/features.h"

namespace {

scoped_refptr<
    const private_verification_tokens::PrivateVerificationTokensIssuerConfig>
MergeCommandLineCustomIssuerConfig(
    scoped_refptr<const private_verification_tokens::
                      PrivateVerificationTokensIssuerConfig> base_config) {
  std::string custom_issuer_json =
      net::features::kPrivateVerificationTokensCustomIssuer.Get();
  if (!custom_issuer_json.empty()) {
    auto parsed =
        base::JSONReader::ReadAndReturnValueWithError(custom_issuer_json, 0);
    if (!parsed.has_value()) {
      LOG(WARNING) << "Failed to parse command-line custom PVT issuer JSON: "
                   << parsed.error().message;
    } else if (!parsed->is_dict()) {
      LOG(WARNING) << "Failed to parse command-line custom PVT issuer JSON: "
                   << "Root value is not a dictionary.";
    } else {
      VLOG(1) << "Successfully parsed and will use custom PVT issuer config "
              << "from command line: " << custom_issuer_json;
      return private_verification_tokens::
          PrivateVerificationTokensIssuerConfig::CreateWithCustomIssuer(
              std::move(base_config), std::move(*parsed).TakeDict());
    }
  }
  return base_config;
}

scoped_refptr<
    const private_verification_tokens::PrivateVerificationTokensIssuerConfig>&
GetGlobalIssuerConfigStorage() {
  static base::NoDestructor<scoped_refptr<
      const private_verification_tokens::PrivateVerificationTokensIssuerConfig>>
      config(MergeCommandLineCustomIssuerConfig(nullptr));
  return *config;
}

}  // namespace

// static
void PrivateVerificationTokensServiceFactory::SetGlobalIssuerConfig(
    scoped_refptr<const private_verification_tokens::
                      PrivateVerificationTokensIssuerConfig> config) {
  GetGlobalIssuerConfigStorage() =
      MergeCommandLineCustomIssuerConfig(std::move(config));
}

// static
scoped_refptr<
    const private_verification_tokens::PrivateVerificationTokensIssuerConfig>
PrivateVerificationTokensServiceFactory::GetGlobalIssuerConfig() {
  return GetGlobalIssuerConfigStorage();
}

PrivateVerificationTokensService*
PrivateVerificationTokensServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<PrivateVerificationTokensService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
PrivateVerificationTokensService*
PrivateVerificationTokensServiceFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<PrivateVerificationTokensService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

PrivateVerificationTokensServiceFactory*
PrivateVerificationTokensServiceFactory::GetInstance() {
  static base::NoDestructor<PrivateVerificationTokensServiceFactory> instance;
  return instance.get();
}

ProfileSelections
PrivateVerificationTokensServiceFactory::CreateProfileSelections() {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kRedirectedToOriginal)
      .WithGuest(ProfileSelection::kNone)
      .WithSystem(ProfileSelection::kNone)
      .WithAshInternals(ProfileSelection::kNone)
      .Build();
}

std::unique_ptr<KeyedService>
PrivateVerificationTokensServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          net::features::kEnablePrivateVerificationTokens)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  CHECK(profile);
  auto service = PrivateVerificationTokensService::Create(
      profile->GetPath(),
      HostContentSettingsMapFactory::GetForProfile(profile));
  if (service) {
    if (auto config = GetGlobalIssuerConfig()) {
      service->SetIssuerConfig(std::move(config));
    }
  }
  return service;
}

bool PrivateVerificationTokensServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return false;
}

PrivateVerificationTokensServiceFactory::
    PrivateVerificationTokensServiceFactory()
    : ProfileKeyedServiceFactory("PrivateVerificationTokensServiceFactory",
                                 CreateProfileSelections()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

PrivateVerificationTokensServiceFactory::
    ~PrivateVerificationTokensServiceFactory() = default;
