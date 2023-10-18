// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/tpcd/metadata/devtools_observer.h"

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"

namespace tpcd::metadata {

TpcdMetadataDevtoolsObserver::TpcdMetadataDevtoolsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<TpcdMetadataDevtoolsObserver>(
          *web_contents) {
  cookie_settings_ = CookieSettingsFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
}

TpcdMetadataDevtoolsObserver::~TpcdMetadataDevtoolsObserver() = default;

void TpcdMetadataDevtoolsObserver::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  OnCookiesAccessedImpl(details.url, details.first_party_url);
}

void TpcdMetadataDevtoolsObserver::OnCookiesAccessed(
    content::NavigationHandle* navigation_handle,
    const content::CookieAccessDetails& details) {
  OnCookiesAccessedImpl(details.url, details.first_party_url);
}

void TpcdMetadataDevtoolsObserver::OnCookiesAccessedImpl(
    const GURL& url,
    const GURL& first_party_url) {
  if (cookie_settings_->IsAllowedByTpcdMetadataGrant(url, first_party_url)) {
    EmitMetadataGrantDevtoolsIssue(url);
  }
}

void TpcdMetadataDevtoolsObserver::EmitMetadataGrantDevtoolsIssue(
    const GURL& url) {
  auto details = blink::mojom::InspectorIssueDetails::New();

  auto metadata_issue_details =
      blink::mojom::CookieDeprecationMetadataIssueDetails::New();
  metadata_issue_details->allowed_sites.push_back(url.host());
  details->cookie_deprecation_metadata_issue_details =
      std::move(metadata_issue_details);

  web_contents()->GetPrimaryMainFrame()->ReportInspectorIssue(
      blink::mojom::InspectorIssueInfo::New(
          blink::mojom::InspectorIssueCode::kCookieDeprecationMetadataIssue,
          std::move(details)));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TpcdMetadataDevtoolsObserver);

}  // namespace tpcd::metadata
