// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/digital_goods/util.h"

#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

std::string GetTwaPackageName(content::RenderFrameHost* render_frame_host) {
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return "";

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!web_app::AppBrowserController::IsWebApp(browser)) {
    return "";
  }

  auto* profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());
  if (profile->IsIncognitoProfile()) {
    return "";
  }

  auto* apk_web_app_service = ash::ApkWebAppService::Get(profile);
  if (!apk_web_app_service) {
    return "";
  }

  absl::optional<std::string> twa_package_name =
      apk_web_app_service->GetPackageNameForWebApp(
          content::WebContents::FromRenderFrameHost(render_frame_host)
              ->GetLastCommittedURL());

  return twa_package_name.value_or("");
}

std::string GetScope(content::RenderFrameHost* render_frame_host) {
  web_app::WebAppRegistrar& registrar =
      web_app::WebAppProvider::GetForWebApps(
          Profile::FromBrowserContext(render_frame_host->GetBrowserContext()))
          ->registrar();
  absl::optional<web_app::AppId> app_id = registrar.FindAppWithUrlInScope(
      content::WebContents::FromRenderFrameHost(render_frame_host)
          ->GetLastCommittedURL());
  if (!app_id) {
    return "";
  }

  GURL scope = registrar.GetAppScope(app_id.value());
  if (!scope.is_valid()) {
    return "";
  }

  return scope.spec();
}

}  // namespace apps
