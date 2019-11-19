// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/android_sms_app_setup_controller_impl.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "url/gurl.h"

namespace chromeos {

namespace android_sms {

namespace {

const char kDefaultToPersistCookieName[] = "default_to_persist";
const char kMigrationCookieName[] = "cros_migrated_to";
const char kDefaultToPersistCookieValue[] = "true";

}  // namespace

// static
const base::TimeDelta AndroidSmsAppSetupControllerImpl::kInstallRetryDelay =
    base::TimeDelta::FromSeconds(5);
const size_t AndroidSmsAppSetupControllerImpl::kMaxInstallRetryCount = 7u;

AndroidSmsAppSetupControllerImpl::PwaDelegate::PwaDelegate() = default;

AndroidSmsAppSetupControllerImpl::PwaDelegate::~PwaDelegate() = default;

const extensions::Extension*
AndroidSmsAppSetupControllerImpl::PwaDelegate::GetPwaForUrl(
    const GURL& install_url,
    Profile* profile) {
  return extensions::util::GetInstalledPwaForUrl(profile, install_url);
}

network::mojom::CookieManager*
AndroidSmsAppSetupControllerImpl::PwaDelegate::GetCookieManager(
    const GURL& app_url,
    Profile* profile) {
  return content::BrowserContext::GetStoragePartitionForSite(profile, app_url)
      ->GetCookieManagerForBrowserProcess();
}

bool AndroidSmsAppSetupControllerImpl::PwaDelegate::RemovePwa(
    const extensions::ExtensionId& extension_id,
    base::string16* error,
    Profile* profile) {
  return extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->UninstallExtension(
          extension_id,
          extensions::UNINSTALL_REASON_ORPHANED_EXTERNAL_EXTENSION, error);
}

AndroidSmsAppSetupControllerImpl::AndroidSmsAppSetupControllerImpl(
    Profile* profile,
    web_app::PendingAppManager* pending_app_manager,
    HostContentSettingsMap* host_content_settings_map)
    : profile_(profile),
      pending_app_manager_(pending_app_manager),
      host_content_settings_map_(host_content_settings_map),
      pwa_delegate_(std::make_unique<PwaDelegate>()) {}

AndroidSmsAppSetupControllerImpl::~AndroidSmsAppSetupControllerImpl() = default;

void AndroidSmsAppSetupControllerImpl::SetUpApp(const GURL& app_url,
                                                const GURL& install_url,
                                                SuccessCallback callback) {
  PA_LOG(VERBOSE) << "AndroidSmsAppSetupControllerImpl::SetUpApp(): Setting "
                  << "DefaultToPersist cookie at " << app_url << " before PWA "
                  << "installation.";
  net::CookieOptions options;
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);
  pwa_delegate_->GetCookieManager(app_url, profile_)
      ->SetCanonicalCookie(
          *net::CanonicalCookie::CreateSanitizedCookie(
              app_url, kDefaultToPersistCookieName,
              kDefaultToPersistCookieValue, std::string() /* domain */,
              std::string() /* path */, base::Time::Now() /* creation_time */,
              base::Time() /* expiration_time */,
              base::Time::Now() /* last_access_time */,
              !net::IsLocalhost(app_url) /* secure */, false /* http_only */,
              net::CookieSameSite::STRICT_MODE, net::COOKIE_PRIORITY_DEFAULT),
          "https", options,
          base::BindOnce(&AndroidSmsAppSetupControllerImpl::
                             OnSetRememberDeviceByDefaultCookieResult,
                         weak_ptr_factory_.GetWeakPtr(), app_url, install_url,
                         std::move(callback)));
}

const extensions::Extension* AndroidSmsAppSetupControllerImpl::GetPwa(
    const GURL& install_url) {
  return pwa_delegate_->GetPwaForUrl(install_url, profile_);
}

void AndroidSmsAppSetupControllerImpl::DeleteRememberDeviceByDefaultCookie(
    const GURL& app_url,
    SuccessCallback callback) {
  PA_LOG(INFO) << "AndroidSmsAppSetupControllerImpl::"
               << "DeleteRememberDeviceByDefaultCookie(): Deleting "
               << "DefaultToPersist cookie at " << app_url << ".";
  network::mojom::CookieDeletionFilterPtr filter(
      network::mojom::CookieDeletionFilter::New());
  filter->url = app_url;
  filter->cookie_name = kDefaultToPersistCookieName;
  pwa_delegate_->GetCookieManager(app_url, profile_)
      ->DeleteCookies(
          std::move(filter),
          base::BindOnce(&AndroidSmsAppSetupControllerImpl::
                             OnDeleteRememberDeviceByDefaultCookieResult,
                         weak_ptr_factory_.GetWeakPtr(), app_url,
                         std::move(callback)));
}

void AndroidSmsAppSetupControllerImpl::RemoveApp(
    const GURL& app_url,
    const GURL& install_url,
    const GURL& migrated_to_app_url,
    SuccessCallback callback) {
  const extensions::Extension* extension =
      pwa_delegate_->GetPwaForUrl(install_url, profile_);

  // If there is no app installed at |url|, there is nothing more to do.
  if (!extension) {
    PA_LOG(VERBOSE) << "AndroidSmsAppSetupControllerImpl::RemoveApp(): No app "
                    << "is installed at " << install_url
                    << "; skipping removal process.";
    std::move(callback).Run(true /* success */);
    return;
  }

  PA_LOG(INFO) << "AndroidSmsAppSetupControllerImpl::RemoveApp(): "
               << "Uninstalling app at " << install_url << ".";

  const extensions::ExtensionId& extension_id = extension->id();
  base::string16 error;
  bool uninstalled_successfully =
      pwa_delegate_->RemovePwa(extension_id, &error, profile_);
  UMA_HISTOGRAM_BOOLEAN("AndroidSms.PWAUninstallationResult",
                        uninstalled_successfully);

  if (!uninstalled_successfully) {
    PA_LOG(ERROR) << "AndroidSmsAppSetupControllerImpl::RemoveApp(): "
                  << "PWA for " << install_url << " failed to uninstall. "
                  << error;
    std::move(callback).Run(false /* success */);
    return;
  }

  SetMigrationCookie(app_url, migrated_to_app_url, std::move(callback));
}

void AndroidSmsAppSetupControllerImpl::OnSetRememberDeviceByDefaultCookieResult(
    const GURL& app_url,
    const GURL& install_url,
    SuccessCallback callback,
    net::CanonicalCookie::CookieInclusionStatus status) {
  if (!status.IsInclude()) {
    PA_LOG(WARNING)
        << "AndroidSmsAppSetupControllerImpl::"
        << "OnSetRememberDeviceByDefaultCookieResult(): Failed to set "
        << "DefaultToPersist cookie at " << app_url << ". Proceeding "
        << "to remove migration cookie.";
  }

  // Delete migration cookie in case it was set by a previous RemoveApp call.
  network::mojom::CookieDeletionFilterPtr filter(
      network::mojom::CookieDeletionFilter::New());
  filter->url = app_url;
  filter->cookie_name = kMigrationCookieName;
  pwa_delegate_->GetCookieManager(app_url, profile_)
      ->DeleteCookies(
          std::move(filter),
          base::BindOnce(
              &AndroidSmsAppSetupControllerImpl::OnDeleteMigrationCookieResult,
              weak_ptr_factory_.GetWeakPtr(), app_url, install_url,
              std::move(callback)));
}

void AndroidSmsAppSetupControllerImpl::OnDeleteMigrationCookieResult(
    const GURL& app_url,
    const GURL& install_url,
    SuccessCallback callback,
    uint32_t num_deleted) {
  // If the app is already installed at |url|, there is nothing more to do.
  if (pwa_delegate_->GetPwaForUrl(install_url, profile_)) {
    PA_LOG(VERBOSE) << "AndroidSmsAppSetupControllerImpl::"
                    << "OnDeleteMigrationCookieResult():"
                    << "App is already installed at " << install_url
                    << "; skipping setup process.";
    std::move(callback).Run(true /* success */);
    return;
  }

  TryInstallApp(install_url, app_url, 0 /* num_attempts_so_far */,
                std::move(callback));
}

void AndroidSmsAppSetupControllerImpl::TryInstallApp(const GURL& install_url,
                                                     const GURL& app_url,
                                                     size_t num_attempts_so_far,
                                                     SuccessCallback callback) {
  PA_LOG(VERBOSE) << "AndroidSmsAppSetupControllerImpl::TryInstallApp(): "
                  << "Trying to install PWA for " << install_url
                  << ". Num attempts so far # " << num_attempts_so_far;
  web_app::ExternalInstallOptions options(
      install_url, blink::mojom::DisplayMode::kStandalone,
      web_app::ExternalInstallSource::kInternalDefault);
  options.override_previous_user_uninstall = true;
  // The ServiceWorker does not load in time for the installability check, so
  // bypass it as a workaround.
  options.bypass_service_worker_check = true;
  options.require_manifest = true;
  pending_app_manager_->Install(
      std::move(options),
      base::BindOnce(&AndroidSmsAppSetupControllerImpl::OnAppInstallResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     num_attempts_so_far, app_url));
}

void AndroidSmsAppSetupControllerImpl::OnAppInstallResult(
    SuccessCallback callback,
    size_t num_attempts_so_far,
    const GURL& app_url,
    const GURL& install_url,
    web_app::InstallResultCode code) {
  UMA_HISTOGRAM_ENUMERATION("AndroidSms.PWAInstallationResult", code);
  const bool install_succeeded = web_app::IsSuccess(code);

  if (!install_succeeded && num_attempts_so_far < kMaxInstallRetryCount) {
    base::TimeDelta retry_delay =
        kInstallRetryDelay * (1 << num_attempts_so_far);
    PA_LOG(VERBOSE)
        << "AndroidSmsAppSetupControllerImpl::OnAppInstallResult(): "
        << "PWA for " << install_url << " failed to install."
        << "InstallResultCode: " << static_cast<int>(code)
        << " Retrying again in " << retry_delay;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AndroidSmsAppSetupControllerImpl::TryInstallApp,
                       weak_ptr_factory_.GetWeakPtr(), install_url, app_url,
                       num_attempts_so_far + 1, std::move(callback)),
        retry_delay);
    return;
  }
  UMA_HISTOGRAM_BOOLEAN("AndroidSms.EffectivePWAInstallationSuccess",
                        install_succeeded);

  if (!install_succeeded) {
    PA_LOG(WARNING)
        << "AndroidSmsAppSetupControllerImpl::OnAppInstallResult(): "
        << "PWA for " << install_url << " failed to install. "
        << "InstallResultCode: " << static_cast<int>(code);
    std::move(callback).Run(false /* success */);
    return;
  }
  PA_LOG(INFO) << "AndroidSmsAppSetupControllerImpl::OnAppInstallResult(): "
               << "PWA for " << install_url << " was installed successfully.";

  UMA_HISTOGRAM_EXACT_LINEAR("AndroidSms.NumAttemptsForSuccessfulInstallation",
                             num_attempts_so_far + 1, kMaxInstallRetryCount);

  // Grant notification permission for the PWA.
  host_content_settings_map_->SetWebsiteSettingDefaultScope(
      app_url, GURL() /* top_level_url */, ContentSettingsType::NOTIFICATIONS,
      content_settings::ResourceIdentifier(),
      std::make_unique<base::Value>(ContentSetting::CONTENT_SETTING_ALLOW));

  std::move(callback).Run(true /* success */);
}

void AndroidSmsAppSetupControllerImpl::SetMigrationCookie(
    const GURL& app_url,
    const GURL& migrated_to_app_url,
    SuccessCallback callback) {
  // Set migration cookie on the client for which the PWA was just uninstalled.
  // The client checks for this cookie to redirect users to the new domain. This
  // prevents unwanted connection stealing between old and new clients should
  // the user try to open old client.
  net::CookieOptions options;
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);
  pwa_delegate_->GetCookieManager(app_url, profile_)
      ->SetCanonicalCookie(
          *net::CanonicalCookie::CreateSanitizedCookie(
              app_url, kMigrationCookieName, migrated_to_app_url.GetContent(),
              std::string() /* domain */, std::string() /* path */,
              base::Time::Now() /* creation_time */,
              base::Time() /* expiration_time */,
              base::Time::Now() /* last_access_time */,
              !net::IsLocalhost(app_url) /* secure */, false /* http_only */,
              net::CookieSameSite::STRICT_MODE, net::COOKIE_PRIORITY_DEFAULT),
          "https", options,
          base::BindOnce(
              &AndroidSmsAppSetupControllerImpl::OnSetMigrationCookieResult,
              weak_ptr_factory_.GetWeakPtr(), app_url, std::move(callback)));
}

void AndroidSmsAppSetupControllerImpl::OnSetMigrationCookieResult(
    const GURL& app_url,
    SuccessCallback callback,
    net::CanonicalCookie::CookieInclusionStatus status) {
  if (!status.IsInclude()) {
    PA_LOG(ERROR)
        << "AndroidSmsAppSetupControllerImpl::OnSetMigrationCookieResult(): "
        << "Failed to set migration cookie for " << app_url << ". Proceeding "
        << "to remove DefaultToPersist cookie.";
  }

  DeleteRememberDeviceByDefaultCookie(app_url, std::move(callback));
}

void AndroidSmsAppSetupControllerImpl::
    OnDeleteRememberDeviceByDefaultCookieResult(const GURL& app_url,
                                                SuccessCallback callback,
                                                uint32_t num_deleted) {
  if (num_deleted != 1u) {
    PA_LOG(WARNING) << "AndroidSmsAppSetupControllerImpl::"
                    << "OnDeleteRememberDeviceByDefaultCookieResult(): "
                    << "Tried to delete a single cookie at " << app_url
                    << ", but " << num_deleted << " cookies were deleted.";
  }

  // Even if an unexpected number of cookies was deleted, consider this a
  // success. If SetUpApp() failed to install a cookie earlier, the setup
  // process is still considered a success, so failing to delete a cookie should
  // also be considered a success.
  std::move(callback).Run(true /* success */);
}

void AndroidSmsAppSetupControllerImpl::SetPwaDelegateForTesting(
    std::unique_ptr<PwaDelegate> test_pwa_delegate) {
  pwa_delegate_ = std::move(test_pwa_delegate);
}

}  // namespace android_sms

}  // namespace chromeos
