// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/digital_goods/util.h"

#include <optional>

#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/web_contents.h"

namespace apps {

std::string GetTwaPackageName(content::RenderFrameHost* render_frame_host) {
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return std::string();

  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!web_app::AppBrowserController::IsWebApp(browser)) {
    return std::string();
  }

  auto* profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());
  if (profile->IsIncognitoProfile()) {
    return std::string();
  }

  auto* apk_web_app_service = ash::ApkWebAppService::Get(profile);
  if (!apk_web_app_service) {
    return std::string();
  }

  std::optional<std::string> twa_package_name =
      apk_web_app_service->GetPackageNameForWebApp(
          render_frame_host->GetMainFrame()->GetLastCommittedURL());

  return twa_package_name.value_or(std::string());
}

std::string GetScope(content::RenderFrameHost* render_frame_host) {
  web_app::WebAppProvider* provider = web_app::WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext()));
  if (!provider) {
    return std::string();
  }

  const web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();
  // TODO(crbug.com/379827962): Evaluate call sites of FindBestAppWithUrlInScope
  // for correctness.
  std::optional<webapps::AppId> app_id = registrar.FindBestAppWithUrlInScope(
      render_frame_host->GetMainFrame()->GetLastCommittedURL(),
      {
          web_app::proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
          web_app::proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
      });
  if (!app_id) {
    return std::string();
  }

  GURL scope = registrar.GetAppScope(app_id.value());
  if (!scope.is_valid()) {
    return std::string();
  }

  return scope.spec();
}

}  // namespace apps
