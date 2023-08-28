// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/web_app_link_capturing_delegate.h"

#include "base/functional/bind.h"
#include "base/memory/values_equivalent.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {
namespace {
void LaunchApp(base::WeakPtr<Profile> profile,
               const AppId& app_id,
               const GURL& url,
               base::OnceClosure callback) {
  if (!profile) {
    std::move(callback).Run();
    return;
  }
  WebAppProvider* provider = WebAppProvider::GetForWebApps(profile.get());
  provider->scheduler().LaunchUrlInApp(
      app_id, url,
      base::IgnoreArgs<base::WeakPtr<Browser>,
                       base::WeakPtr<content::WebContents>,
                       apps::LaunchContainer>(std::move(callback)));
}
}  // namespace

WebAppLinkCapturingDelegate::WebAppLinkCapturingDelegate() = default;
WebAppLinkCapturingDelegate::~WebAppLinkCapturingDelegate() = default;

bool WebAppLinkCapturingDelegate::ShouldCancelThrottleCreation(
    content::NavigationHandle* handle) {
  return false;
}

absl::optional<apps::LinkCapturingNavigationThrottle::LaunchCallback>
WebAppLinkCapturingDelegate::CreateLinkCaptureLaunchClosure(
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& url,
    bool is_navigation_from_link) {
  if (!is_navigation_from_link) {
    return absl::nullopt;
  }

  WebAppProvider* provider = WebAppProvider::GetForWebApps(profile);

  // This operation must be synchronous, so unfortunately we must use unsafe
  // access to the registrar.
  WebAppRegistrar& registrar = provider->registrar_unsafe();
  absl::optional<AppId> possible_app_id =
      registrar.FindAppThatCapturesLinksInScope(url);

  if (!possible_app_id) {
    return absl::nullopt;
  }

  AppId app_id = possible_app_id.value();
  // Don't capture links for apps that open in a tab.
  if (registrar.GetAppEffectiveDisplayMode(app_id) == DisplayMode::kBrowser) {
    return absl::nullopt;
  }

  // Don't capture if already inside the target app scope.
  if (base::ValuesEquivalent(web_app::WebAppTabHelper::GetAppId(web_contents),
                             &app_id)) {
    return absl::nullopt;
  }
  // Note: The launch can occur after this object is destroyed, so bind to a
  // static function.
  // TODO(b/297256243): Investigate possible reparenting instead of relaunching.
  return base::BindOnce(&LaunchApp, profile->GetWeakPtr(), app_id, url);
}

}  // namespace web_app
