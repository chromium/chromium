// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/android/cross_device_signin_flow_navigation_throttle.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/android/signin_bridge.h"
#include "chrome/browser/signin/android/signin_bridge_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/signin_deep_link_metrics.h"
#include "components/signin/public/base/signin_deep_link_parser.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"

CrossDeviceSigninFlowNavigationThrottle::
    CrossDeviceSigninFlowNavigationThrottle(
        content::NavigationThrottleRegistry& registry,
        SigninBridge* signin_bridge,
        signin::IdentityManager* identity_manager,
        signin::SigninDeepLinkParser deep_link_parser)
    : content::NavigationThrottle(registry),
      signin_bridge_(signin_bridge),
      identity_manager_(identity_manager),
      deep_link_parser_(std::move(deep_link_parser)) {}

content::NavigationThrottle::ThrottleCheckResult
CrossDeviceSigninFlowNavigationThrottle::WillStartRequest() {
  const GURL& url = navigation_handle()->GetURL();
  const auto payload = deep_link_parser_.Parse(url);
  if (payload.has_value() && payload->HasAllRequiredFields()) {
    signin_metrics::RecordUrlDetected(
        payload->entry_point_id_raw_value_for_metrics.value());
    signin_metrics::RecordInitialAccountsNumber(
        payload->entry_point_id.value(),
        identity_manager_->GetAccountsWithRefreshTokens().size());
    content::WebContents* web_contents = navigation_handle()->GetWebContents();
    if (!web_contents) {
      return content::NavigationThrottle::PROCEED;
    }
    ui::WindowAndroid* window = web_contents->GetTopLevelNativeWindow();
    if (!window) {
      return content::NavigationThrottle::PROCEED;
    }
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    if (!profile || !profile->IsRegularProfile()) {
      return content::NavigationThrottle::PROCEED;
    }
    signin_bridge_->StartSigninDeepLinkFlow(window, profile, payload.value());
    ClosePageIfNeeded();
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
CrossDeviceSigninFlowNavigationThrottle::WillRedirectRequest() {
  return WillStartRequest();
}

const char* CrossDeviceSigninFlowNavigationThrottle::GetNameForLogging() {
  return "CrossDeviceSigninFlowNavigationThrottle";
}

CrossDeviceSigninFlowNavigationThrottle::
    ~CrossDeviceSigninFlowNavigationThrottle() = default;

// When the cross-device deep link is opened via intent, Chrome starts a new
// empty tab to handle the intent. We need to close the empty tab to avoid the
// user seeing a blank page. Tab is not closed if it was not empty, meaning user
// navigated to the deep link by entering the URL in the address bar or clicking
// a link in the page.
void CrossDeviceSigninFlowNavigationThrottle::ClosePageIfNeeded() {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (web_contents && navigation_handle()->IsInPrimaryMainFrame() &&
      web_contents->GetController().IsInitialBlankNavigation()) {
    // ClosePage() must be executed asynchronously. Calling it synchronously
    // could lead to immediate WebContents destruction, violating
    // NavigationThrottle requirements and causing a use-after-free crash when
    // returning CANCEL_AND_IGNORE.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<content::WebContents> weak_web_contents) {
              if (weak_web_contents && weak_web_contents->GetController()
                                           .IsInitialBlankNavigation()) {
                weak_web_contents->ClosePage();
              }
            },
            web_contents->GetWeakPtr()));
  }
}

// static
void CrossDeviceSigninFlowNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  auto parser =
      signin::SigninDeepLinkParser::CreateForCrossDeviceSigninIfEnabled();
  if (!parser.has_value()) {
    return;
  }

  auto* web_contents = registry.GetNavigationHandle().GetWebContents();
  if (!web_contents) {
    return;
  }

  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile) {
    return;
  }

  auto* signin_bridge = SigninBridgeFactory::GetForProfile(profile);
  if (!signin_bridge) {
    return;
  }

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return;
  }

  registry.AddThrottle(
      base::WrapUnique(new CrossDeviceSigninFlowNavigationThrottle(
          registry, signin_bridge, identity_manager,
          std::move(parser.value()))));
}
