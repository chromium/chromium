// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/breadcrumbs/breadcrumb_manager_tab_helper_desktop.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/security_state/content/content_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/security_style_explanations.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace {

bool IsNtpUrl(const GURL& url) {
  return url.DeprecatedGetOriginAsURL() == chrome::kChromeUINewTabURL;
}

}  // namespace

BreadcrumbManagerTabHelperDesktop::BreadcrumbManagerTabHelperDesktop(
    content::WebContents* web_contents)
    : breadcrumbs::BreadcrumbManagerTabHelper(
          infobars::ContentInfoBarManager::FromWebContents(web_contents)),
      content::WebContentsObserver(web_contents),
      web_contents_(web_contents) {}

BreadcrumbManagerTabHelperDesktop::~BreadcrumbManagerTabHelperDesktop() =
    default;

void BreadcrumbManagerTabHelperDesktop::PlatformLogEvent(
    const std::string& event) {
  LogEvent(event, BreadcrumbManagerKeyedServiceFactory::GetForBrowserContext(
                      web_contents_->GetBrowserContext()));
}

void BreadcrumbManagerTabHelperDesktop::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  LogDidStartNavigation(navigation_handle->GetNavigationId(),
                        navigation_handle->GetURL(),
                        IsNtpUrl(navigation_handle->GetURL()),
                        navigation_handle->IsRendererInitiated(),
                        navigation_handle->HasUserGesture(),
                        navigation_handle->GetPageTransition());
}

void BreadcrumbManagerTabHelperDesktop::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  LogDidFinishNavigation(navigation_handle->GetNavigationId(),
                         navigation_handle->IsDownload(),
                         navigation_handle->GetNetErrorCode());
}

void BreadcrumbManagerTabHelperDesktop::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  LogPageLoaded(IsNtpUrl(validated_url), validated_url,
                /*page_load_success=*/true,
                web_contents_->GetContentsMimeType());
}

void BreadcrumbManagerTabHelperDesktop::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code) {
  LogPageLoaded(IsNtpUrl(validated_url), validated_url,
                /*page_load_success=*/false,
                web_contents_->GetContentsMimeType());
}

void BreadcrumbManagerTabHelperDesktop::DidChangeVisibleSecurityState() {
  const auto visible_security_state =
      security_state::GetVisibleSecurityState(web_contents_);
  DCHECK(visible_security_state);
  const bool displayed_mixed_content =
      visible_security_state->displayed_mixed_content;

  content::SecurityStyleExplanations security_style_explanations;
  const blink::SecurityStyle security_style = GetSecurityStyle(
      security_state::GetSecurityLevel(
          *(visible_security_state.get()),
          /*used_policy_installed_certificate=*/false),
      *(visible_security_state.get()), &security_style_explanations);
  const bool security_style_authentication_broken =
      security_style == blink::SecurityStyle::kInsecureBroken;

  LogDidChangeVisibleSecurityState(displayed_mixed_content,
                                   security_style_authentication_broken);
}

void BreadcrumbManagerTabHelperDesktop::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  LogRenderProcessGone();
}

void BreadcrumbManagerTabHelperDesktop::WebContentsDestroyed() {
  web_contents_ = nullptr;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(BreadcrumbManagerTabHelperDesktop);
