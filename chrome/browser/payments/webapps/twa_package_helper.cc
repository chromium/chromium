// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/webapps/twa_package_helper.h"

#include <utility>

#include "base/location.h"
#include "base/task/single_thread_task_runner.h"

#if BUILDFLAG(IS_CHROMEOS)
#include <optional>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

#if BUILDFLAG(IS_CHROMEOS)
// Returns `nullopt` if `rfh` is null, or is not a web app window, or if the
// current url is not within the scope of the web app.
std::optional<webapps::AppId> GetWebAppId(content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    return std::nullopt;
  }

  DCHECK(rfh);
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!web_app::AppBrowserController::IsWebApp(browser)) {
    return std::nullopt;
  }

  webapps::AppId app_id = browser->app_controller()->app_id();
  auto* web_app_provider =
      web_app::WebAppProvider::GetForWebApps(browser->profile());
  if (!web_app_provider ||
      !web_app_provider->registrar_unsafe().IsUrlInAppScope(
          web_contents->GetLastCommittedURL(), app_id)) {
    return std::nullopt;
  }

  return app_id;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Obtains the Android package name of the Trusted Web Activity that invoked
// this browser, if any.
std::string FetchTwaPackageName(content::RenderFrameHost* rfh) {
  std::optional<webapps::AppId> app_id = GetWebAppId(rfh);
  if (!app_id.has_value()) {
    return "";
  }

  auto* apk_web_app_service = ash::ApkWebAppService::Get(
      Profile::FromBrowserContext(rfh->GetBrowserContext()));
  if (!apk_web_app_service) {
    return "";
  }

  std::optional<std::string> twa_package_name =
      apk_web_app_service->GetPackageNameForWebApp(*app_id);

  return twa_package_name.has_value() ? twa_package_name.value() : "";
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

namespace payments {

TwaPackageHelper::TwaPackageHelper(
    content::RenderFrameHost* render_frame_host) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  twa_package_name_ = FetchTwaPackageName(render_frame_host);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* lacros_service = chromeos::LacrosService::Get();
  std::optional<webapps::AppId> app_id = GetWebAppId(render_frame_host);
  if (!lacros_service || !app_id.has_value()) {
    on_twa_package_name_ready_.Signal();
    return;
  }
  lacros_service->GetRemote<crosapi::mojom::WebAppService>()
      ->GetAssociatedAndroidPackage(
          *app_id,
          base::BindOnce(&TwaPackageHelper::OnGetAssociatedAndroidPackage,
                         weak_ptr_factory_.GetWeakPtr()));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

TwaPackageHelper::~TwaPackageHelper() = default;

void TwaPackageHelper::GetTwaPackageName(
    GetTwaPackageNameCallback callback) const {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  on_twa_package_name_ready_.Post(
      FROM_HERE,
      base::BindOnce(&TwaPackageHelper::GetCachedTwaPackageName,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
#else
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), twa_package_name_));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void TwaPackageHelper::OnGetAssociatedAndroidPackage(
    crosapi::mojom::WebAppAndroidPackagePtr package) {
  if (package) {
    twa_package_name_ = package->package_name;
  }

  on_twa_package_name_ready_.Signal();
}

void TwaPackageHelper::GetCachedTwaPackageName(
    GetTwaPackageNameCallback callback) const {
  DCHECK(on_twa_package_name_ready_.is_signaled());
  std::move(callback).Run(twa_package_name_);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace payments
