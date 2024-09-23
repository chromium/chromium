// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/android_sms/android_sms_app_setup_controller_impl.h"

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/url_util.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "url/gurl.h"

namespace ash {
namespace android_sms {

namespace {

const char kDefaultToPersistCookieName[] = "default_to_persist";
const char kMigrationCookieName[] = "cros_migrated_to";
const char kDefaultToPersistCookieValue[] = "true";

}  // namespace

// static
const base::TimeDelta AndroidSmsAppSetupControllerImpl::kInstallRetryDelay =
    base::Seconds(5);
const size_t AndroidSmsAppSetupControllerImpl::kMaxInstallRetryCount = 7u;

AndroidSmsAppSetupControllerImpl::PwaDelegate::PwaDelegate() = default;

AndroidSmsAppSetupControllerImpl::PwaDelegate::~PwaDelegate() = default;

std::optional<webapps::AppId>
AndroidSmsAppSetupControllerImpl::PwaDelegate::GetPwaForUrl(
    const GURL& install_url,
    Profile* profile) {
  return web_app::FindInstalledAppWithUrlInScope(profile, install_url);
}

network::mojom::CookieManager*
AndroidSmsAppSetupControllerImpl::PwaDelegate::GetCookieManager(
    Profile* profile) {
  return profile->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess();
}

void AndroidSmsAppSetupControllerImpl::PwaDelegate::RemovePwa(
    const webapps::AppId& app_id,
    Profile* profile,
    SuccessCallback callback) {
  // |provider| will be nullptr if Lacros web apps are enabled.
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  if (!provider) {
    std::move(callback).Run(false);
    return;
  }

  provider->scheduler().RemoveInstallManagementMaybeUninstall(
      app_id, web_app::WebAppManagement::kDefault,
      webapps::WebappUninstallSource::kInternalPreinstalled,
      base::BindOnce(
          [](SuccessCallback callback, webapps::UninstallResultCode code) {
            std::move(callback).Run(UninstallSucceeded(code));
          },
          std::move(callback)));
}

AndroidSmsAppSetupControllerImpl::AndroidSmsAppSetupControllerImpl(
    Profile* profile,
    web_app::ExternallyManagedAppManager* externally_managed_app_manager,
    HostContentSettingsMap* host_content_settings_map)
    : profile_(profile),
      externally_managed_app_manager_(externally_managed_app_manager),
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
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
  net::CanonicalCookie cookie = *net::CanonicalCookie::CreateSanitizedCookie(
      app_url, kDefaultToPersistCookieName, kDefaultToPersistCookieValue,
      std::string() /* domain */, std::string() /* path */,
      base::Time::Now() /* creation_time */, base::Time() /* expiration_time */,
      base::Time::Now() /* last_access_time */,
      !net::IsLocalhost(app_url) /* secure */, false /* http_only */,
      net::CookieSameSite::STRICT_MODE, net::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /* partition_key */, /*status=*/nullptr);
  // TODO(crbug.com/1069974): The cookie source url must be faked here because
  // otherwise, this would fail to set a secure cookie if |app_url| is insecure.
  // Consider instead to use url::Replacements to force the scheme to be https.
  pwa_delegate_->GetCookieManager(profile_)->SetCanonicalCookie(
      cookie, net::cookie_util::SimulatedCookieSource(cookie, "https"), options,
      base::BindOnce(&AndroidSmsAppSetupControllerImpl::
                         OnSetRememberDeviceByDefaultCookieResult,
                     weak_ptr_factory_.GetWeakPtr(), app_url, install_url,
                     std::move(callback)));
}

std::optional<webapps::AppId> AndroidSmsAppSetupControllerImpl::GetPwa(
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
  pwa_delegate_->GetCookieManager(profile_)->DeleteCookies(
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
  std::optional<webapps::AppId> app_id =
      pwa_delegate_->GetPwaForUrl(install_url, profile_);

  // If there is no app installed at |url|, there is nothing more to do.
  if (!app_id) {
    PA_LOG(VERBOSE) << "AndroidSmsAppSetupControllerImpl::RemoveApp(): No app "
                    << "is installed at " << install_url
                    << "; skipping removal process.";
    std::move(callback).Run(true /* success */);
    return;
  }

  PA_LOG(INFO) << "AndroidSmsAppSetupControllerImpl::RemoveApp(): "
               << "Uninstalling app at " << install_url << ".";

  pwa_delegate_->RemovePwa(
      *app_id, profile_,
      base::BindOnce(&AndroidSmsAppSetupControllerImpl::OnAppRemoved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     app_url, install_url, migrated_to_app_url));
}

void AndroidSmsAppSetupControllerImpl::OnAppRemoved(
    SuccessCallback callback,
    const GURL& app_url,
    const GURL& install_url,
    const GURL& migrated_to_app_url,
    bool uninstalled) {
  UMA_HISTOGRAM_BOOLEAN("AndroidSms.PWAUninstallationResult", uninstalled);

  if (!uninstalled) {
    PA_LOG(ERROR) << "AndroidSmsAppSetupControllerImpl::RemoveApp(): "
                  << "PWA for " << install_url << " failed to uninstall.";
    std::move(callback).Run(false /* success */);
    return;
  }

  SetMigrationCookie(app_url, migrated_to_app_url, std::move(callback));
}

void AndroidSmsAppSetupControllerImpl::OnSetRememberDeviceByDefaultCookieResult(
    const GURL& app_url,
    const GURL& install_url,
    SuccessCallback callback,
    net::CookieAccessResult result) {
  if (!result.status.IsInclude()) {
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
  pwa_delegate_->GetCookieManager(profile_)->DeleteCookies(
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
      install_url, web_app::mojom::UserDisplayMode::kStandalone,
      web_app::ExternalInstallSource::kInternalDefault);
  options.override_previous_user_uninstall = true;
  options.require_manifest = true;
  externally_managed_app_manager_->Install(
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
    web_app::ExternallyManagedAppManager::InstallResult result) {
  UMA_HISTOGRAM_ENUMERATION("AndroidSms.PWAInstallationResult", result.code);
  const bool install_succeeded = webapps::IsSuccess(result.code);

  if (!install_succeeded && num_attempts_so_far < kMaxInstallRetryCount) {
    base::TimeDelta retry_delay =
        kInstallRetryDelay * (1 << num_attempts_so_far);
    PA_LOG(VERBOSE)
        << "AndroidSmsAppSetupControllerImpl::OnAppInstallResult(): "
        << "PWA for " << install_url << " failed to install."
        << "InstallResultCode: " << static_cast<int>(result.code)
        << " Retrying again in " << retry_delay;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
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
        << "InstallResultCode: " << static_cast<int>(result.code);
    std::move(callback).Run(false /* success */);
    return;
  }
  PA_LOG(INFO) << "AndroidSmsAppSetupControllerImpl::OnAppInstallResult(): "
               << "PWA for " << install_url << " was installed successfully.";

  UMA_HISTOGRAM_EXACT_LINEAR("AndroidSms.NumAttemptsForSuccessfulInstallation",
                             num_attempts_so_far + 1, kMaxInstallRetryCount);

  // Grant notification permission for the PWA.
  host_content_settings_map_->SetContentSettingDefaultScope(
      app_url, GURL() /* top_level_url */, ContentSettingsType::NOTIFICATIONS,
      ContentSetting::CONTENT_SETTING_ALLOW);

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
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
  net::CanonicalCookie cookie = *net::CanonicalCookie::CreateSanitizedCookie(
      app_url, kMigrationCookieName, migrated_to_app_url.GetContent(),
      std::string() /* domain */, std::string() /* path */,
      base::Time::Now() /* creation_time */, base::Time() /* expiration_time */,
      base::Time::Now() /* last_access_time */,
      !net::IsLocalhost(app_url) /* secure */, false /* http_only */,
      net::CookieSameSite::STRICT_MODE, net::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /* partition_key */, /*status=*/nullptr);
  // TODO(crbug.com/1069974): The cookie source url must be faked here because
  // otherwise, this would fail to set a secure cookie if |app_url| is insecure.
  // Consider instead to use url::Replacements to force the scheme to be https.
  pwa_delegate_->GetCookieManager(profile_)->SetCanonicalCookie(
      cookie, net::cookie_util::SimulatedCookieSource(cookie, "https"), options,
      base::BindOnce(
          &AndroidSmsAppSetupControllerImpl::OnSetMigrationCookieResult,
          weak_ptr_factory_.GetWeakPtr(), app_url, std::move(callback)));
}

void AndroidSmsAppSetupControllerImpl::OnSetMigrationCookieResult(
    const GURL& app_url,
    SuccessCallback callback,
    net::CookieAccessResult result) {
  if (!result.status.IsInclude()) {
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
}  // namespace ash
