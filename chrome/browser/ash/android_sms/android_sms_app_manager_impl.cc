// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/android_sms/android_sms_app_manager_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/android_sms/android_sms_app_setup_controller.h"
#include "chrome/browser/ash/android_sms/android_sms_urls.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"

namespace ash {
namespace android_sms {

namespace {

const PwaDomain kDomains[] = {PwaDomain::kProdAndroid, PwaDomain::kProdGoogle,
                              PwaDomain::kStaging};

}  // namespace

AndroidSmsAppManagerImpl::PwaDelegate::PwaDelegate() = default;

AndroidSmsAppManagerImpl::PwaDelegate::~PwaDelegate() = default;

void AndroidSmsAppManagerImpl::PwaDelegate::OpenApp(Profile* profile,
                                                    const std::string& app_id) {
  apps::AppServiceProxyFactory::GetForProfile(profile)->Launch(
      app_id,
      apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                          false /* preferred_containner */),
      apps::LaunchSource::kFromChromeInternal);
}

bool AndroidSmsAppManagerImpl::PwaDelegate::TransferItemAttributes(
    const std::string& from_app_id,
    const std::string& to_app_id,
    app_list::AppListSyncableService* app_list_syncable_service) {
  return app_list_syncable_service->TransferItemAttributes(from_app_id,
                                                           to_app_id);
}

bool AndroidSmsAppManagerImpl::PwaDelegate::IsAppRegistryReady(
    Profile* profile) {
  // |provider| will be nullptr if Lacros web apps are enabled.
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  if (!provider)
    return false;

  return provider->on_registry_ready().is_signaled();
}

void AndroidSmsAppManagerImpl::PwaDelegate::ExecuteOnAppRegistryReady(
    Profile* profile,
    base::OnceClosure task) {
  // |provider| will be nullptr if Lacros web apps are enabled.
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  if (!provider)
    return;

  provider->on_registry_ready().Post(FROM_HERE, std::move(task));
}

AndroidSmsAppManagerImpl::AndroidSmsAppManagerImpl(
    Profile* profile,
    AndroidSmsAppSetupController* setup_controller,
    PrefService* pref_service,
    app_list::AppListSyncableService* app_list_syncable_service,
    std::unique_ptr<PwaDelegate> test_pwa_delegate)
    : profile_(profile),
      setup_controller_(setup_controller),
      app_list_syncable_service_(app_list_syncable_service),
      pref_service_(pref_service) {
  pwa_delegate_ = test_pwa_delegate ? std::move(test_pwa_delegate)
                                    : std::make_unique<PwaDelegate>();
  // Post a task to complete initialization. This portion of the flow must be
  // posted asynchronously because it accesses the networking stack and apps
  // registry, which might not be loaded until later.
  pwa_delegate_->ExecuteOnAppRegistryReady(
      profile_,
      base::BindOnce(&AndroidSmsAppManagerImpl::CompleteAsyncInitialization,
                     weak_ptr_factory_.GetWeakPtr()));
}

AndroidSmsAppManagerImpl::~AndroidSmsAppManagerImpl() = default;

std::optional<GURL> AndroidSmsAppManagerImpl::GetCurrentAppUrl() {
  std::optional<PwaDomain> domain = GetInstalledPwaDomain();
  if (!domain)
    return std::nullopt;

  return GetAndroidMessagesURL(false /* use_install_url */, *domain);
}

void AndroidSmsAppManagerImpl::SetUpAndroidSmsApp() {
  // If setup is already in progress, there is nothing else to do.
  if (is_new_app_setup_in_progress_)
    return;

  std::optional<PwaDomain> migrating_from = GetInstalledPwaDomainForMigration();

  // If the preferred domain is already installed, no migration is happening at
  // all.
  if (migrating_from && *migrating_from == GetPreferredPwaDomain())
    migrating_from.reset();

  GURL install_url = GetAndroidMessagesURL(true /* use_install_url */);

  is_new_app_setup_in_progress_ = true;
  setup_controller_->SetUpApp(
      GetAndroidMessagesURL() /* app_url */, install_url,
      base::BindOnce(&AndroidSmsAppManagerImpl::OnSetUpNewAppResult,
                     weak_ptr_factory_.GetWeakPtr(), migrating_from,
                     install_url));
}

void AndroidSmsAppManagerImpl::SetUpAndLaunchAndroidSmsApp() {
  is_app_launch_pending_ = true;
  SetUpAndroidSmsApp();
}

void AndroidSmsAppManagerImpl::TearDownAndroidSmsApp() {
  std::optional<GURL> installed_app_url = GetCurrentAppUrl();
  if (!installed_app_url)
    return;

  setup_controller_->DeleteRememberDeviceByDefaultCookie(*installed_app_url,
                                                         base::DoNothing());
}

bool AndroidSmsAppManagerImpl::IsAppInstalled() {
  if (GetInstalledPwaDomain())
    return true;
  return false;
}

bool AndroidSmsAppManagerImpl::IsAppRegistryReady() {
  return pwa_delegate_->IsAppRegistryReady(profile_);
}

void AndroidSmsAppManagerImpl::ExecuteOnAppRegistryReady(
    base::OnceClosure task) {
  pwa_delegate_->ExecuteOnAppRegistryReady(profile_, std::move(task));
}

std::optional<PwaDomain> AndroidSmsAppManagerImpl::GetInstalledPwaDomain() {
  PwaDomain preferred_domain = GetPreferredPwaDomain();
  if (setup_controller_->GetPwa(GetAndroidMessagesURL(
          true /* use_install_url */, preferred_domain))) {
    return preferred_domain;
  }

  // If the preferred PWA app is not installed. Check all migration domains.
  return GetInstalledPwaDomainForMigration();
}

std::optional<PwaDomain>
AndroidSmsAppManagerImpl::GetInstalledPwaDomainForMigration() {
  for (auto* it = std::begin(kDomains); it != std::end(kDomains); ++it) {
    if (setup_controller_->GetPwa(
            GetAndroidMessagesURL(true /* use_install_url */, *it))) {
      return *it;
    }
  }

  return std::nullopt;
}

void AndroidSmsAppManagerImpl::CompleteAsyncInitialization() {
  // Must wait until the app registry is ready before querying the current url.
  last_installed_url_ = GetCurrentAppUrl();

  std::optional<PwaDomain> domain = GetInstalledPwaDomain();

  // If no app was installed before this object was created, there is nothing
  // else to initialize.
  if (!domain)
    return;

  if (GetPreferredPwaDomain() == *domain) {
    PA_LOG(INFO) << "AndroidSmsAppManagerImpl::CompleteAsyncInitialization(): "
                 << "Currently using: " << *domain;
    return;
  }

  PA_LOG(INFO) << "AndroidSmsAppManagerImpl::CompleteAsyncInitialization(): "
               << "Attempting app migration. From: " << *domain
               << ", to: " << GetPreferredPwaDomain();
  SetUpAndroidSmsApp();
}

void AndroidSmsAppManagerImpl::NotifyInstalledAppUrlChangedIfNecessary() {
  std::optional<GURL> installed_app_url = GetCurrentAppUrl();
  if (last_installed_url_ == installed_app_url)
    return;

  last_installed_url_ = installed_app_url;
  NotifyInstalledAppUrlChanged();
}

void AndroidSmsAppManagerImpl::OnSetUpNewAppResult(
    const std::optional<PwaDomain>& migrating_from,
    const GURL& install_url,
    bool success) {
  is_new_app_setup_in_progress_ = false;

  std::optional<webapps::AppId> new_pwa = setup_controller_->GetPwa(
      GetAndroidMessagesURL(true /* use_install_url */));

  // If the app failed to install or the PWA does not exist, do not launch.
  if (!success || !new_pwa) {
    is_app_launch_pending_ = false;
    return;
  }

  // If there is no PWA installed at the old URL, no migration is needed and
  // setup is finished.
  if (!migrating_from) {
    HandleAppSetupFinished();
    return;
  }

  std::optional<webapps::AppId> old_pwa = setup_controller_->GetPwa(
      GetAndroidMessagesURL(true /* use_install_url */, *migrating_from));

  // Transfer attributes from the old PWA to the new one. This ensures that the
  // PWA's placement in the app launcher and shelf remains constant..
  bool transfer_attributes_success = pwa_delegate_->TransferItemAttributes(
      *old_pwa /* from_app_id */, *new_pwa /* to_app_id */,
      app_list_syncable_service_);
  if (!transfer_attributes_success) {
    PA_LOG(ERROR) << "AndroidSmsAppManagerImpl::OnSetUpNewAppResult(): Failed "
                  << "to transfer item attributes. From: " << *migrating_from
                  << ", to: " << GetPreferredPwaDomain();
  }

  // Finish the migration by removing the old app now that it has been replaced.
  setup_controller_->RemoveApp(
      GetAndroidMessagesURL(false /* use_install_url */,
                            *migrating_from) /* app_url */,
      GetAndroidMessagesURL(true /* use_install_url */,
                            *migrating_from) /* install_url */,
      GetAndroidMessagesURL() /* migrated_to_app_url */,
      base::BindOnce(&AndroidSmsAppManagerImpl::OnRemoveOldAppResult,
                     weak_ptr_factory_.GetWeakPtr(), migrating_from));
}

void AndroidSmsAppManagerImpl::OnRemoveOldAppResult(
    const std::optional<PwaDomain>& migrating_from,
    bool success) {
  // If app removal fails, log an error but continue anyway, since clients
  // should still be notified of the URL change.
  if (!success) {
    PA_LOG(ERROR) << "AndroidSmsAppManagerImpl::OnRemoveOldAppResult(): Failed "
                  << "to remove PWA at old domain: " << *migrating_from;
  }

  HandleAppSetupFinished();
}

void AndroidSmsAppManagerImpl::HandleAppSetupFinished() {
  NotifyInstalledAppUrlChangedIfNecessary();

  // If no launch was requested, setup is complete.
  if (!is_app_launch_pending_)
    return;

  is_app_launch_pending_ = false;

  // If launch was requested but setup failed, there is no app to launch.
  std::optional<PwaDomain> domain = GetInstalledPwaDomain();
  if (!domain)
    return;

  // Otherwise, launch the app.
  PA_LOG(VERBOSE) << "AndroidSmsAppManagerImpl::HandleAppSetupFinished(): "
                  << "Launching Messages PWA.";
  pwa_delegate_->OpenApp(profile_,
                         *setup_controller_->GetPwa(GetAndroidMessagesURL(
                             true /* use_install_url */, *domain)));
}

}  // namespace android_sms
}  // namespace ash
