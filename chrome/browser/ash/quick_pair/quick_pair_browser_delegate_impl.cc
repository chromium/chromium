// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/quick_pair/quick_pair_browser_delegate_impl.h"

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/constants/ash_features.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/ash/services/quick_pair/public/mojom/quick_pair_service.mojom.h"
#include "components/cross_device/logging/logging.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/service_process_host.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {
namespace quick_pair {

QuickPairBrowserDelegateImpl::QuickPairBrowserDelegateImpl() {
  SetInstance(this);
}

QuickPairBrowserDelegateImpl::~QuickPairBrowserDelegateImpl() {
  SetInstance(nullptr);
}

scoped_refptr<network::SharedURLLoaderFactory>
QuickPairBrowserDelegateImpl::GetURLLoaderFactory() {
  Profile* profile = GetActiveProfile();
  if (!profile) {
    return nullptr;
  }

  return profile->GetURLLoaderFactory();
}

signin::IdentityManager* QuickPairBrowserDelegateImpl::GetIdentityManager() {
  Profile* profile = GetActiveProfile();
  if (!profile) {
    return nullptr;
  }

  return IdentityManagerFactory::GetForProfile(profile);
}

std::unique_ptr<image_fetcher::ImageFetcher>
QuickPairBrowserDelegateImpl::GetImageFetcher() {
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory =
      GetURLLoaderFactory();
  if (!shared_url_loader_factory) {
    CD_LOG(WARNING, Feature::FP)
        << "No URL loader factory to provide an image fetcher instance.";
    return nullptr;
  }

  return std::make_unique<image_fetcher::ImageFetcherImpl>(
      std::make_unique<ImageDecoderImpl>(), shared_url_loader_factory);
}

PrefService* QuickPairBrowserDelegateImpl::GetActivePrefService() {
  Profile* profile = GetActiveProfile();
  if (!profile) {
    return nullptr;
  }

  return profile->GetPrefs();
}

void QuickPairBrowserDelegateImpl::RequestService(
    mojo::PendingReceiver<mojom::QuickPairService> receiver) {
  content::ServiceProcessHost::Launch<mojom::QuickPairService>(
      std::move(receiver), content::ServiceProcessHost::Options()
                               .WithDisplayName("QuickPair Service")
                               .Pass());
}

bool QuickPairBrowserDelegateImpl::CompanionAppInstalled(
    const std::string& app_id) {
  Profile* profile = GetActiveProfile();
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  bool installed = false;
  proxy->AppRegistryCache().ForOneApp(
      app_id, [&installed](const apps::AppUpdate& update) {
        installed = apps_util::IsInstalled(update.Readiness());
      });
  return installed;
}

void QuickPairBrowserDelegateImpl::LaunchCompanionApp(
    const std::string& app_id) {
  Profile* profile = GetActiveProfile();
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  proxy->Launch(app_id, ui::EF_NONE, apps::LaunchSource::kFromChromeInternal);
}

void QuickPairBrowserDelegateImpl::OpenPlayStorePage(GURL play_store_uri) {
  Profile* profile = GetActiveProfile();
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  proxy->LaunchAppWithUrl(arc::kPlayStoreAppId, ui::EF_NONE, play_store_uri,
                          apps::LaunchSource::kFromChromeInternal);
}

Profile* QuickPairBrowserDelegateImpl::GetActiveProfile() {
  if (!user_manager::UserManager::Get()->IsUserLoggedIn())
    return nullptr;

  user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  DCHECK(active_user);

  return ProfileHelper::Get()->GetProfileByUser(active_user);
}

}  // namespace quick_pair
}  // namespace ash
