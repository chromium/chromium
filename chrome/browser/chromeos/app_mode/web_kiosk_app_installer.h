// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_KIOSK_APP_INSTALLER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_KIOSK_APP_INSTALLER_H_

#include <string>

#include "base/check_deref.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class Profile;

namespace web_app {
class WebAppProvider;
}  // namespace web_app

namespace chromeos {

class WebKioskAppInstaller {
 public:
  enum class InstallState {
    kInstalled = 0,
    kNotInstalled = 1,
    kPlaceholderInstalled = 2,
  };
  using InstallStateCallback =
      base::OnceCallback<void(InstallState,
                              const absl::optional<web_app::AppId>&)>;
  using InstallCallback =
      base::OnceCallback<void(const absl::optional<web_app::AppId>&)>;

  WebKioskAppInstaller(Profile& profile, const GURL& install_url);
  WebKioskAppInstaller(const WebKioskAppInstaller&) = delete;
  WebKioskAppInstaller& operator=(const WebKioskAppInstaller&) = delete;
  ~WebKioskAppInstaller();

  void GetInstallState(InstallStateCallback callback);
  void InstallApp(InstallCallback callback);

 private:
  web_app::WebAppProvider& web_app_provider() {
    return CHECK_DEREF(web_app::WebAppProvider::GetForWebApps(&profile_.get()));
  }

  void OnExternalInstallCompleted(
      InstallCallback callback,
      const GURL& app_url,
      web_app::ExternallyManagedAppManager::InstallResult result);

  const base::raw_ref<Profile> profile_;
  const GURL install_url_;

  base::WeakPtrFactory<WebKioskAppInstaller> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_KIOSK_APP_INSTALLER_H_
