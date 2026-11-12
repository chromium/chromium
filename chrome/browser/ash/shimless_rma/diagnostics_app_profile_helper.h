// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SHIMLESS_RMA_DIAGNOSTICS_APP_PROFILE_HELPER_H_
#define CHROME_BROWSER_ASH_SHIMLESS_RMA_DIAGNOSTICS_APP_PROFILE_HELPER_H_

#include "ash/webui/shimless_rma/backend/shimless_rma_delegate.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/webapps/common/web_app_id.h"
#include "extensions/common/extension_id.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
class ServiceWorkerContext;
}  // namespace content

namespace webapps {
enum class UninstallResultCode;
enum class WebappUninstallSource;
}  // namespace webapps

namespace web_app {
struct InstallIsolatedWebAppCommandError;
struct InstallIsolatedWebAppCommandSuccess;
class IsolatedWebAppInstallSource;
class IsolatedWebAppUrlInfo;
class WebApp;
}  // namespace web_app

namespace ash::shimless_rma {

// Delegate to replace operations which are hard to mocked in unit tests.
class DiagnosticsAppProfileHelperDelegate {
 public:
  using InstallIsolatedWebAppCallback = base::OnceCallback<void(
      base::expected<web_app::InstallIsolatedWebAppCommandSuccess,
                     web_app::InstallIsolatedWebAppCommandError>)>;

  DiagnosticsAppProfileHelperDelegate();
  DiagnosticsAppProfileHelperDelegate(
      const DiagnosticsAppProfileHelperDelegate&) = delete;
  virtual ~DiagnosticsAppProfileHelperDelegate();

  virtual content::ServiceWorkerContext* GetServiceWorkerContextForExtensionId(
      const extensions::ExtensionId& extension_id,
      content::BrowserContext* browser_context);

  virtual void InstallIsolatedWebApp(
      content::BrowserContext* browser_context,
      const web_app::IsolatedWebAppUrlInfo& url_info,
      const web_app::IsolatedWebAppInstallSource& install_source,
      InstallIsolatedWebAppCallback callback);

  virtual void RemoveShimlessRmaKnownIwa(
      content::BrowserContext* browser_context,
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      base::OnceCallback<void(webapps::UninstallResultCode)> callback);

  virtual const web_app::WebApp* GetWebAppByIdUnsafe(
      const webapps::AppId& app_id,
      content::BrowserContext* browser_context);

  static const std::optional<url::Origin>& GetInstalledDiagnosticsAppOrigin();
};

// Implements ShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContext.
void PrepareDiagnosticsAppProfile(
    DiagnosticsAppProfileHelperDelegate* delegate,
    const base::FilePath& crx_path,
    const base::FilePath& swbn_path,
    ShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContextCallback callback);

}  // namespace ash::shimless_rma

#endif  // CHROME_BROWSER_ASH_SHIMLESS_RMA_DIAGNOSTICS_APP_PROFILE_HELPER_H_
