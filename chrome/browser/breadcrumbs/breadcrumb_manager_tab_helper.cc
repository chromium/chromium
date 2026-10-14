// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/breadcrumbs/breadcrumb_manager_tab_helper.h"

#include "chrome/browser/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

bool IsNtpUrl(const GURL& url) {
  const GURL origin = url.DeprecatedGetOriginAsURL();
  return origin == chrome::ChromeUINewTabURLAsGURL() ||
         origin == chrome::ChromeUINewTabPageURLAsGURL() ||
         origin.spec() == chrome::kChromeUINewTabPageThirdPartyURL;
}

}  // namespace

DEFINE_USER_DATA(BreadcrumbManagerTabHelper);

BreadcrumbManagerTabHelper::BreadcrumbManagerTabHelper(
    tabs::TabInterface& tab,
    content::WebContents* web_contents,
    std::optional<int> previous_unique_id)
    : breadcrumbs::BreadcrumbManagerTabHelper(
          infobars::ContentInfoBarManager::FromWebContents(web_contents),
          breadcrumbs::BreadcrumbManagerTabHelper::ReserveUniqueId()),
      content::WebContentsObserver(web_contents),
      previous_unique_id_(previous_unique_id),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {}

BreadcrumbManagerTabHelper::~BreadcrumbManagerTabHelper() = default;

// static
BreadcrumbManagerTabHelper* BreadcrumbManagerTabHelper::From(
    tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

// static
const BreadcrumbManagerTabHelper* BreadcrumbManagerTabHelper::From(
    const tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

void BreadcrumbManagerTabHelper::PlatformLogEvent(const std::string& event) {
  // web_contents() securely returns nullptr if the WebContents is tearing down.
  // Events emitted by other UserData objects during destruction are ignored.
  if (!web_contents()) {
    return;
  }

  BreadcrumbManagerKeyedServiceFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext())
      ->AddEvent(event);
}

void BreadcrumbManagerTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  LogDidStartNavigation(navigation_handle->GetNavigationId(),
                        navigation_handle->GetURL(),
                        IsNtpUrl(navigation_handle->GetURL()),
                        navigation_handle->IsRendererInitiated(),
                        navigation_handle->HasUserGesture(),
                        navigation_handle->GetPageTransition());
}

void BreadcrumbManagerTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  LogDidFinishNavigation(navigation_handle->GetNavigationId(),
                         navigation_handle->IsDownload(),
                         navigation_handle->GetNetErrorCode());
}

void BreadcrumbManagerTabHelper::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  LogPageLoaded(IsNtpUrl(validated_url), validated_url,
                /*page_load_success=*/true,
                web_contents()->GetContentsMimeType());
}

void BreadcrumbManagerTabHelper::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code) {
  LogPageLoaded(IsNtpUrl(validated_url), validated_url,
                /*page_load_success=*/false,
                web_contents()->GetContentsMimeType());
}

void BreadcrumbManagerTabHelper::DidChangeVisibleSecurityState() {
  const auto visible_security_state =
      security_state::GetVisibleSecurityState(web_contents());
  CHECK(visible_security_state, base::NotFatalUntil::M161);

  // Note that mixed content is auto-upgraded to HTTPS in almost all cases on
  // desktop (the user has to specifically allow it on a per-site basis in
  // settings), so this is unlikely.
  const bool displayed_mixed_content =
      visible_security_state->displayed_mixed_content;

  security_state::SecurityLevel security_level =
      security_state::GetSecurityLevel(*visible_security_state);
  const bool security_style_authentication_broken =
      security_level == security_state::SecurityLevel::DANGEROUS;

  LogDidChangeVisibleSecurityState(displayed_mixed_content,
                                   security_style_authentication_broken);
}

void BreadcrumbManagerTabHelper::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  LogRenderProcessGone();
}
