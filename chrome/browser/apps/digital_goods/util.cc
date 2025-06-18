// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/digital_goods/util.h"

#include <optional>
#include <string>

#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/web_contents.h"

namespace apps {

std::string GetTwaPackageName(content::RenderFrameHost* render_frame_host) {
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return std::string();

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab) {
    // This can happen when the RenderFrameHost does not belong to a proper tab
    // such as a PWA or in this case a TWA. In these cases, there are no
    // guarantees that a TabInterface is associated with the WebContents. See
    // crbug.com/425155901 for more details.
    return std::string();
  }
  BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
  if (!browser || !browser->GetProfile()) {
    return std::string();
  }
  if (browser->GetProfile()->IsIncognitoProfile()) {
    return std::string();
  }

  auto* apk_web_app_service = ash::ApkWebAppService::Get(browser->GetProfile());
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
  std::optional<webapps::AppId> app_id = registrar.FindBestAppWithUrlInScope(
      render_frame_host->GetMainFrame()->GetLastCommittedURL(),
      web_app::WebAppFilter::InstalledInChrome());
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
