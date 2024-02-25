// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/support/trial_test_utils.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/dips/dips_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"

using content::CookieAccessDetails;
using content::NavigationHandle;
using content::RenderFrameHost;
using content::WebContents;

namespace tpcd::trial {

void AccessCookieViaJsIn(content::WebContents* web_contents,
                         content::RenderFrameHost* frame) {
  FrameCookieAccessObserver observer(web_contents, frame,
                                     CookieOperation::kChange);
  ASSERT_TRUE(content::ExecJs(
      frame, "document.cookie = 'foo=bar;SameSite=None;Secure';",
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  observer.Wait();
}

ContentSettingChangeObserver::ContentSettingChangeObserver(
    content::BrowserContext* browser_context,
    const GURL request_url,
    const GURL partition_url,
    ContentSettingsType setting_type)
    : browser_context_(browser_context),
      request_url_(request_url),
      partition_url_(partition_url),
      setting_type_(setting_type) {
  HostContentSettingsMapFactory::GetForProfile(browser_context_)
      ->AddObserver(this);
}

ContentSettingChangeObserver::~ContentSettingChangeObserver() {
  HostContentSettingsMapFactory::GetForProfile(browser_context_)
      ->RemoveObserver(this);
}
void ContentSettingChangeObserver::Wait() {
  run_loop_.Run();
}

void ContentSettingChangeObserver::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if (content_type_set.Contains(setting_type_) &&
      primary_pattern.Matches(request_url_) &&
      secondary_pattern.Matches(partition_url_)) {
    run_loop_.Quit();
  }
}

URLCookieAccessObserver::URLCookieAccessObserver(WebContents* web_contents,
                                                 const GURL& url,
                                                 CookieOperation access_type)
    : WebContentsObserver(web_contents), url_(url), access_type_(access_type) {}

void URLCookieAccessObserver::Wait() {
  run_loop_.Run();
}

void URLCookieAccessObserver::OnCookiesAccessed(
    RenderFrameHost* render_frame_host,
    const CookieAccessDetails& details) {
  if (!IsInPrimaryPage(render_frame_host)) {
    return;
  }

  if (!details.blocked_by_policy && details.type == access_type_ &&
      details.url == url_) {
    run_loop_.Quit();
  }
}

void URLCookieAccessObserver::OnCookiesAccessed(
    NavigationHandle* navigation_handle,
    const CookieAccessDetails& details) {
  if (!IsInPrimaryPage(navigation_handle)) {
    return;
  }

  if (!details.blocked_by_policy && details.type == access_type_ &&
      details.url == url_) {
    run_loop_.Quit();
  }
}

FrameCookieAccessObserver::FrameCookieAccessObserver(
    WebContents* web_contents,
    RenderFrameHost* render_frame_host,
    CookieOperation access_type)
    : WebContentsObserver(web_contents),
      render_frame_host_(render_frame_host),
      access_type_(access_type) {}

void FrameCookieAccessObserver::Wait() {
  run_loop_.Run();
}

void FrameCookieAccessObserver::OnCookiesAccessed(
    RenderFrameHost* render_frame_host,
    const CookieAccessDetails& details) {
  if (!details.blocked_by_policy && details.type == access_type_ &&
      render_frame_host_ == render_frame_host) {
    run_loop_.Quit();
  }
}

}  // namespace tpcd::trial
