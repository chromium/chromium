// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_SUPPORT_TRIAL_TEST_UTILS_H_
#define CHROME_BROWSER_TPCD_SUPPORT_TRIAL_TEST_UTILS_H_

#include "base/run_loop.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace tpcd::trial {

using CookieOperation = network::mojom::CookieAccessDetails::Type;

inline constexpr char kTestTokenPublicKey[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=,fMS4mpO6buLQ/QMd+zJmxzty/"
    "VQ6B1EUZqoCU04zoRU=";

inline constexpr char kTrialEnabledDomain[] = "example.test";
inline constexpr char kTrialEnabledSubdomain[] = "sub.example.test";
inline constexpr char kTrialEnabledIframePath[] = "origin-trial-iframe";
inline constexpr char kEmbeddedScriptPagePath[] =
    "tpcd/page_with_cross_site_tpcd_support_ot.html";
inline constexpr char kSubdomainMatchingEmbeddedScriptPagePath[] =
    "tpcd/page_with_cross_site_tpcd_support_ot_with_subdomain_matching.html";
// Origin Trials token for `kTrialEnabledSite` generated with:
// tools/origin_trials/generate_token.py  https://example.test Tpcd
// --expire-days 5000
inline constexpr char kTrialToken[] =
    "A1F5vUG256mdaDWxcpAddjWWg7LdOPuoEBswgFVy8b3j0ejT56eJ+e+"
    "IBocST6j2C8nYcnDm6gkd5O7M3FMo4AIAAABPeyJvcmlnaW4iOiAiaHR0cHM6Ly"
    "9leGFtcGxlLnRlc3Q6NDQzIiwgImZlYXR1cmUiOiAiVHBjZCIsICJleHBpcnkiO"
    "iAyMTI0MzA4MDY1fQ==";

// Origin Trials token for `kTrialEnabledSite` (and all its subdomains)
// generated with:
// tools/origin_trials/generate_token.py https://example.test Tpcd
// --is-subdomain --expire-days 5000
inline constexpr char kTrialSubdomainMatchingToken[] =
    "AwvUTouERi5ZSbMQGkQhzRCxh3hWd4mu1/"
    "d8CPaQGC3LGmelPVjpqV8VPvKHXNB6ES337b3xvLRsQ6Z/"
    "5TAjdQAAAABkeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLnRlc3Q6NDQzIiwgImZlYXR1cm"
    "UiOiAiVHBjZCIsICJleHBpcnkiOiAyMTMwODYwOTA1LCAiaXNTdWJkb21haW4iOiB0cnVlfQ="
    "=";

// Origin Trials token for `kTrialEnabledSiteSubdomain` (and all its subdomains)
// generated with:
// tools/origin_trials/generate_token.py https://sub.example.test Tpcd
// --is-subdomain --expire-days 5000
inline constexpr char kSubdomainTrialSubdomainMatchingToken[] =
    "A1XUCMiQfJGkSpeUIg7HmIpY9YtNoANQivDQYA8DLujoJhNovnyi0Fu2huOKeooMwHvfPecmA/"
    "8uJbrgH28T6A8AAABoeyJvcmlnaW4iOiAiaHR0cHM6Ly9zdWIuZXhhbXBsZS50ZXN0OjQ0MyIs"
    "ICJmZWF0dXJlIjogIlRwY2QiLCAiZXhwaXJ5IjogMjEzMzk2NzQwOCwgImlzU3ViZG9tYWluIj"
    "ogdHJ1ZX0=";

// Helper function for performing client side cookie access via JS.
void AccessCookieViaJsIn(content::WebContents* web_contents,
                         content::RenderFrameHost* frame);

class ContentSettingChangeObserver : public content_settings::Observer {
 public:
  explicit ContentSettingChangeObserver(
      content::BrowserContext* browser_context,
      const GURL request_url,
      const GURL partition_url,
      ContentSettingsType setting_type);

  ~ContentSettingChangeObserver() override;

  void Wait();

 private:
  // content_settings::Observer overrides:
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  raw_ptr<content::BrowserContext> browser_context_;
  base::RunLoop run_loop_;
  GURL request_url_;
  GURL partition_url_;
  ContentSettingsType setting_type_;
};

class URLCookieAccessObserver : public content::WebContentsObserver {
 public:
  URLCookieAccessObserver(content::WebContents* web_contents,
                          const GURL& url,
                          CookieOperation access_type);

  void Wait();

 private:
  // WebContentsObserver overrides
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override;
  void OnCookiesAccessed(content::NavigationHandle* navigation_handle,
                         const content::CookieAccessDetails& details) override;

  GURL url_;
  CookieOperation access_type_;
  base::RunLoop run_loop_;
};

class FrameCookieAccessObserver : public content::WebContentsObserver {
 public:
  explicit FrameCookieAccessObserver(
      content::WebContents* web_contents,
      content::RenderFrameHost* render_frame_host,
      CookieOperation access_type);

  // Wait until the frame accesses cookies.
  void Wait();

  // WebContentsObserver override
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override;

 private:
  const raw_ptr<content::RenderFrameHost> render_frame_host_;
  CookieOperation access_type_;
  base::RunLoop run_loop_;
};

}  // namespace tpcd::trial

#endif  // CHROME_BROWSER_TPCD_SUPPORT_TRIAL_TEST_UTILS_H_
