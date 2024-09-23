// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/preview/preview_navigation_throttle.h"

#include <string_view>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/error_page/common/error.h"
#include "components/error_page/common/localized_error.h"
#include "components/grit/components_resources.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/preview_cancel_reason.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

// static
std::unique_ptr<PreviewNavigationThrottle>
PreviewNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  auto* web_contents = content::WebContents::FromFrameTreeNodeId(
      navigation_handle->GetFrameTreeNodeId());
  CHECK(web_contents);
  if (web_contents->IsInPreviewMode()) {
    return base::WrapUnique(new PreviewNavigationThrottle(navigation_handle));
  }

  return nullptr;
}

PreviewNavigationThrottle::PreviewNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

PreviewNavigationThrottle::~PreviewNavigationThrottle() = default;

const char* PreviewNavigationThrottle::GetNameForLogging() {
  return "PreviewNavigationThrottle";
}

std::string MakeErrorPage(content::NavigationHandle& navigation_handle,
                          error_page::LinkPreviewErrorCode error_code) {
  const auto error = error_page::Error::LinkPreviewError(
      navigation_handle.GetURL(), error_code);
  base::Value::Dict error_page_params;
  error_page::LocalizedError::PageState page_state =
      error_page::LocalizedError::GetPageState(
          error.reason(), error.domain(), error.url(),
          navigation_handle.IsPost(),
          /*is_secure_dns_network_error=*/false,
          /*stale_copy_in_cache=*/false,
          /*can_show_network_diagnostics_dialog=*/false,
          /*is_incognito=*/
          Profile::FromBrowserContext(
              navigation_handle.GetWebContents()->GetBrowserContext())
              ->IsIncognitoProfile(),
          /*offline_content_feature_enabled=*/false,
          /*auto_fetch_feature_enabled=*/false,
          /*is_kiosk_mode=*/
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kForceAppMode),
          /*locale=*/g_browser_process->GetApplicationLocale(),
          /*is_blocked_by_extension=*/false, &error_page_params);

  std::string extracted_string =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_NET_ERROR_HTML);
  std::string_view template_html(extracted_string.data(),
                                 extracted_string.size());
  CHECK(!template_html.empty()) << "unable to load template.";
  return webui::GetLocalizedHtml(template_html, page_state.strings);
}

content::NavigationThrottle::ThrottleCheckResult Cancel(
    content::NavigationHandle& navigation_handle,
    content::PreviewCancelReason reason,
    error_page::LinkPreviewErrorCode error_code) {
  auto* web_contents = content::WebContents::FromFrameTreeNodeId(
      navigation_handle.GetFrameTreeNodeId());
  if (web_contents->GetDelegate()) {
    web_contents->GetDelegate()->CancelPreview(std::move(reason));
  }

  return content::NavigationThrottle::ThrottleCheckResult(
      content::NavigationThrottle::CANCEL, net::ERR_BLOCKED_BY_CLIENT,
      MakeErrorPage(navigation_handle, error_code));
}

content::NavigationThrottle::ThrottleCheckResult
PreviewNavigationThrottle::WillStartRequest() {
  return WillStartRequestOrRedirect();
}

content::NavigationThrottle::ThrottleCheckResult
PreviewNavigationThrottle::WillRedirectRequest() {
  return WillStartRequestOrRedirect();
}

content::NavigationThrottle::ThrottleCheckResult
PreviewNavigationThrottle::WillStartRequestOrRedirect() {
  if (!navigation_handle()->GetURL().SchemeIs(url::kHttpsScheme)) {
    return Cancel(*navigation_handle(),
                  content::PreviewCancelReason::Build(
                      content::PreviewFinalStatus::kBlockedByNonHttps),
                  error_page::LinkPreviewErrorCode::kNonHttpsForbidden);
  }

  return content::NavigationThrottle::ThrottleAction::PROCEED;
}
