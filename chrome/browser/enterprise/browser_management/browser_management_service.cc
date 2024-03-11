// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/browser_management/browser_management_service.h"

#include "base/logging.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/browser_management/browser_management_status_provider.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/common/pref_names.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/image_fetcher/core/image_fetcher_types.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/prefs/pref_service.h"

namespace policy {

namespace {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
void UpdateEnterpriseLogo(
    Profile* profile,
    base::OnceCallback<void(const gfx::Image&,
                            const image_fetcher::RequestMetadata&)> callback) {
  auto badge_url = profile->GetPrefs()->GetString(prefs::kEnterpriseLogoUrl);
  if (badge_url.empty()) {
    std::move(callback).Run(gfx::Image(), image_fetcher::RequestMetadata());
    return;
  }
  constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
      net::DefineNetworkTrafficAnnotation("enterprise_logo_fetcher",
                                          R"(
        semantics {
          sender: "Chrome Profiles"
          description:
            "Retrieves an image set by the admin as the enterprise logo. This "
            "is used to show the user which organization manages their browser "
            "in the profile menu."
          trigger:
            "When the user launches the browser and the EnterpriseLogoUrl is "
            "set."
          data:
            "An admin-controlled URL for an image on the profile menu."
          destination: OTHER
          internal {
            contacts {
              email: "cbe-magic@google.com"
            }
          }
          user_data {
            type: SENSITIVE_URL
          }
          last_reviewed: "2024-02-26"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This fetch is enabled for any managed user with the "
            "EnterpriseLogoUrl policy set."
          chrome_policy {
            EnterpriseLogoUrl {
              EnterpriseLogoUrl: ""
            }
          }
        })");

  image_fetcher::ImageFetcher* fetcher =
      ImageFetcherServiceFactory::GetForKey(profile->GetProfileKey())
          ->GetImageFetcher(image_fetcher::ImageFetcherConfig::kDiskCacheOnly);
  fetcher->FetchImage(GURL(badge_url), std::move(callback),
                      image_fetcher::ImageFetcherParams(
                          kTrafficAnnotation,
                          /*uma_client_name=*/"BrowserManagementMetadata"));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

std::vector<std::unique_ptr<ManagementStatusProvider>>
GetManagementStatusProviders(Profile* profile) {
  std::vector<std::unique_ptr<ManagementStatusProvider>> providers;
  providers.emplace_back(
      std::make_unique<BrowserCloudManagementStatusProvider>());
  providers.emplace_back(
      std::make_unique<LocalBrowserManagementStatusProvider>());
  providers.emplace_back(
      std::make_unique<LocalDomainBrowserManagementStatusProvider>());
  providers.emplace_back(
      std::make_unique<ProfileCloudManagementStatusProvider>(profile));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  providers.emplace_back(std::make_unique<DeviceManagementStatusProvider>());
#endif
  return providers;
}

}  // namespace

BrowserManagementMetadata::BrowserManagementMetadata(Profile* profile) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  UpdateManagementLogo(profile);
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kEnterpriseLogoUrl,
      base::BindRepeating(&BrowserManagementMetadata::UpdateManagementLogo,
                          weak_ptr_factory_.GetWeakPtr(), profile));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

BrowserManagementMetadata::~BrowserManagementMetadata() = default;

const gfx::Image& BrowserManagementMetadata::GetManagementLogo() const {
  return management_logo_;
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
void BrowserManagementMetadata::UpdateManagementLogo(Profile* profile) {
  UpdateEnterpriseLogo(
      profile, base::BindOnce(&BrowserManagementMetadata::SetManagementLogo,
                              weak_ptr_factory_.GetWeakPtr()));
}

void BrowserManagementMetadata::SetManagementLogo(
    const gfx::Image& management_logo,
    const image_fetcher::RequestMetadata& metadata) {
  if (management_logo.IsEmpty()) {
    LOG(WARNING) << "EnterpriseLogoUrl fetch failed with error code "
                 << metadata.http_response_code << " and MIME type "
                 << metadata.mime_type;
  }
  management_logo_ = management_logo;
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

BrowserManagementService::BrowserManagementService(Profile* profile)
    : ManagementService(GetManagementStatusProviders(profile)),
      metadata_(profile) {}

BrowserManagementService::~BrowserManagementService() = default;

const BrowserManagementMetadata& BrowserManagementService::GetMetadata() {
  return metadata_;
}

}  // namespace policy
