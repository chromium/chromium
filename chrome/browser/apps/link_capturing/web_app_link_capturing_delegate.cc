// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/web_app_link_capturing_delegate.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/memory/values_equivalent.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/navigation_handle.h"

namespace web_app {
namespace {
void LaunchAppAndMaybeTriggerIPH(base::WeakPtr<Profile> profile,
                                 const webapps::AppId& app_id,
                                 const GURL& url,
                                 base::OnceClosure callback) {
  if (!profile) {
    std::move(callback).Run();
    return;
  }
  WebAppProvider* provider = WebAppProvider::GetForWebApps(profile.get());
  provider->scheduler().LaunchApp(
      app_id, url,
      base::BindOnce(
          [](const webapps::AppId& app_id, base::WeakPtr<Profile> profile,
             base::OnceClosure callback, base::WeakPtr<Browser> browser,
             base::WeakPtr<content::WebContents> web_contents,
             apps::LaunchContainer container) {
            if (!profile) {
              std::move(callback).Run();
              return;
            }

            if (web_contents) {
              WebAppProvider* provider =
                  WebAppProvider::GetForWebApps(profile.get());
              provider->ui_manager()
                  .MaybeShowIPHPromoForAppsLaunchedViaLinkCapturing(
                      /*browser=*/nullptr, profile.get(), app_id);
            }

            std::move(callback).Run();
          },
          app_id, profile, std::move(callback)));
}
}  // namespace

WebAppLinkCapturingDelegate::WebAppLinkCapturingDelegate() = default;
WebAppLinkCapturingDelegate::~WebAppLinkCapturingDelegate() = default;

bool WebAppLinkCapturingDelegate::ShouldCancelThrottleCreation(
    content::NavigationHandle* handle) {
  Profile* profile = Profile::FromBrowserContext(
      handle->GetWebContents()->GetBrowserContext());
  return !web_app::AreWebAppsUserInstallable(profile);
}

std::optional<apps::LinkCapturingNavigationThrottle::LaunchCallback>
WebAppLinkCapturingDelegate::CreateLinkCaptureLaunchClosure(
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& url,
    bool is_navigation_from_link) {
  if (!is_navigation_from_link) {
    return std::nullopt;
  }

  WebAppProvider* provider = WebAppProvider::GetForWebApps(profile);

  // This operation must be synchronous, so unfortunately we must use unsafe
  // access to the registrar.
  WebAppRegistrar& registrar = provider->registrar_unsafe();
  std::optional<webapps::AppId> possible_app_id =
      registrar.FindAppThatCapturesLinksInScope(url);

  if (!possible_app_id) {
    return std::nullopt;
  }

  webapps::AppId app_id = possible_app_id.value();
  // Don't capture links for apps that open in a tab.
  if (registrar.GetAppEffectiveDisplayMode(app_id) == DisplayMode::kBrowser) {
    return std::nullopt;
  }

  // Don't capture if already inside the target app scope.
  if (base::ValuesEquivalent(web_app::WebAppTabHelper::GetAppId(web_contents),
                             &app_id)) {
    return std::nullopt;
  }

  // Don't capture if already inside a window for the target app. If the
  // previous early return didn't trigger, this means we are in an app window
  // but out of scope of the original app, and navigating will put us back in
  // scope.
  if (base::ValuesEquivalent(
          provider->ui_manager().GetAppIdForWindow(web_contents), &app_id)) {
    return std::nullopt;
  }
  // Note: The launch can occur after this object is destroyed, so bind to a
  // static function.
  // TODO(b/297256243): Investigate possible reparenting instead of relaunching.
  return base::BindOnce(&LaunchAppAndMaybeTriggerIPH, profile->GetWeakPtr(),
                        app_id, url);
}

}  // namespace web_app
