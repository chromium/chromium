// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/android_sms_app_manager_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/chromeos/android_sms/android_sms_app_setup_controller.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "extensions/common/extension.h"

namespace chromeos {

namespace android_sms {

namespace {

const PwaDomain kDomains[] = {PwaDomain::kProdAndroid, PwaDomain::kProdGoogle,
                              PwaDomain::kStaging};

const char kLastSuccessfulDomainPref[] = "android_sms.last_successful_domain";

}  // namespace

// static
void AndroidSmsAppManagerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kLastSuccessfulDomainPref, std::string());
}

AndroidSmsAppManagerImpl::PwaDelegate::PwaDelegate() = default;

AndroidSmsAppManagerImpl::PwaDelegate::~PwaDelegate() = default;

content::WebContents* AndroidSmsAppManagerImpl::PwaDelegate::OpenApp(
    Profile* profile,
    const apps::AppLaunchParams& params) {
  return apps::LaunchService::Get(profile)->OpenApplication(params);
}

bool AndroidSmsAppManagerImpl::PwaDelegate::TransferItemAttributes(
    const std::string& from_app_id,
    const std::string& to_app_id,
    app_list::AppListSyncableService* app_list_syncable_service) {
  return app_list_syncable_service->TransferItemAttributes(from_app_id,
                                                           to_app_id);
}

AndroidSmsAppManagerImpl::AndroidSmsAppManagerImpl(
    Profile* profile,
    AndroidSmsAppSetupController* setup_controller,
    PrefService* pref_service,
    app_list::AppListSyncableService* app_list_syncable_service,
    scoped_refptr<base::TaskRunner> task_runner)
    : profile_(profile),
      setup_controller_(setup_controller),
      app_list_syncable_service_(app_list_syncable_service),
      pref_service_(pref_service),
      installed_url_at_last_notify_(GetCurrentAppUrl()),
      pwa_delegate_(std::make_unique<PwaDelegate>()) {
  // Post a task to complete initialization. This portion of the flow must be
  // posted asynchronously because it accesses the networking stack, which is
  // not completely loaded until after this class is instantiated.
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&AndroidSmsAppManagerImpl::CompleteAsyncInitialization,
                     weak_ptr_factory_.GetWeakPtr()));
}

AndroidSmsAppManagerImpl::~AndroidSmsAppManagerImpl() = default;

base::Optional<GURL> AndroidSmsAppManagerImpl::GetCurrentAppUrl() {
  base::Optional<PwaDomain> domain = GetInstalledPwaDomain();
  if (!domain)
    return base::nullopt;

  return GetAndroidMessagesURL(false /* use_install_url */, *domain);
}

void AndroidSmsAppManagerImpl::SetUpAndroidSmsApp() {
  // If setup is already in progress, there is nothing else to do.
  if (is_new_app_setup_in_progress_)
    return;

  base::Optional<PwaDomain> migrating_from = GetInstalledPwaDomain();

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
  pref_service_->SetString(kLastSuccessfulDomainPref, std::string());

  base::Optional<GURL> installed_app_url = GetCurrentAppUrl();
  if (!installed_app_url)
    return;

  setup_controller_->DeleteRememberDeviceByDefaultCookie(*installed_app_url,
                                                         base::DoNothing());
}

bool AndroidSmsAppManagerImpl::HasAppBeenManuallyUninstalledByUser() {
  GURL url = GetAndroidMessagesURL(true /* use_install_url */);
  return pref_service_->GetString(kLastSuccessfulDomainPref) == url.spec() &&
         !setup_controller_->GetPwa(url);
}

base::Optional<PwaDomain> AndroidSmsAppManagerImpl::GetInstalledPwaDomain() {
  for (auto* it = std::begin(kDomains); it != std::end(kDomains); ++it) {
    if (setup_controller_->GetPwa(
            GetAndroidMessagesURL(true /* use_install_url */, *it))) {
      return *it;
    }
  }

  return base::nullopt;
}

void AndroidSmsAppManagerImpl::CompleteAsyncInitialization() {
  base::Optional<PwaDomain> domain = GetInstalledPwaDomain();

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
  base::Optional<GURL> installed_app_url = GetCurrentAppUrl();
  if (installed_url_at_last_notify_ == installed_app_url)
    return;

  installed_url_at_last_notify_ = installed_app_url;
  NotifyInstalledAppUrlChanged();
}

void AndroidSmsAppManagerImpl::OnSetUpNewAppResult(
    const base::Optional<PwaDomain>& migrating_from,
    const GURL& install_url,
    bool success) {
  is_new_app_setup_in_progress_ = false;

  const extensions::Extension* new_pwa = setup_controller_->GetPwa(
      GetAndroidMessagesURL(true /* use_install_url */));

  // If the app failed to install or the PWA does not exist, do not launch.
  if (!success || !new_pwa) {
    is_app_launch_pending_ = false;
    return;
  }

  if (success)
    pref_service_->SetString(kLastSuccessfulDomainPref, install_url.spec());

  // If there is no PWA installed at the old URL, no migration is needed and
  // setup is finished.
  if (!migrating_from) {
    HandleAppSetupFinished();
    return;
  }

  const extensions::Extension* old_pwa = setup_controller_->GetPwa(
      GetAndroidMessagesURL(true /* use_install_url */, *migrating_from));

  // Transfer attributes from the old PWA to the new one. This ensures that the
  // PWA's placement in the app launcher and shelf remains constant..
  bool transfer_attributes_success = pwa_delegate_->TransferItemAttributes(
      old_pwa->id() /* from_app_id */, new_pwa->id() /* to_app_id */,
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
    const base::Optional<PwaDomain>& migrating_from,
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
  base::Optional<PwaDomain> domain = GetInstalledPwaDomain();
  if (!domain)
    return;

  // Otherwise, launch the app.
  PA_LOG(VERBOSE) << "AndroidSmsAppManagerImpl::HandleAppSetupFinished(): "
                  << "Launching Messages PWA.";
  pwa_delegate_->OpenApp(
      profile_, apps::AppLaunchParams(
                    setup_controller_
                        ->GetPwa(GetAndroidMessagesURL(
                            true /* use_install_url */, *domain))
                        ->id(),
                    apps::mojom::LaunchContainer::kLaunchContainerWindow,
                    WindowOpenDisposition::NEW_WINDOW,
                    apps::mojom::AppLaunchSource::kSourceChromeInternal));
}

void AndroidSmsAppManagerImpl::SetPwaDelegateForTesting(
    std::unique_ptr<PwaDelegate> test_pwa_delegate) {
  pwa_delegate_ = std::move(test_pwa_delegate);
}

}  // namespace android_sms

}  // namespace chromeos
