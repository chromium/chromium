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
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace {

bool IsNtpUrl(const GURL& url) {
  const std::string origin = url.DeprecatedGetOriginAsURL().spec();
  return origin == chrome::kChromeUINewTabURL ||
         origin == chrome::kChromeUINewTabPageURL ||
         origin == chrome::kChromeUINewTabPageThirdPartyURL;
}

}  // namespace

BreadcrumbManagerTabHelper::BreadcrumbManagerTabHelper(
    content::WebContents* web_contents)
    : breadcrumbs::BreadcrumbManagerTabHelper(
          infobars::ContentInfoBarManager::FromWebContents(web_contents)),
      content::WebContentsObserver(web_contents),
      content::WebContentsUserData<BreadcrumbManagerTabHelper>(*web_contents) {}

BreadcrumbManagerTabHelper::~BreadcrumbManagerTabHelper() = default;

void BreadcrumbManagerTabHelper::PlatformLogEvent(const std::string& event) {
  BreadcrumbManagerKeyedServiceFactory::GetForBrowserContext(
      GetWebContents().GetBrowserContext())
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
                GetWebContents().GetContentsMimeType());
}

void BreadcrumbManagerTabHelper::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code) {
  LogPageLoaded(IsNtpUrl(validated_url), validated_url,
                /*page_load_success=*/false,
                GetWebContents().GetContentsMimeType());
}

void BreadcrumbManagerTabHelper::DidChangeVisibleSecurityState() {
  const auto visible_security_state =
      security_state::GetVisibleSecurityState(&GetWebContents());
  DCHECK(visible_security_state);

  // Note that mixed content is auto-upgraded to HTTPS in almost all cases on
  // desktop (the user has to specifically allow it on a per-site basis in
  // settings), so this is unlikely.
  const bool displayed_mixed_content =
      visible_security_state->displayed_mixed_content;

  security_state::SecurityLevel security_level =
      security_state::GetSecurityLevel(
          *visible_security_state,
          /*used_policy_installed_certificate=*/false);
  const bool security_style_authentication_broken =
      security_level == security_state::SecurityLevel::DANGEROUS;

  LogDidChangeVisibleSecurityState(displayed_mixed_content,
                                   security_style_authentication_broken);
}

void BreadcrumbManagerTabHelper::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  LogRenderProcessGone();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(BreadcrumbManagerTabHelper);
