// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/tpcd/metadata/devtools_observer.h"

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/metadata/manager_factory.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "components/tpcd/metadata/browser/manager.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/cookies/site_for_cookies.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"

namespace tpcd::metadata {

TpcdMetadataDevtoolsObserver::TpcdMetadataDevtoolsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<TpcdMetadataDevtoolsObserver>(
          *web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  cookie_settings_ = CookieSettingsFactory::GetForProfile(profile);
  tpcd_metadata_manager_ =
      tpcd::metadata::ManagerFactory::GetForProfile(profile);
}

TpcdMetadataDevtoolsObserver::~TpcdMetadataDevtoolsObserver() = default;

void TpcdMetadataDevtoolsObserver::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  OnCookiesAccessedImpl(details);
}

void TpcdMetadataDevtoolsObserver::OnCookiesAccessed(
    content::NavigationHandle* navigation_handle,
    const content::CookieAccessDetails& details) {
  OnCookiesAccessedImpl(details);
}

void TpcdMetadataDevtoolsObserver::OnCookiesAccessedImpl(
    const content::CookieAccessDetails& details) {
  if (content_settings::CookieSettingsBase::IsAnyTpcdMetadataAllowMechanism(
          cookie_settings_->GetThirdPartyCookieAllowMechanism(
              details.url,
              net::SiteForCookies::FromUrl(details.first_party_url),
              details.first_party_url, details.cookie_setting_overrides))) {
    EmitMetadataGrantDevtoolsIssue(details.url, details.first_party_url,
                                   details.type);
  }
}

void TpcdMetadataDevtoolsObserver::EmitMetadataGrantDevtoolsIssue(
    const GURL& third_party_url,
    const GURL& first_party_url,
    const content::CookieAccessDetails::Type cookie_access_type) {
  auto details = blink::mojom::InspectorIssueDetails::New();
  auto metadata_issue_details =
      blink::mojom::CookieDeprecationMetadataIssueDetails::New();

  metadata_issue_details->allowed_sites.push_back(third_party_url.host());
  metadata_issue_details->operation =
      cookie_access_type == content::CookieAccessDetails::Type::kRead
          ? blink::mojom::CookieOperation::kReadCookie
          : blink::mojom::CookieOperation::kSetCookie;

  if (tpcd_metadata_manager_) {
    content_settings::SettingInfo out_info;
    bool allowed = tpcd_metadata_manager_->IsAllowed(
        third_party_url, first_party_url, &out_info);
    if (allowed) {
      metadata_issue_details->opt_out_percentage =
          out_info.metadata.tpcd_metadata_elected_dtrp();
      metadata_issue_details->is_opt_out_top_level =
          out_info.metadata.tpcd_metadata_rule_source() ==
              TpcdMetadataRuleSource::SOURCE_1P_DT ||
          out_info.primary_pattern == ContentSettingsPattern::Wildcard();
    }
  }

  details->cookie_deprecation_metadata_issue_details =
      std::move(metadata_issue_details);
  web_contents()->GetPrimaryMainFrame()->ReportInspectorIssue(
      blink::mojom::InspectorIssueInfo::New(
          blink::mojom::InspectorIssueCode::kCookieDeprecationMetadataIssue,
          std::move(details)));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TpcdMetadataDevtoolsObserver);

}  // namespace tpcd::metadata
