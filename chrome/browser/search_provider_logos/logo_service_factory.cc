// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_provider_logos/logo_service_factory.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/search_provider_logos/logo_service.h"
#include "components/search_provider_logos/logo_service_impl.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using search_provider_logos::LogoService;
using search_provider_logos::LogoServiceImpl;

namespace {

constexpr base::FilePath::CharType kCachedLogoDirectory[] =
    FILE_PATH_LITERAL("Search Logos");

bool UseGrayLogo() {
  return false;
}

}  // namespace

// static
LogoService* LogoServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<LogoService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
LogoServiceFactory* LogoServiceFactory::GetInstance() {
  static base::NoDestructor<LogoServiceFactory> instance;
  return instance.get();
}

LogoServiceFactory::LogoServiceFactory()
    : ProfileKeyedServiceFactory(
          "LogoService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

LogoServiceFactory::~LogoServiceFactory() = default;

std::unique_ptr<KeyedService>
LogoServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  DCHECK(!profile->IsOffTheRecord());
  return std::make_unique<LogoServiceImpl>(
      profile->GetPath().Append(kCachedLogoDirectory),
      IdentityManagerFactory::GetForProfile(profile),
      TemplateURLServiceFactory::GetForProfile(profile),
      std::make_unique<ImageDecoderImpl>(),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      base::BindRepeating(&UseGrayLogo));
}
