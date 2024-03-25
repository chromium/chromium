// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/http_error_observer/http_error_tab_helper.h"

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"

HttpErrorTabHelper::~HttpErrorTabHelper() = default;

// This override of the ResourceLoadComplete Method will be used to record
// instances of the ThirdPartyCookiesBreakageIndicator UKM
void HttpErrorTabHelper::ResourceLoadComplete(
    content::RenderFrameHost* render_frame_host,
    const content::GlobalRequestID& request_id,
    const blink::mojom::ResourceLoadInfo& resource_load_info) {
  // The response code will determine whether or not we record the metric. Only
  // record for https status codes indicating a server or client error.
  int response_code = resource_load_info.http_status_code;
  if (response_code < 400 || response_code >= 600) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());
  scoped_refptr<content_settings::CookieSettings> cs =
      CookieSettingsFactory::GetForProfile(profile);

  // For this metric, we define "cookies blocked in settings" based on the
  // global opt-in to third-party cookie blocking as well as no overriding
  // content setting on the top-level site.
  bool cookies_blocked_in_settings =
      cs && cs->ShouldBlockThirdPartyCookies() &&
      !cs->IsThirdPartyAccessAllowed(resource_load_info.final_url, nullptr);

  // Also measure if 3P cookies were actually blocked on the site.
  content_settings::PageSpecificContentSettings* pscs =
      content_settings::PageSpecificContentSettings::GetForFrame(
          render_frame_host);
  bool cookies_blocked =
      pscs && pscs->blocked_browsing_data_model()->size() > 0;

  ukm::builders::ThirdPartyCookies_BreakageIndicator_HTTPError(
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId())
      .SetTPCBlocked(cookies_blocked)
      .SetTPCBlockedInSettings(cookies_blocked_in_settings)
      .Record(ukm::UkmRecorder::Get());
}

HttpErrorTabHelper::HttpErrorTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<HttpErrorTabHelper>(*web_contents) {
  CHECK(web_contents);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HttpErrorTabHelper);
