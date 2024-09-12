// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_ASH_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_ASH_H_

#include <iosfwd>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/app_service/app_install/app_install_almanac_endpoint.h"
#include "chrome/browser/apps/app_service/app_install/app_install_service.h"
#include "chrome/browser/apps/app_service/app_install/arc_app_installer.h"
#include "chrome/browser/apps/app_service/app_install/web_app_installer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/native_window_tracker.h"

static_assert(BUILDFLAG(IS_CHROMEOS_ASH));

namespace ash::app_install {
class AppInstallDialog;
}

namespace apps {

// These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
// Additions to this enum must be added to the corresponding enum XML in:
// tools/metrics/histograms/metadata/apps/enums.xml
enum class AppInstallResult {
  kUnknown = 0,
  kSuccess = 1,
  kAlmanacFetchFailed = 2,  // Connection error trying to contact server.
  kAppDataCorrupted = 3,    // Server response failed validity checks.
  kAppProviderNotAvailable = 4,
  kAppTypeNotSupported = 5,
  kInstallParametersInvalid = 6,
  kInstallDialogNotAccepted = 7,
  kAppTypeInstallFailed = 8,
  kUserTypeNotPermitted = 9,
  kBadAppRequest = 10,  // Server rejected request.
  kInstallUrlFallback = 11,
  kMaxValue = kInstallUrlFallback,
};

class AppInstallServiceAsh : public AppInstallService {
 public:
  static base::OnceCallback<void(PackageId)>& InstallAppCallbackForTesting();

  explicit AppInstallServiceAsh(Profile& profile);
  AppInstallServiceAsh(const AppInstallServiceAsh&) = delete;
  AppInstallServiceAsh& operator=(const AppInstallServiceAsh&) = delete;
  ~AppInstallServiceAsh() override;

  // AppInstallService:
  void InstallAppWithFallback(AppInstallSurface surface,
                              std::string serialized_package_id,
                              std::optional<WindowIdentifier> anchor_window,
                              base::OnceClosure callback) override;

  void InstallApp(AppInstallSurface surface,
                  PackageId package_id,
                  std::optional<gfx::NativeWindow> anchor_window,
                  base::OnceClosure callback) override;

  void InstallAppHeadless(
      AppInstallSurface surface,
      PackageId package_id,
      base::OnceCallback<void(bool success)> callback) override;

  void InstallAppHeadless(
      AppInstallSurface surface,
      AppInstallData data,
      base::OnceCallback<void(bool success)> callback) override;

 private:
  bool CanUserInstall() const;
  bool MaybeLaunchApp(const PackageId& package_id);
  void FetchAppInstallData(
      PackageId package_id,
      app_install_almanac_endpoint::GetAppInstallInfoCallback data_callback);

  void PerformInstallHeadless(AppInstallSurface surface,
                              PackageId expected_package_id,
                              base::OnceCallback<void(bool success)> callback,
                              base::expected<AppInstallData, QueryError> data);

  void ShowDialogAndInstall(
      AppInstallSurface surface,
      PackageId expected_package_id,
      std::optional<gfx::NativeWindow> anchor_window,
      std::unique_ptr<views::NativeWindowTracker> anchor_window_tracker,
      base::OnceCallback<void(AppInstallResult)> callback,
      base::expected<AppInstallData, QueryError> data);
  void InstallIfDialogAccepted(
      AppInstallSurface surface,
      AppInstallData data,
      base::WeakPtr<ash::app_install::AppInstallDialog> dialog,
      base::OnceCallback<void(AppInstallResult)> callback,
      bool dialog_accepted);
  void ProcessInstallResult(
      AppInstallSurface surface,
      AppInstallData data,
      base::WeakPtr<ash::app_install::AppInstallDialog> dialog,
      base::OnceCallback<void(AppInstallResult)> callback,
      bool install_success);

  // Installs the requested app from the given `data`. This method is
  // called by both headless and dialog-based flows, and assumes that all
  // relevant policy checks have already been completed.
  void PerformInstall(AppInstallSurface surface,
                      AppInstallData data,
                      base::OnceCallback<void(bool)> install_callback);

  void FetchAppInstallUrl(
      std::string serialized_package_id,
      base::OnceCallback<void(base::expected<GURL, QueryError>)> callback);
  void MaybeLaunchAppInstallUrl(
      base::OnceCallback<void(AppInstallResult)> callback,
      base::expected<GURL, QueryError> install_url);

  raw_ref<Profile> profile_;
  ArcAppInstaller arc_app_installer_;
  WebAppInstaller web_app_installer_;

  base::WeakPtrFactory<AppInstallServiceAsh> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_ASH_H_
